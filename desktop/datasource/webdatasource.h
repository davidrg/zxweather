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
#include <QByteArray>

typedef struct _data_file_t data_file_t;
typedef struct _cache_stats_t cache_stats_t;

class WebDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit WebDataSource(QWidget* parentWidget = 0, QObject *parent = 0);
    
    void fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0);

    void enableLiveData();

    hardware_type_t getHardwareType();

    bool hasUVAndSolarSensors();
private slots:
    void liveDataReady(QNetworkReply* reply);
    void liveDataPoll();

    /**** Sample data network response handlers ****/
    void requestFinished(QNetworkReply* reply);

private:

    /**** Sample data private functions ****/
    QUrl buildRangeRequestURL() const;

    void rangeRequestFinished(QNetworkReply* reply);


    void makeNextCacheStatusRequest();
    void cacheStatusRequestFinished(QNetworkReply* reply);

    void makeNextDataRequest();
    void downloadRequestFinished(QNetworkReply* reply);
    void completeDataRequest();
    data_file_t loadDataFile(QString url, QStringList fileData,
                             QDateTime lastModified, int fileSize,
                             cache_stats_t cacheStats);

    SampleSet selectRequestedData();

    typedef enum {
        DLS_READY,          /*!< Not doing anything. Ready to fetch data */
        DLS_INIT,           /*!< Setup */
        DLS_RANGE_REQUEST,  /*!< Checking that the request range falls within
                                 what the server has available */
        DLS_CHECK_CACHE,    /*!< Checking which URLs are almost certainly
                                 cached */
        DLS_DOWNLOAD_DATA   /*!< We're downloading full data files */
    } SampleDownloadState;

    void setDownloadState(SampleDownloadState state) {download_state = state;}

    void makeProgress(QString message);

    void dlReset();

    /**** Sample data member variables ****/
    SampleDownloadState download_state;

    // Details of the data to request from the CacheDB once its populated.
    SampleColumns columnsToReturn;
    AggregateFunction returnAggregate;
    AggregateGroupType returnGroupType;
    uint32_t returnGroupMinutes;


    // Time range we are currently downloading:
    QDateTime dlStartTime, dlEndTime;

    QStringList candidateURLs;
    QStringList acceptedURLs;
    QHash<QString, QString> urlNames;

    QString baseURL;
    QString stationUrl; // Used as a key in the cache DB.
    QString stationCode;


    QScopedPointer<QNetworkAccessManager> netAccessManager;

    // Local cache database functionality
    void openCache();
    bool isCached(QString station, QDate date);
    bool isMonthCached(QString station, QDate month);



    // for live data stuff
    void ProcessStationConfig(QNetworkReply *reply);
    bool LoadStationConfigData(QByteArray data);
    QScopedPointer<QNetworkAccessManager> liveNetAccessManager;
    QUrl liveDataUrl;
    QTimer livePollTimer;
    bool stationConfigLoaded;
    bool isSolarDataAvailable;  // also used by sample retrieval
    hardware_type_t hwType;
    QString station_name;
};

#endif // WEBDATASOURCE_H
