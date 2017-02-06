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
#include <QQueue>

typedef struct _data_file_t data_file_t;
typedef struct _cache_stats_t cache_stats_t;

class AbstractWebTask;
class SelectSamplesWebTask;

class WebDataSource : public AbstractDataSource
{
    friend class SelectSamplesWebTask;
    friend class FetchImageWebTask;
    friend class FetchThumbnailWebTask;
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

    // Stubs to allow the app to build while other related functionality is
    // implemented.
    void fetchImageDateList();
    void fetchImageList(QDate date, QString imageSourceCode);
    void fetchImage(int imageId);
    void fetchThumbnails(QList<int> imageIds);

    void fetchLatestImages();

    void hasActiveImageSources();

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
    void queueTask(AbstractWebTask *task, bool startProcessing);

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

    // Task queue processing
    void startQueueProcessing();
    void processNextTask();


    // Progress dialog stuff
    void makeProgress(QString message);
    void moveGoalpost(int,QString);

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


    // Task queue state
    QScopedPointer<QNetworkAccessManager> taskQueueNetworkAccessManager;
    bool processingQueue;
    QQueue<AbstractWebTask*> taskQueue;
    AbstractWebTask* currentTask;
    int currentSubtask;
};

#endif // WEBDATASOURCE_H
