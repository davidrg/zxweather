#ifndef WEBDATASOURCE_H
#define WEBDATASOURCE_H

#include "abstractdatasource.h"

#include <QProgressDialog>
#include <QScopedPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QDateTime>
#include <QUrl>
#include <QTimer>

typedef struct _data_file_t {
    QString filename;
    uint32_t size;
    QDateTime last_modified;
    QString name;
} data_file_t;

class SampleDownloader;

class WebDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit WebDataSource(QWidget* parentWidget = 0, QObject *parent = 0);
    
    void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime());

    void enableLiveData();

    hardware_type_t getHardwareType();

private slots:
    void dataReady(QNetworkReply* reply);
    void liveDataReady(QNetworkReply* reply);
    void liveDataPoll();

private:
    QString baseURL;
    QString stationCode;

    QStringList urlQueue;
    QStringList dataSetQueue;

    QList<data_file_t> dataFileQueue;
    data_file_t currentDataFile;

    QStringList allData;
    QDateTime start;
    QDateTime end;

    QDateTime minTimestamp;
    QDateTime maxTimestamp;

    QScopedPointer<QNetworkAccessManager> netAccessManager;

    QStringList failedDataSets;

    void prepareNextDataSet();
    void processData();
    void rangeRequestResult(QString data);

    void openCache();
    bool isCached(QString station, QDate date);
    bool isMonthCached(QString station, QDate month);

    bool rangeRequest;
    bool dataSetQueuePrep;

    // for live data stuff
    QScopedPointer<QNetworkAccessManager> liveNetAccessManager;
    QUrl liveDataUrl;
    QTimer livePollTimer;
};

#endif // WEBDATASOURCE_H
