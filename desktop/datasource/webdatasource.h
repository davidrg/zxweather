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
            QString baseURL, QWidget* parentWidget = 0, QObject *parent = 0);
    
    void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime());

private slots:
    void dataReady(QNetworkReply* reply);

private:
    QString baseURL;

    QStringList urlQueue;
    QStringList dataSetQueue;

    QStringList allData;
    QDateTime start;
    QDateTime end;

    QWidget* parentWidget;
    QScopedPointer<QProgressDialog> progressDialog;
    QScopedPointer<QNetworkAccessManager> netAccessManager;

    void downloadNextDataSet();
    void processData();
};

#endif // WEBDATASOURCE_H
