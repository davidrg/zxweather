#ifndef WEBDATASOURCE_H
#define WEBDATASOURCE_H

#include "abstractdatasource.h"

#include <QProgressDialog>
#include <QScopedPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QDateTime>

class WebDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit WebDataSource(
            QString baseURL, QString stationCode, QWidget* parentWidget = 0, QObject *parent = 0);
    
    void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime());

private slots:
    void dataReady(QNetworkReply* reply);

private:
    QString baseURL;
    QString stationCode;

    QStringList urlQueue;
    QStringList dataSetQueue;

    QStringList allData;
    QDateTime start;
    QDateTime end;

    QDateTime minTimestamp;
    QDateTime maxTimestamp;

    QScopedPointer<QNetworkAccessManager> netAccessManager;

    QStringList failedDataSets;

    void downloadNextDataSet();
    void processData();
    void rangeRequestResult(QString data);

    bool rangeRequest;
};

#endif // WEBDATASOURCE_H
