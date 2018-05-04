#ifndef REPORT_H
#define REPORT_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QIcon>

class AbstractDataSource;

class QSqlQuery;

class Report
{
public:
    explicit Report() {_isNull = true; }
    explicit Report(QString name);

    typedef enum {
        TP_None,    // Use of this setting won't prime the web cache!
        TP_Timespan,
        TP_Datespan,
        TP_Day,
        TP_Month,
        TP_Year
    } TimePickerType;

    bool isNull() const {return _isNull;}

    QString name() const {return _name;}
    QString title() const {return _title;}
    QString description() const {return _description;}
    QIcon icon() const {return _icon;}
    TimePickerType timePickerType() const {return _tpType;} // TODO: from file
    bool hasCustomCriteria() const {return false;} // TODO: from file
    bool supportsWebDS() const { return _web_ok; }
    bool supportsDBDS() const { return _db_ok; }

    void run(AbstractDataSource* dataSource, QDateTime start, QDateTime end);
    void run(AbstractDataSource* dataSource, QDate start, QDate end);
    void run(AbstractDataSource* dataSource, QDate dayOrMonth, bool month);
    void run(AbstractDataSource* dataSource, int year);

    static QStringList reports();

    static QList<Report> loadReports();

private:
    bool _isNull;
    QString _name;
    QString _title;
    QString _description;
    bool _db_ok;
    bool _web_ok;
    QIcon _icon;
    TimePickerType _tpType;

    typedef struct _query {
        QString name;
        QString web_query;
        QString db_query;
    } query_t;

    QList<query_t> queries;

    typedef enum _format {
        OF_HTML,
        OF_TEXT,
        OF_TABLE
    } output_format_t;

    typedef struct _output {
        QString name;
        QString title;
        QIcon icon;
        output_format_t format;

        // for OF_HTML and OF_TEXT
        QString output_template;    // For templated output

        // for OF_TABLE
        QString query_name;  // for grid/csv output

        // default filename for saving. If output type is OT_SAVE and
        // multiple outputs are present this will be the save filename (the
        // user will be prompted for an output directory to store all outputs
        // rather than an output file for the single output)
        QString filename; // for when a report is saved
    } output_t;

    QList<output_t> outputs;

    typedef enum {
        OT_DISPLAY,
        OT_SAVE
    } output_type_t;

    output_type_t output_type;

    void run(AbstractDataSource*, QMap<QString, QVariant> parameters);
    void outputToUI(QMap<QString, QVariant> reportParameters,
                    QMap<QString, QSqlQuery> queries);
    void outputToDisk(QMap<QString, QVariant> reportParameters,
                      QMap<QString, QSqlQuery> queries);
    QString renderTemplatedReport(QMap<QString, QVariant> reportParameters,
                                  QMap<QString, QSqlQuery> queries,
                                  QString outputTemplate);
};

#endif // REPORT_H
