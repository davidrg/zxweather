#include "report.h"

#include <QStringList>
#include <QDir>
#include <QFile>
#include <QSqlRecord>
#include <QSqlField>
#include <QtDebug>
#include <QSqlError>

#include <QTabWidget>
#include <QTextBrowser>
#include <QTableView>
#include <QSqlQueryModel>
#include <QGridLayout>

#include "json/json.h"
#include "datasource/abstractdatasource.h"
#include "datasource/webdatasource.h"
#include "reporting/qt-mustache/mustache.h"
#include "settings.h"

QByteArray readFile(QString name) {
    QStringList files;
    files << "reports" + QString(QDir::separator()) + name;
    files << ":/reports/" + name;

    foreach (QString filename, files) {
        qDebug() << "Trying" << filename;

        QFile file(filename);

        if (file.exists()) {
            qDebug() << "found" << filename;
            if (file.open(QIODevice::ReadOnly)) {
                return file.readAll();
            }
        }
    }

    qDebug() << "Could not find one of" << files;
    return QByteArray();
}

QIcon readIcon(QString name, QIcon defaultIcon) {
    QByteArray data = readFile(name);
    if (!data.isNull()) {
        QPixmap pix;
        if (pix.loadFromData(data)) {
            return QIcon(pix);
        }
    }

    return defaultIcon;
}

QString readTextFile(QString name) {
    QByteArray data = readFile(name);
    if (data.isNull()) {
        return QString();
    }

    return QString(data);
}

Report::Report(QString name)
{
    using namespace QtJson;
    _isNull = true;
    this->_name = name;

    QString reportDir = name + QDir::separator() ;

    QByteArray report = readFile(reportDir + "report.json");

    if (report.isNull()) {
        return; // Couldn't find the report.
    }

    bool ok = true;
    QVariantMap doc = Json::parse(report, ok).toMap();
    if (!ok) {
        return;
    }

    _title = doc["title"].toString();
    _description = QString(readFile(reportDir + doc["description"].toString()));

    // Load the reports icon
    _icon = readIcon(reportDir + doc["icon"].toString(), QIcon(":/icons/report"));

    _tpType = TP_Timespan;
    if (doc.contains("time_picker")) {
        QString time_picker = doc["time_picker"].toString().toLower();
        if (time_picker == "timespan") {
            _tpType = TP_Timespan;
        } else if (time_picker == "datespan") {
            _tpType = TP_Datespan;
        } else if (time_picker == "date") {
            _tpType = TP_Day;
        } else if (time_picker == "month") {
            _tpType = TP_Month;
        } else if (time_picker == "year") {
            _tpType = TP_Year;
        } else if (time_picker == "none") {
            _tpType = TP_None;
        } else {
            qWarning() << "Report" << _name << "has invalid time picker type" << time_picker;
        }
    }

    // Load queries
    _web_ok = true;
    _db_ok = true;

    QVariantMap queries = doc["queries"].toMap();
    foreach (QString key, queries.keys()) {
        query_t query;
        query.name = key;

        QVariantMap q = queries[key].toMap();

        query.db_query = readTextFile(reportDir + q["db"].toString());
        query.web_query = readTextFile(reportDir + q["web"].toString());

        if (query.web_query.isNull()) {
            qDebug() << "No WebDataSource query supplied for" << query.name;
        }
        if (query.db_query.isNull()) {
            qDebug() << "No DatabaseDataSource query supplied for" << query.name;
        }

        _web_ok = _web_ok && !query.web_query.isNull();
        _db_ok = _db_ok && !query.db_query.isNull();

        this->queries.append(query);
    }

    output_type = OT_DISPLAY;
    if (doc.contains("output_type")) {
        QString t = doc["output_type"].toString().toLower();
        if (t == "show") {
            output_type = OT_DISPLAY;
        } else if (t == "save") {
            output_type = OT_SAVE;
        }
    }

    QVariantList outputs = doc["outputs"].toList();
    foreach (QVariant item, outputs) {
        QVariantMap o = item.toMap();
        QString key = o["name"].toString();

        output_t output;
        output.name = key;
        output.title = o["title"].toString();

        if (o.contains("icon")) {
            output.icon = readIcon(reportDir + o["icon"].toString(), QIcon());
        }

        QString format = o["format"].toString().toLower();
        if (format == "html") {
            output.format = OF_HTML;
        } else if (format == "text") {
            output.format = OF_TEXT;
        } else if (format == "table") {
            output.format = OF_TABLE;
        }

        if (output.format == OF_HTML || output.format == OF_TEXT) {
            if (!o.contains("template")) {
                qWarning() << "invalid output" << key
                           << "for report" << name
                           << "- no output_template specified for TEXT/HTML format output";
                continue;
            }
            output.output_template = readFile(reportDir + o["template"].toString());
            if (output.output_template.isNull()) {
                qWarning() << "invalid output" << key
                           << "for report" << name
                           << "- failed to load template for specified for TEXT/HTML format output";
                continue;
            }
        } else if (output.format == OF_TABLE) {
            if (!o.contains("query")) {
                qWarning() << "invalid output" << key
                           << "for report" << name
                           << "- no query specified for TABLE format output";
                continue;
            }
            output.query_name = o["query"].toString();
            if (!queries.keys().contains(output.query_name)) {
                qWarning() << "invalid output" << key
                           << "for report" << name
                           << "- no such query" << output.query_name
                           << "for TABLE format output";
                continue;
            }
        }

        if (o.contains("filename")) {
            output.filename = o["filename"].toString();
        }

        this->outputs.append(output);
    }

    _isNull = false;
}


