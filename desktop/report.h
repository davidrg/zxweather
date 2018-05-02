#ifndef REPORT_H
#define REPORT_H

#include <QList>
#include <QString>
#include <QStringList>

class AbstractDataSource;

class Report
{
public:
    explicit Report() {_isNull = true; }
    explicit Report(QString name);

    typedef enum {
        TP_None,
        TP_Time,
        TP_Date,
        TP_Month,
        TP_Year
    } TimePickerType;

    bool isNull() const {return _isNull;}

    QString name() const {return _name;}
    QString title() const {return _title;}
    QString description() const {return _description;}
    TimePickerType timePickerType() const {return TP_Time;}
    bool hasCustomCriteria() const {return false;}

    void run(AbstractDataSource* dataSource, QDateTime start, QDateTime end);

    static QStringList reports();

    static QList<Report> loadReports();

private:
    bool _isNull;
    QString _name;
    QString _title;
    QString _description;

    typedef struct _query {
        QString name;
        QString web_query;
        QString db_query;
    } query_t;

    QList<query_t> queries;

    QString outputTemplate;
};

#endif // REPORT_H
