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

typedef struct _data_file_t data_file_t;
typedef struct _cache_stats_t cache_stats_t;

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
    QScopedPointer<QNetworkAccessManager> liveNetAccessManager;
    QUrl liveDataUrl;
    QTimer livePollTimer;
};

#endif // WEBDATASOURCE_H