QStringList findReportsIn(QString directory) {
    QDir dir(directory);

    QStringList result;

    foreach (QString d, dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden)) {
        qDebug() << d;

        QString reportFile = directory + QDir::separator()
                + d + QDir::separator() + QString("report.json");
        qDebug() << reportFile;

        if (QFile(reportFile).exists()) {
            result.append(d);
            qDebug() << "Found report" << d;
        }
    }

    return result;
}

QStringList Report::reports() {
     QStringList reports = findReportsIn(":/reports");

     foreach (QString report, findReportsIn("reports")) {
         if (!reports.contains(report)) {
             reports.append(report);
         }
     }
     return reports;
}

QList<Report> Report::loadReports() {
    QList<Report> reports;

    foreach (QString reportName, Report::reports()) {
        reports.append(Report(reportName));
    }

    return reports;
}

void Report::run(AbstractDataSource *dataSource, QDateTime start, QDateTime end) {
    dataSource->primeCache(start, end);
    QMap<QString, QVariant> parameters;
    parameters["start"] = start;
    parameters["end"] = end;
    run(dataSource, parameters);
}

void Report::run(AbstractDataSource* dataSource, QDate start, QDate end) {
    dataSource->primeCache(QDateTime(start, QTime(0,0,0)),
                           QDateTime(end, QTime(23,59,59,59)));
    QMap<QString, QVariant> parameters;
    parameters["start"] = start;
    parameters["end"] = end;
    run(dataSource, parameters);
}

void Report::run(AbstractDataSource* dataSource, QDate dayOrMonth, bool month) {
    if (month) {
        QDate end = dayOrMonth.addMonths(1).addDays(-1);
        dataSource->primeCache(QDateTime(dayOrMonth, QTime(0,0,0)),
                               QDateTime(end, QTime(23,59,59,59)));
    } else {
        dataSource->primeCache(QDateTime(dayOrMonth, QTime(0,0,0)),
                               QDateTime(dayOrMonth, QTime(23,59,59,59)));
    }
    QMap<QString, QVariant> parameters;
    parameters["date"] = dayOrMonth;
    run(dataSource, parameters);
}

