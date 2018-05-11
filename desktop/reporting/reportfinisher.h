#ifndef REPORTFINISHER_H
#define REPORTFINISHER_H

#include <QObject>

class Report;

class ReportFinisher: public QObject {
    friend class Report;
    Q_OBJECT

public:
    ReportFinisher(Report* report);
    bool isFinished();

signals:
    void reportCompleted();

private slots:
    void cachingFinished();

private:
    void finishReport();

    Report* r;
    bool finished;
};

#endif // REPORTFINISHER_H
