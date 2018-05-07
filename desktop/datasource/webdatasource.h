#ifndef WEBDATASOURCE_H
#define WEBDATASOURCE_H

#include "abstractdatasource.h"
#include <QScopedPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QDateTime>
#include <QUrl>
#include <QTimer>
#include <QByteArray>
#include <QQueue>
#include <QMap>

typedef struct _data_file_t data_file_t;
typedef struct _cache_stats_t cache_stats_t;

class AbstractWebTask;
class SelectSamplesWebTask;

class WebDataSource : public AbstractDataSource
{
    friend class SelectSamplesWebTask;
    friend class FetchImageWebTask;
    friend class FetchThumbnailWebTask;
    friend class FetchRainTotalsWebTask;
    friend class FetchSamplesWebTask;
    friend class FetchStationInfoWebTask;
    Q_OBJECT

public:
    explicit WebDataSource(AbstractProgressListener *progressListener = 0, QObject *parent = 0);
    
    void fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0);

    void fetchSamplesFromCache(DataSet dataSet);

    QSqlQuery query();

    void enableLiveData();

    hardware_type_t getHardwareType();

    // Stubs to allow the app to build while other related functionality is
    // implemented.
    void fetchImageDateList();
    void fetchImageList(QDate date, QString imageSourceCode);
    void fetchImage(int imageId);
    void fetchThumbnails(QList<int> imageIds);

    void fetchLatestImages();

    void hasActiveImageSources();

    void fetchRainTotals();

    void primeCache(QDateTime start, QDateTime end);

    bool solarAvailable();

private slots:
    void liveDataReady(QNetworkReply* reply);
    void liveDataPoll();
    void imagesPoll();

    // Called by ActiveImageSourcesWebTask
    void foundActiveImageSource();
    void foundArchivedImages();

    void fetchImageDateListComplete(QList<ImageDate> imageDates,
                                    QList<ImageSource> imageSources);

    void imageListComplete(QList<ImageInfo> images);

    // Task queue slots

    void queueTask(AbstractWebTask* task);
    void queueTask(AbstractWebTask *task, bool startProcessing,
                   bool priority=false, bool lowPriority=false);

    void subtaskChanged(QString name);
    void httpGet(QNetworkRequest request);
    void httpHead(QNetworkRequest request);
    void taskFinished();
    void taskFailed(QString error);

    void taskQueueResponseDataReady(QNetworkReply* reply);
private:
    // Called by SelectSamplesWebTask
    void fireSamplesReady(SampleSet samples);

    // Called by GetImageWebTask
    void fireImageReady(ImageInfo imageInfo, QImage image, QString cacheFile);
    void fireThumbnailReady(int imageId, QImage thumbnail);

    // called by FetchRainTotalsWebTask
    void fireRainTotals(QDate date, double day, double month, double year);

    // Task queue processing
    void startQueueProcessing();
    void processNextTask();


    // Progress dialog stuff
    void makeProgress(QString message);
    void moveGoalpost(int,QString);

    // For updating station info in the cache DB
    void updateStation(QString title, QString description, QString type_code,
                       int interval, float latitude, float longitude, float altitude,
                       bool solar, int davis_broadcast_id);

    QString stationURL() const;

    QString baseURL;
    QString stationCode;

    // for live data stuff
    void ProcessStationConfig(QNetworkReply *reply);
    bool LoadStationConfigData(QByteArray data);
    QScopedPointer<QNetworkAccessManager> liveNetAccessManager;
    QUrl liveDataUrl;
    QTimer livePollTimer;
    QTimer imagePollTimer;
    bool stationConfigLoaded;
    bool isSolarDataAvailable;  // also used by sample retrieval
    hardware_type_t hwType;
    QString station_name;
    int lastImageId;
    int lastSampleId;
    QMap<QString, int> lastImageIds;


    // Task queue state
    QScopedPointer<QNetworkAccessManager> taskQueueNetworkAccessManager;
    bool processingQueue;
    QQueue<AbstractWebTask*> highPriorityTaskQueue;
    QQueue<AbstractWebTask*> taskQueue;
    QQueue<AbstractWebTask*> lowPriorityQueue;
    AbstractWebTask* currentTask;
    int currentSubtask;
};

#endif // WEBDATASOURCE_H