void Report::run(AbstractDataSource* dataSource, int year) {

    dataSource->primeCache(QDateTime(QDate(year, 1, 1), QTime(0,0,0)),
                           QDateTime(QDate(year, 1, 1).addYears(1).addDays(-1),
                                     QTime(23,59,59,59)));

    QMap<QString, QVariant> parameters;
    parameters["year"] = year;
    run(dataSource, parameters);
}

void Report::run(AbstractDataSource* dataSource, QMap<QString, QVariant> parameters) {
    bool isWeb = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE;
    QString stationCode = Settings::getInstance().stationCode();

    parameters["stationCode"] = stationCode;

    QMap<QString, QSqlQuery> queryResults;

    foreach (query_t q, queries) {
        QSqlQuery query = dataSource->query();

        query.prepare(isWeb ? q.web_query : q.db_query);
        foreach (QString paramName, parameters.keys()) {
            query.bindValue(":" + paramName, parameters[paramName]);
            qDebug() << "Parameter" << paramName << "value" << parameters[paramName];
        }
        if (query.exec()) {
            queryResults[q.name] = QSqlQuery(query);
        } else {
            qDebug() << "Query failed" << query.lastError().databaseText() << query.lastError().driverText();
        }
    }

    if (output_type == OT_DISPLAY) {
        outputToUI(parameters, queryResults);
    } else if (output_type == OT_SAVE) {
        outputToDisk(parameters, queryResults);
    }
}

void Report::outputToUI(QMap<QString, QVariant> reportParameters,
                        QMap<QString, QSqlQuery> queries) {

    QWidget *window = new QWidget();
    window->setWindowTitle(_name);
    window->setWindowIcon(_icon);
    window->resize(800,600);

    QGridLayout *layout = new QGridLayout(window);

    QTabWidget *tabs = new QTabWidget(window);
    layout->addWidget(tabs, 0, 0);

    foreach (output_t output, outputs) {
        QWidget *tab = new QWidget();
        QGridLayout *tabLayout = new QGridLayout(tab);
        if (output.format == OF_HTML || output.format == OF_TEXT) {
            QTextBrowser *browser = new QTextBrowser();

            if (output.format == OF_HTML) {
                browser->setHtml(renderTemplatedReport(reportParameters, queries,
                                                       output.output_template));
            } else {
                browser->setText(renderTemplatedReport(reportParameters, queries,
                                                       output.output_template));
            }

            tabLayout->addWidget(browser, 0, 0);
        } else {
            QTableView *table = new QTableView();
            QSqlQueryModel *model = new QSqlQueryModel(table);
            model->setQuery(queries[output.query_name]);
            table->setModel(model);
            tabLayout->addWidget(table, 0, 0);
        }

        if (output.icon.isNull()) {
            tabs->addTab(tab, output.title);
        } else {
            tabs->addTab(tab, output.icon, output.title);
        }
    }

    window->show();
    window->setAttribute(Qt::WA_DeleteOnClose);
}

void Report::outputToDisk(QMap<QString, QVariant> reportParameters,
                          QMap<QString, QSqlQuery> queries) {

    // TODO
}

QString Report::renderTemplatedReport(QMap<QString, QVariant> reportParameters,
                                      QMap<QString, QSqlQuery> queries,
                                      QString outputTemplate) {
    using namespace Mustache;

    QVariantMap parameters;
    foreach (QString parameterName, reportParameters.keys()) {
        parameters[parameterName] = reportParameters[parameterName];
    }

    foreach (QString queryName, queries.keys()) {
        QVariantList rows;
        QSqlQuery query = queries[queryName];

        if (query.first()) {
            do {
                QSqlRecord record = query.record();
                QVariantMap row;

                for (int i = 0; i < record.count(); i++) {
                    QSqlField f = record.field(i);
                    row[f.name()] = f.value();
                    qDebug() << f.name() << f.value();
                }

                rows.append(row);
            } while (query.next());
        }
        parameters[queryName] = rows;
    }

    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(parameters);

    QString result = renderer.render(outputTemplate, &context);
    qDebug() << result;
    return result;
}
