#ifndef REPORT_H
#define REPORT_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QSqlQuery>
#include <QSet>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QVariant>

#include "scriptingengine.h"
#include "reportfinisher.h"

class AbstractDataSource;
class ScriptValue;

class QSqlQuery;

typedef struct {
    QString default_filename;
    QString dialog_filter;
    QByteArray data;
    QSqlQuery query;
    QMap<QString, QString> columnHeadings;
} report_output_file_t;

class Report
{
    friend class ReportFinisher;

public:
    explicit Report() {_isNull = true; _finisher = NULL;}
    explicit Report(QString name);
    ~Report();

    typedef enum {
        TP_None,    // Use of this setting won't prime the web cache!
        TP_Timespan,
        TP_Datespan,
        TP_Day,
        TP_Month,
        TP_Year
    } TimePickerType;

    typedef enum {
        FTS_None,
        FTS_Today,
        FTS_Last_24H,
        FTS_Yesterday,
        FTS_ThisWeek,
        FTS_LastWeek,
        FTS_Last_7D,
        FTS_Last_14D,
        FTS_ThisMonth,
        FTS_LastMonth,
        FTS_Last_30D,
        FTS_ThisYear,
        FTS_LastYear,
        FTS_Last_365D,
        FTS_AllTime
    } FixedTimeSpan;

    typedef enum {
        WST_Generic,
        WST_WH1080,
        WST_VantagePro2,
        WST_VantagePro2Plus
    } WeatherStationType;

    bool isNull() const {return _isNull;}

    QString name() const {return _name;}
    QString title() const {return _title;}
    QString description() const {return _description;}
    QIcon icon() const {return _icon;}
    TimePickerType timePickerType() const {return _tpType;}
    FixedTimeSpan defaultTimeSpan() const {return _defaultTimeSpan; }
    bool hasCustomCriteria() const {return _custom_criteria;}
    QByteArray customCriteriaUi() const {return _ui;}
    bool supportsWebDS() const { return _web_ok; }
    bool supportsDBDS() const { return _db_ok; }
    QSet<WeatherStationType> supportedWeatherStations() const { return QSet<WeatherStationType>::fromList(_weatherStations); }

    ReportFinisher* run(AbstractDataSource* dataSource, QDateTime start, QDateTime end, QVariantMap parameters);
    ReportFinisher* run(AbstractDataSource* dataSource, QDate start, QDate end, QVariantMap parameters);
    ReportFinisher* run(AbstractDataSource* dataSource, QDate dayOrMonth, bool month, QVariantMap parameters);
    ReportFinisher* run(AbstractDataSource* dataSource, int year, QVariantMap parameters);

    static QStringList reports();

    static QList<Report> loadReports();

    static void saveReport(QList<report_output_file_t> outputs, QWidget *parent=NULL);

    void executeReport();

    bool operator <(Report const& b)const;

private:
    bool _isNull;
    QString _name;
    QString _title;
    QString _description;
    bool _custom_criteria;
    QByteArray _ui;
    bool _db_ok;
    bool _web_ok;
    QIcon _icon;
    TimePickerType _tpType;
    FixedTimeSpan _defaultTimeSpan;
    QList<WeatherStationType> _weatherStations;
    bool _primeCache;
    bool _debug;
    QStringList _scripts;

    QSharedPointer<ScriptingEngine> scriptingEngine;

    typedef struct _query_variant {
        QString query_text;
        QSet<QString> parameters;
    } query_variant_t;

//    typedef struct _data_generator_variant {
//        QString script_text;
//        QString file_name;
//    } data_generator_variant_t;

    typedef struct _query {
        QString name;
        query_variant_t web_query;
        query_variant_t db_query;
//        data_generator_variant_t generator;
    } query_t;

    QList<query_t> queries;

    typedef enum _format {
        OF_HTML,
        OF_TEXT,
        OF_TEXT_WRAPPED,
        OF_TABLE
    } output_format_t;

    typedef struct _output {
        QString name;
        QString title;
        QIcon icon;
        output_format_t format;

        // for OF_HTML and OF_TEXT
        QString output_template;    // For templated output
        QString view_output_template; // For templated output on screen

        // for OF_TABLE
        QString query_name;  // for grid/csv output

        // default filename for saving. If output type is OT_SAVE and
        // multiple outputs are present this will be the save filename (the
        // user will be prompted for an output directory to store all outputs
        // rather than an output file for the single output)
        QString filename; // for when a report is saved

        // Column headings for view and save. Use a column heading of null
        // to disable that column in either the view-as-grid or save-to-csv.
        // Columns not included in these will receive default settings.
        QMap<QString, QString> viewColumns;
        QMap<QString, QString> saveColumns;
    } output_t;
    
    typedef struct _query_result {
        QStringList columnNames;
        QList<QVariantList> rowData;
        QString name;
    } query_result_t;

    QList<output_t> outputs;

    typedef enum {
        OT_DISPLAY,
        OT_SAVE
    } output_type_t;

    output_type_t output_type;

    QSize outputWindowSize;

    // For running the report
    ReportFinisher* _finisher;
    AbstractDataSource *_dataSource;
    QVariantMap _parameters;

    static QByteArray queryResultToCSV(query_result_t query, QMap<QString, QString> columnHeadings);
    static void writeFile(report_output_file_t file);

    void run(AbstractDataSource*, QMap<QString, QVariant> parameters);
    void completeReport();
    void outputToUI(QMap<QString, QVariant> reportParameters,
                    QMap<QString, query_result_t> queries, bool hasSolar, bool isWireless);
    void outputToDisk(QMap<QString, QVariant> reportParameters,
                      QMap<QString, query_result_t> queries);
    QString renderTemplatedReport(QMap<QString, QVariant> reportParameters,
                                  QMap<QString, query_result_t> queries,
                                  QString outputTemplate);

    query_result_t runDataGenerator(QString script_text,
                                    QString scriptFileName,
                                    QMap<QString, QVariant> parameters,
                                    bool isWeb,
                                    QString stationCode,
                                    QString queryName);
    query_result_t runDataQuery(QString queryText,
                                QMap<QString, QVariant> parameters,
                                bool isWeb,
                                QSqlQuery query,
                                QVariantMap &debugInfo,
                                QVariantList &queryDebugInfo,
                                QString queryName,
                                QSet<QString> queryParameters);

    QMap<QString, Report::query_result_t> runDataGenerators(QMap<QString, QVariant> parameters);

    Report::query_result_t scriptValueToResultSet(ScriptValue value);
};

#endif // REPORT_H
