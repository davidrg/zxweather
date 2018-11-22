#include "report.h"

#include <QStringList>
#include <QDir>
#include <QFile>
#include <QSqlRecord>
#include <QSqlField>
#include <QtDebug>
#include <QSqlError>
#include <QSqlQueryModel>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QMap>
#include <QDesktopServices>

#include "json/json.h"
#include "datasource/abstractdatasource.h"
#include "datasource/webdatasource.h"
#include "reporting/qt-mustache/mustache.h"
#include "reportdisplaywindow.h"
#include "settings.h"
#include "reportfinisher.h"

QByteArray readFile(QString name) {
    QStringList files;
    files << QDir::cleanPath("reports" + QString(QDir::separator()) + name);

    QStringList paths = Settings::getInstance().reportSearchPath();
// #if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
//    paths.append(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation) + "/reports/");
//    paths.append(QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation) + "/reports/");
//#else
//    paths.append(QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/reports/");

// #endif
    foreach (QString path, paths) {
        files << QDir::cleanPath(path + "/" + name);
    }

    files << QDir::cleanPath(":/reports/" + name);

    foreach (QString filename, files) {
        qDebug() << "Checking for" << filename;
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

    qDebug() << "========== Load report " << name << "==========";

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

    _debug = false;
    if (doc.contains("debug_mode")) {
        _debug = doc["debug_mode"].toBool();
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

    _defaultTimeSpan = FTS_None;
    if (doc.contains("default_time_span")) {
        QString ts = doc["default_time_span"].toString().toLower();
        if (ts == "none") {
            _defaultTimeSpan = FTS_None;
        } else if (ts == "today") {
            _defaultTimeSpan = FTS_Today;
        } else if (ts == "yesterday") {
            _defaultTimeSpan = FTS_Yesterday;
        } else if (ts == "last_24h") {
            _defaultTimeSpan = FTS_Last_24H;
        } else if (ts == "this_week") {
            _defaultTimeSpan = FTS_ThisWeek;
        } else if (ts == "last_week") {
            _defaultTimeSpan = FTS_LastWeek;
        } else if (ts == "last_7d") {
            _defaultTimeSpan = FTS_Last_7D;
        } else if (ts == "last_14d") {
            _defaultTimeSpan = FTS_Last_14D;
        } else if (ts == "this_month") {
            _defaultTimeSpan = FTS_ThisMonth;
        } else if (ts == "last_month") {
            _defaultTimeSpan = FTS_LastMonth;
        } else if (ts == "last_30d") {
            _defaultTimeSpan = FTS_Last_30D;
        } else if (ts == "this_year") {
            _defaultTimeSpan = FTS_ThisYear;
        } else if (ts == "last_year") {
            _defaultTimeSpan = FTS_LastYear;
        } else if (ts == "last_365d") {
            _defaultTimeSpan = FTS_Last_365D;
        } else if (ts == "all_time") {
            _defaultTimeSpan = FTS_AllTime;
        }
    }

    _primeCache = true;
    if (doc.contains("dont_prime_cache")) {
        _primeCache = !doc["dont_prime_cache"].toBool();
    }

    if (!doc.contains("supported_weather_stations")) {
        _weatherStations.append(WST_Generic);
    } else {
        QVariantList wsts = doc["supported_weather_stations"].toList();
        foreach (QVariant wst, wsts) {
            QString t = wst.toString();
            if (t == "generic") {
                _weatherStations.append(WST_Generic);
            } else if (t == "wh1080") {
                _weatherStations.append(WST_WH1080);
            } else if (t == "vantage_pro2") {
                _weatherStations.append(WST_VantagePro2);
            } else if (t == "vantage_pro2_plus") {
                _weatherStations.append(WST_VantagePro2Plus);
            }
        }
    }

    _custom_criteria = doc.contains("criteria_ui");
    if (_custom_criteria) {
        _ui = readFile(reportDir + doc["criteria_ui"].toString());
        _custom_criteria = !_ui.isNull();
    }

    // Load queries
    _web_ok = true;
    _db_ok = true;

    QVariantMap queries = doc["queries"].toMap();
    foreach (QString key, queries.keys()) {
        query_t query;
        query.name = key;

        QVariantMap q = queries[key].toMap();

        if (q.contains("db")) {
            QVariantMap queryDetails = q["db"].toMap();
            if (queryDetails.contains("file") && queryDetails.contains("parameters")) {
                query.db_query.query_text = readTextFile(
                            reportDir + queryDetails["file"].toString());
                query.db_query.parameters = QSet<QString>::fromList(
                            queryDetails["parameters"].toStringList());
            }
        }
        if (q.contains("web")) {
            QVariantMap queryDetails = q["web"].toMap();
            if (queryDetails.contains("file") && queryDetails.contains("parameters")) {
                query.web_query.query_text = readTextFile(
                            reportDir + queryDetails["file"].toString());
                query.web_query.parameters = QSet<QString>::fromList(
                            queryDetails["parameters"].toStringList());
            }
        }

        if (query.web_query.query_text.isNull()) {
            qDebug() << "No WebDataSource query supplied for" << query.name;
        }
        if (query.db_query.query_text.isNull()) {
            qDebug() << "No DatabaseDataSource query supplied for" << query.name;
        }

        _web_ok = _web_ok && !query.web_query.query_text.isNull();
        _db_ok = _db_ok && !query.db_query.query_text.isNull();

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

    if (doc.contains("output_window_size") && !this->_debug) {
        QVariantMap sz = doc["output_window_size"].toMap();
        if (sz.contains("height") && sz.contains("width")) {
            outputWindowSize.setHeight(sz.value("height").toInt());
            outputWindowSize.setWidth(sz.value("width").toInt());
        } else {
            outputWindowSize = QSize();
        }
    } else {
        outputWindowSize = QSize();
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

            if (o.contains("view_template")) {
                output.view_output_template = readFile(reportDir + o["view_template"].toString());
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

            if (o.contains("view_columns")) {
                QVariantMap m = o["view_columns"].toMap();
                foreach (QString key, m.keys()) {
                    output.viewColumns[key] = m[key].toString();
                }
            }
            if (o.contains("save_columns")) {
                QVariantMap m = o["save_columns"].toMap();
                foreach (QString key, m.keys()) {
                    output.saveColumns[key] = m[key].toString();
                }
            }
        }

        if (o.contains("filename")) {
            output.filename = o["filename"].toString();
        }

        this->outputs.append(output);
    }

    if (_debug) {
        // TODO: Gather together debug info from the report loading stage and store in member
        // variable for later output.

        this->outputs.clear();
        output_t summary;
        summary.name = "debug_summary";
        summary.title = "Debug Summary";
        summary.format = OF_HTML;
        summary.output_template = readFile("debug_summary.html");
        this->outputs.append(summary);

        foreach (query_t q, this->queries) {
            output_t o;
            o.name = q.name;
            o.title = q.name;
            o.format = OF_TABLE;
            o.query_name = q.name;
            this->outputs.append(o);
        }

        output_type = OT_DISPLAY;
    }

    _isNull = false;
    _finisher = NULL;
}

Report::~Report() {
    if (_finisher != NULL) {
        delete _finisher;
    }
}

bool Report::operator <(Report const& b)const {
    return _title < b.title();
}


QStringList findReportsIn(QString directory) {
    qDebug() << "Searching for reports in" << directory;
    QDir dir(directory);

    QStringList result;

    foreach (QString d, dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden)) {
        qDebug() << d;

        QString reportFile = QDir::cleanPath(directory + QDir::separator()
                + d + QDir::separator() + QString("report.json"));
        qDebug() << reportFile;

        QFile f(reportFile);

        if (QFile(reportFile).exists()) {
            result.append(d);
            qDebug() << "Found report" << d;
        }
    }

    return result;
}

QStringList Report::reports() {
     QStringList searchPaths = Settings::getInstance().reportSearchPath();
     searchPaths << "./";

//#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
//     searchPaths.append(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation) + "/reports");
//     searchPaths.append(QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation) + "/reports");
//#else
//     searchPaths.append(QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/reports");
//#endif

     searchPaths.removeDuplicates();

     QStringList reports = findReportsIn(":/reports");

     foreach (QString path, searchPaths) {
         foreach (QString report, findReportsIn(QDir::cleanPath(path))) {
             if (!reports.contains(report)) {
                 reports.append(report);
             }
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

ReportFinisher* Report::run(AbstractDataSource *dataSource, QDateTime start, QDateTime end, QVariantMap parameters) {
    parameters["start"] = start;
    parameters["end"] = end;
    parameters["start_t"] = start.toTime_t();
    parameters["end_t"] = end.toTime_t();

    _dataSource = dataSource;
    _parameters = parameters;

    if (_primeCache) {
        _finisher=(new ReportFinisher(this));
        dataSource->connect(dataSource, SIGNAL(cachingFinished()),
                            _finisher, SLOT(cachingFinished()));
        dataSource->primeCache(start, end);
        return _finisher;
    } else {
        executeReport();
    }
    return NULL;
}

ReportFinisher* Report::run(AbstractDataSource* dataSource, QDate start, QDate end, QVariantMap parameters) {
    parameters["start"] = start;
    parameters["end"] = end;
    parameters["start_t"] = QDateTime(start, QTime(0, 0, 0)).toTime_t();
    parameters["end_t"] = QDateTime(end, QTime(0, 0, 0)).toTime_t();

    _dataSource = dataSource;
    _parameters = parameters;

    if (_primeCache) {
        _finisher=(new ReportFinisher(this));
        dataSource->connect(dataSource, SIGNAL(cachingFinished()),
                            _finisher, SLOT(cachingFinished()));
        dataSource->primeCache(QDateTime(start, QTime(0,0,0)),
                               QDateTime(end, QTime(23,59,59,59)));
        return _finisher;
    } else {
        executeReport();
    }
    return NULL;
}

ReportFinisher* Report::run(AbstractDataSource* dataSource, QDate dayOrMonth, bool month, QVariantMap parameters) {
    parameters["atDate"] = dayOrMonth;

    QDateTime start = QDateTime(dayOrMonth, QTime(0, 0));
    if (month) {
        parameters["start"] = QDateTime(dayOrMonth, QTime(0,0,0));
        parameters["end"] = QDateTime(dayOrMonth.addMonths(1).addDays(-1), QTime(23,59,59));
        parameters["end_t"] = start.addMonths(1).addMSecs(-1).toTime_t();
    } else {
        parameters["start"] = start;
        parameters["end"] = start.addDays(1).addMSecs(-1);
        parameters["end_t"] = start.addDays(1).addMSecs(-1).toTime_t();
    }

    parameters["start_t"] = start.toTime_t();


    _dataSource = dataSource;
    _parameters = parameters;

    if (_primeCache) {
        _finisher=(new ReportFinisher(this));
        dataSource->connect(dataSource, SIGNAL(cachingFinished()),
                            _finisher, SLOT(cachingFinished()));
        if (month) {
            QDate end = dayOrMonth.addMonths(1).addDays(-1);
            dataSource->primeCache(QDateTime(dayOrMonth, QTime(0,0,0)),
                                   QDateTime(end, QTime(23,59,59,59)));
        } else {
            dataSource->primeCache(QDateTime(dayOrMonth, QTime(0,0,0)),
                                   QDateTime(dayOrMonth, QTime(23,59,59,59)));
        }
        return _finisher;
    } else {
        executeReport();
    }
    return NULL;
}

ReportFinisher* Report::run(AbstractDataSource* dataSource, int year, QVariantMap parameters) {
    parameters["year"] = year;

    QDateTime start = QDateTime(QDate(year, 1, 1), QTime(0, 0));
    parameters["start"] = start;
    parameters["end"] = start.addYears(1).addMSecs(-1);

    parameters["start_t"] = start.toTime_t();
    parameters["end_t"] = start.addYears(1).addMSecs(-1).toTime_t();

    _dataSource = dataSource;
    _parameters = parameters;

    if (_primeCache) {
        _finisher =(new ReportFinisher(this));
        dataSource->connect(dataSource, SIGNAL(cachingFinished()),
                            _finisher, SLOT(cachingFinished()));
        dataSource->primeCache(QDateTime(QDate(year, 1, 1), QTime(0,0,0)),
                               QDateTime(QDate(year, 1, 1).addYears(1).addDays(-1),
                                         QTime(23,59,59,59)));

        return _finisher;
    } else {
        executeReport();
    }
    return NULL;
}

void Report::executeReport() {
    run(_dataSource, _parameters);
    if (_finisher != NULL) {
        _finisher->finishReport();
    }
}

void Report::run(AbstractDataSource* dataSource, QMap<QString, QVariant> parameters) {
    bool isWeb = Settings::getInstance().sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE;
    QString stationCode = Settings::getInstance().stationCode();

    parameters["stationCode"] = stationCode;

    if (isWeb) {
        parameters["is_web_ds"] = true;
        QString url = Settings::getInstance().webInterfaceUrl();
        if (!url.endsWith("/")) {
            url.append("/");
        }
        url.append("data/" + stationCode + "/");
        parameters["stationCode"] = url;

    }

    QMap<QString, QSqlQuery> queryResults;

    QVariantList queryDebugInfo;

    foreach (query_t q, queries) {
        qDebug() << "==== Run query " << q.name << "====";
        QSqlQuery query = dataSource->query();

        QVariantMap debugInfo;
        bool success;

        query_variant_t variant = isWeb ? q.web_query : q.db_query;

        if (!_debug) {
            query.prepare(variant.query_text);
        } else {
            if (isWeb) {
                query.prepare("explain query plan " + variant.query_text);
            } else {
                // TODO explain support for postgres. This isn't supported with
                // prepared queries so we'd have to inject the parameters into the query
                // manually.
                query.prepare(variant.query_text);
            }

            QVariantMap bv = query.boundValues();
            QVariantList vals;
            foreach (QString key, bv.keys()) {
                QVariantMap m;
                m["key"] = key;
                vals.append(m);
            }
            debugInfo["parameters"] = vals;
            debugInfo["name"] = q.name;
        }

        foreach (QString paramName, parameters.keys()) {
            if (!variant.parameters.contains(paramName)) {
                continue; // Report doesn't need this parameter so exclude it
                // We do this because some database drivers don't like extra unused query
                // parameters (in Qt 5.4 QSQLite seems to report this as an error)
            }
            query.bindValue(":" + paramName, parameters[paramName]);
            qDebug() << "Parameter" << paramName << "value" << parameters[paramName];
        }
        if (query.exec()) {
            queryResults[q.name] = QSqlQuery(query);
            success = true;
        } else {
            qDebug() << "===============================";
            qDebug() << "Query failed";
            qDebug() << "db text:" << query.lastError().databaseText();
            qDebug() << "driver text:" << query.lastError().driverText();
            qDebug() << "--- query";
            qDebug() << query.executedQuery();
            qDebug() << "---- /query";
            success = false;
        }

        if (_debug) {
            debugInfo["success"] = success;
            debugInfo["query"] = query.executedQuery();
            QString driverText = query.lastError().driverText();
            QString dbText = query.lastError().databaseText();
            debugInfo["db_text"] = dbText.isEmpty() ? "none" : dbText;
            debugInfo["driver_text"] = driverText.isEmpty() ? "none" : driverText;

            QVariantMap bv = query.boundValues();
            QVariantList vals;
            foreach (QString key, bv.keys()) {
                QVariantMap m;
                m["key"] = key;
                m["value"] = bv[key].toString();
                vals.append(m);
            }
            debugInfo["bound_parameters"] = vals;

            queryDebugInfo.append(debugInfo);
        }
    }

    if (_debug) {
        parameters["queries"] = queryDebugInfo;
    }

    if (output_type == OT_DISPLAY) {
        station_info_t info = dataSource->getStationInfo();

        outputToUI(parameters, queryResults, info.hasSolarAndUV, info.isWireless);
    } else if (output_type == OT_SAVE) {
        outputToDisk(parameters, queryResults);
    }
}

QString Report::queryToCSV(QSqlQuery query, QMap<QString, QString> columnHeadings) {
    QString result;

    bool haveHeader = false;
    QString header;

    if (query.first()) {
        do {
            QSqlRecord record = query.record();
            QString row;

            for (int i = 0; i < record.count(); i++) {
                QSqlField f = record.field(i);
                QString fieldName = f.name();
                if (columnHeadings.contains(fieldName) && columnHeadings[fieldName].isNull()) {
                    continue;
                }

                if (!row.isEmpty()) {
                    row.append(",");
                }
                row.append(f.value().toString());

                if (!haveHeader) {
                    if (!header.isEmpty()) {
                        header.append(",");
                    }
                    if (columnHeadings.contains(fieldName)) {
                        header.append(columnHeadings[fieldName]);
                    } else {
                        header.append(fieldName);
                    }
                }
            }

            row.append("\n");
            result.append(row);
            haveHeader = true;
        } while (query.next());
    }

    result = header + "\n" + result;

    return result;
}

void Report::writeFile(report_output_file_t file) {
    QFile f(file.default_filename);

    if (f.open(QIODevice::WriteOnly)) {
        if (!file.data.isNull()) {
            f.write(file.data);
        } else if (file.query.isActive()) {
            f.write(Report::queryToCSV(file.query, file.columnHeadings).toUtf8());
        }

        f.close();
    }
}

bool isDirEmpty(QDir dir) {
#if (QT_VERSION >= QT_VERSION_CHECK(5,9,0))
    return dir.isEmpty();
#else
    return dir.entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).count() == 0;
#endif
}

QString getSaveDirectory(QWidget *parent) {
    // This will loop until either:
    // 1) The user selects an empty directory
    // 2) The user cancels
    // 3) The user selects a non-empty directory and chooses to continue anyway
    // 4) The user selects a non-empty directory and cancels
    while (true) {
        QString dir = QFileDialog::getExistingDirectory(parent, QObject::tr("Save report to"));
        if (dir.isEmpty()) {
            return QString(); // user canceled
        }

        if (isDirEmpty(QDir(dir))) {
            // Directory is empty. Save.
            return dir;
        } else {
            QMessageBox msgBox(QMessageBox::Question,
                               QObject::tr("Continue?"),
                               QObject::tr("The selected directory is not empty. Some outputs may not be saved. Continue?"),
                               QMessageBox::Save, parent);
            msgBox.addButton(QObject::tr("&Choose Another Directory"),
                             QMessageBox::ActionRole);
            msgBox.addButton(QMessageBox::Discard);

            int result = msgBox.exec();

            if (result == QMessageBox::Save) {
                // Save
                return dir;
            } else if (result == QMessageBox::Discard) {
                return QString();
            }
            // else continue around again for the user to pick another directory
        }
    }
}

void Report::saveReport(QList<report_output_file_t> outputs, QWidget *parent) {
    if (outputs.count() == 0) {
        return; // Nothing to save.
    } else if (outputs.count() == 1) {
        report_output_file_t f = outputs.first();
        f.default_filename = QFileDialog::getSaveFileName(parent,
                                                          QObject::tr("Save Report"),
                                                          QString(),
                                                          f.dialog_filter);
        if (f.default_filename.isEmpty()) {
            return; // User canceled.
        }
        Report::writeFile(f);
    } else {
        // Multiple outputs. The user needs to pick a directory then we'll save them.
        QString directory = getSaveDirectory(parent);
        if (directory.isNull()) {
            return; // user canceled.
        }

        foreach (report_output_file_t output, outputs) {
            output.default_filename = QDir::cleanPath(directory + QDir::separator() + output.default_filename);
            qDebug() << output.default_filename;
            QFile f(output.default_filename);
            if (f.exists()) {
                continue; // File already exists. Skip it so we don't overwrite anything.
            }
            Report::writeFile(output);
        }
    }
}

void Report::outputToUI(QMap<QString, QVariant> reportParameters,
                        QMap<QString, QSqlQuery> queries, bool hasSolar, bool isWireless) {

    ReportDisplayWindow *window = new ReportDisplayWindow(_title, _icon);
    window->setStationInfo(hasSolar, isWireless);

    QList<report_output_file_t> files;

    foreach (output_t output, outputs) {
        if (output.format == OF_HTML || output.format == OF_TEXT) {
            QString saveResult = renderTemplatedReport(reportParameters, queries,
                                                   output.output_template);
            QString viewResult;
            if (!output.view_output_template.isNull()) {
                viewResult = renderTemplatedReport(reportParameters,
                                                   queries,
                                                   output.view_output_template);
            } else {
                viewResult = saveResult;
            }


            QString filter = "";
            QString extension = "";
            if (output.format == OF_HTML) {
                window->addHtmlTab(output.title, output.icon, viewResult);
                filter = QObject::tr("HTML Files (*.html *.html)");
                extension = ".html";
            } else if (output.format == OF_TEXT) {
                window->addPlainTab(output.title, output.icon, viewResult);
                filter = QObject::tr("Text Files (*.txt)");
                extension = ".txt";
            }

            report_output_file_t output_file;
            output_file.default_filename = output.filename;
            output_file.dialog_filter = filter;
            output_file.data = saveResult.toUtf8();

            if (output_file.default_filename.isEmpty()) {
                output_file.default_filename = output.name + extension;
            }

            files.append(output_file);

        } else if (output.format == OF_TABLE) {
            QSqlQueryModel *model = new QSqlQueryModel();
            model->setQuery(queries[output.query_name]);

            QStringList hideColumns;
            for(int i = 0; i < model->columnCount(QModelIndex()); i++) {
                QString col = model->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
                if (output.viewColumns.contains(col)) {
                    if (output.viewColumns[col].isNull()) {
                        hideColumns.append(col);
                    } else {
                        model->setHeaderData(i, Qt::Horizontal, output.viewColumns[col], Qt::DisplayRole);
                    }
                }
            }

            window->addGridTab(output.title, output.icon, model, hideColumns);

            report_output_file_t output_file;
            output_file.default_filename = output.filename;
            output_file.dialog_filter = QObject::tr("CSV Files (*.csv)");
            output_file.query = QSqlQuery(queries[output.query_name]);
            output_file.columnHeadings = output.saveColumns;

            if (output_file.default_filename.isEmpty()) {
                output_file.default_filename = output.name + ".csv";
            }

            files.append(output_file);
        }
    }

    window->setSaveOutputs(files);

    if (this->outputWindowSize.isValid()) {
        window->resize(this->outputWindowSize);
    }

    window->show();
    window->setAttribute(Qt::WA_DeleteOnClose);
}

void Report::outputToDisk(QMap<QString, QVariant> reportParameters,
                          QMap<QString, QSqlQuery> queries) {

    QList<report_output_file_t> files;

    foreach (output_t output, outputs) {
        if (output.format == OF_HTML || output.format == OF_TEXT) {
            QString result = renderTemplatedReport(reportParameters, queries,
                                                   output.output_template);
            QString filter = "";
            QString extension = "";
            if (output.format == OF_HTML) {
                filter = QObject::tr("HTML Files (*.html *.html)");
                extension = ".html";
            } else if (output.format == OF_TEXT) {
                filter = QObject::tr("Text Files (*.txt)");
                extension = ".txt";
            }

            report_output_file_t output_file;
            output_file.default_filename = output.filename;
            output_file.dialog_filter = filter;
            output_file.data = result.toUtf8();

            if (output_file.default_filename.isEmpty()) {
                output_file.default_filename = output.name + extension;
            }

            files.append(output_file);

        } else if (output.format == OF_TABLE) {
            report_output_file_t output_file;
            output_file.default_filename = output.filename;
            output_file.dialog_filter = QObject::tr("CSV Files (*.csv)");
            output_file.query = QSqlQuery(queries[output.query_name]);
            output_file.columnHeadings = output.saveColumns;

            if (output_file.default_filename.isEmpty()) {
                output_file.default_filename = output.name + ".csv";
            }

            files.append(output_file);
        }
    }

    Report::saveReport(files);
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

        qDebug() << "--- Query result for:" << queryName << "---";

        if (query.first()) {
            do {
                QSqlRecord record = query.record();
                QVariantMap row;

                for (int i = 0; i < record.count(); i++) {
                    QSqlField f = record.field(i);
                    row[f.name()] = f.value();
                }
               // qDebug() << row;

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

ReportFinisher::ReportFinisher(Report *report) : QObject(NULL) {
    r = report;
    finished = false;
}

void ReportFinisher::cachingFinished() {
    r->executeReport();
}

void ReportFinisher::finishReport() {
    finished = true;
    emit reportCompleted();
}

bool ReportFinisher::isFinished() {
    return finished;
}
