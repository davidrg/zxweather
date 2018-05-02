#include "report.h"

#include <QStringList>
#include <QDir>
#include <QFile>
#include <QSqlRecord>
#include <QSqlField>
#include <QTextBrowser>
#include <QtDebug>
#include <QSqlError>

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

    QVariantMap queries = doc["queries"].toMap();
    foreach (QString key, queries.keys()) {
        query_t query;
        query.name = key;

        QVariantMap q = queries[key].toMap();

        query.db_query = QString(readFile(reportDir + q["db"].toString()));
        query.web_query = QString(readFile(reportDir + q["web"].toString()));
        this->queries.append(query);
    }

    outputTemplate = QString(readFile(name + QDir::separator() + doc["template"].toString()));
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


void Report::run(AbstractDataSource *dataSource,
                 QDateTime start, QDateTime end) {
    using namespace Mustache;

    dataSource->primeCache(start, end);

    bool isWeb = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE;
    QString stationCode = Settings::getInstance().stationCode();

    QVariantMap parameters;
    parameters["start"] = start;
    parameters["end"] = end;

    foreach (query_t q, queries) {
        QSqlQuery *query = dataSource->query();
        query->prepare(isWeb ? q.web_query : q.db_query);
        query->bindValue(":start", start);
        query->bindValue(":end", end);
        query->bindValue(":stationCode", stationCode);

        qDebug() << "Running query" << q.name;
        qDebug() << (isWeb ? q.web_query : q.db_query);
        qDebug() << "Start" << start;
        qDebug() << "End" << end;
        qDebug() << "Station" << stationCode;

        if (query->exec()) {
            QVariantList rows;

            if (query->first()) {
                do {
                    QSqlRecord record = query->record();
                    QVariantMap row;

                    for (int i = 0; i < record.count(); i++) {
                        QSqlField f = record.field(i);
                        row[f.name()] = f.value();
                        qDebug() << f.name() << f.value();
                    }

                    rows.append(row);
                } while (query->next());
            }
            parameters[q.name] = rows;
        } else {
            qDebug() << "Query failed" << query->lastError().databaseText() << query->lastError().driverText();
        }
        delete query;
    }

    qDebug() << parameters;

    Mustache::Renderer renderer;
    Mustache::QtVariantContext context(parameters);

    QString result = renderer.render(outputTemplate, &context);
    qDebug() << result;


    QTextBrowser *browser = new QTextBrowser();
    browser->setAttribute(Qt::WA_DeleteOnClose);
    browser->setHtml(result);
    browser->show();
}
