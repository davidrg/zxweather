#include "webdatasource.h"
#include "constants.h"
#include "settings.h"
#include "json/json.h"
#include "webcachedb.h"

#include "webtasks/fetchsampleswebtask.h"
#include "webtasks/activeimagesourceswebtask.h"
#include "webtasks/latestimageswebtask.h"
#include "webtasks/fetchimagewebtask.h"
#include "webtasks/fetchthumbnailwebtask.h"
#include "webtasks/fetchimagedatelistwebtask.h"
#include "webtasks/listdayimageswebtask.h"
#include "webtasks/fetchraintotalswebtask.h"
#include "webtasks/request_data.h"
#include "webtasks/selectsampleswebtask.h"
#include "webtasks/fetchstationinfo.h"

#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QVariantMap>
#include <QtDebug>
#include <float.h>
#include <QDateTime>
#include <QNetworkProxyFactory>

/*
 * A note on CODES
 * ---------------
 * Codes throughout zxweather (station, image source, hardware type, etc)
 * should always be treated case-insensitively. Unlikle the database data source
 * which converts all codes it encounters to UPPERCASE, here in the web data
 * source we convert them to lowercase. This is because they often end up in
 * URLs to the web interface where lowercase is expected for filesystem reasons.
 */


// TODO: make the progress dialog cancel button work.

/*
 * TODO:
 *  - Make the cancel button work
 *  - When downloading data for the current month:
 *    + If the number of days desired is less than 10 then download day-level
 *      data files
 *    + Write it to the cache as a month-level data source and expire anything
 *      that is already there. The only reason to write it to the cache at
 *      all is so that we don't have to mix a cache response with raw data
 *      from a bunch of data files (we just get it all sorted and tidy from
 *      the cache)
 *    This should mean we won't end up downloading hundreds of KB each time
 *    you want a chart for today.
 */

/*****************************************************************************
 **** CONSTRUCTOR ************************************************************
 *****************************************************************************/
WebDataSource::WebDataSource(AbstractProgressListener *progressListener, QObject *parent) :
    AbstractDataSource(progressListener, parent)
{    
    Settings& settings = Settings::getInstance();
    baseURL = settings.webInterfaceUrl();
    stationCode = settings.stationCode().toLower();
    liveDataUrl = QUrl(baseURL + "data/" + stationCode + "/live.json");

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Setup live data functionality
    liveNetAccessManager.reset(new QNetworkAccessManager(this));
    connect(liveNetAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(liveDataReady(QNetworkReply*)));
    livePollTimer.setInterval(30000);
    connect(&livePollTimer, SIGNAL(timeout()), this, SLOT(liveDataPoll()));
    imagePollTimer.setInterval(3600000);
    connect(&imagePollTimer, SIGNAL(timeout()), this, SLOT(imagesPoll()));
    stationConfigLoaded = false;
    lastImageId = 0;

    /*******************
     * Task Queue Stuff
     *//////////////////

    taskQueueNetworkAccessManager.reset(new QNetworkAccessManager(this));
    connect(taskQueueNetworkAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(taskQueueResponseDataReady(QNetworkReply*)));
    processingQueue = false;
    currentTask = 0;
    currentSubtask = 0;

    // Qt 5.9 adds a handy setting where we can just say to follow redirects.
    // For 5.6-5.8 we have to set an attribute on the individual network requests.
    // For 4.8-5.5 we have to handle redirects manually.
    #if QT_VERSION >= 0x050900
    taskQueueNetworkAccessManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    #endif

    if (!WebCacheDB::getInstance().stationKnown(stationURL())) {
        queueTask(new FetchStationInfoWebTask(baseURL, stationCode, this));
    }
}


#if QT_VERSION < 0x050600
QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                 const QUrl& oldRedirectUrl) {
     QUrl redirectUrl;
     /*
      * Check if the URL is empty and
      * that we aren't being fooled into a infinite redirect loop.
      * We could also keep track of how many redirects we have been to
      * and set a limit to it, but we'll leave that to you.
      */
     if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl) {
        redirectUrl = possibleRedirectUrl;
     }
     return redirectUrl;
}
#endif


/*****************************************************************************
 **** SAMPLE DATA ************************************************************
 *****************************************************************************/

/** Fetches weather samples from the remote server. The samplesReady signal is
 * emitted when the samples have arrived.
 *
 * @param columns The columns to return.
 * @param startTime timestamp for the first record to return.
 * @param endTime timestamp for the last record to return
 */
void WebDataSource::fetchSamples(SampleColumns columns,
                                 QDateTime startTime,
                                 QDateTime endTime,
                                 AggregateFunction aggregateFunction,
                                 AggregateGroupType groupType,
                                 uint32_t groupMinutes) {

    FetchSamplesWebTask* task = new FetchSamplesWebTask(
                baseURL,
                stationCode,
                columns,
                startTime,
                endTime,
                aggregateFunction,
                groupType,
                groupMinutes,
                true,
                this
    );

    queueTask(task);
}

void WebDataSource::fetchSamplesFromCache(DataSet dataSet) {
    request_data_t request;
    request.columns = dataSet.columns;
    request.startTime = dataSet.startTime;
    request.endTime = dataSet.endTime;
    request.aggregateFunction = dataSet.aggregateFunction;
    request.groupType = dataSet.groupType;
    request.groupMinutes = dataSet.customGroupMinutes;

    /* These values aren't used by SelectSamplesWebTask
     *
     * request.stationName = station_name;
     * request.isSolarAvailable = isSolarDataAvailable;
     * request.hwType = hwType;
     */

    SelectSamplesWebTask *selectTask = new SelectSamplesWebTask(
                baseURL,
                stationCode,
                request,
                this);

    // Queue the task with low priority to ensure its processed *after* any
    // primeCache() call.
    queueTask(selectTask, false, true);
}

void WebDataSource::primeCache(QDateTime start, QDateTime end) {
    FetchImageDateListWebTask *dateListTask = new FetchImageDateListWebTask(
                baseURL, stationCode, this, true);

    FetchSamplesWebTask* task = new FetchSamplesWebTask(
                baseURL,
                stationCode,
                ALL_SAMPLE_COLUMNS,
                start,
                end,
                AF_None,
                AGT_None,
                0,
                /* Fetch the samples but don't select them from the cache. This will
                 * ensure all samples are available within the specified timespan are
                 * available for future calls to fetchSamplesFromCache(). */
                false,
                this
    );

    // Prime the cache with priority to ensure this is processed before any subsequent
    // selects
    queueTask(dateListTask, false, true);
    queueTask(task, true, false);
}

QSqlQuery WebDataSource::query() {
    return WebCacheDB::query();
}

void WebDataSource::fireSamplesReady(SampleSet samples) {
    emit samplesReady(samples);
}

void WebDataSource::fetchRainTotals() {
    queueTask(new FetchRainTotalsWebTask(baseURL, stationCode, this));
}

void WebDataSource::fireRainTotals(QDate date, double day, double month,
                                   double year) {
    emit rainTotalsReady(date, day, month, year);
}

/*****************************************************************************
 **** LIVE DATA **************************************************************
 *****************************************************************************/

bool WebDataSource::LoadStationConfigData(QByteArray data) {
    using namespace QtJson;
    bool ok;

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        qDebug() << "sysconfig parse error. Data:" << data;
        emit error("JSON parsing failed while loading system configuration.");
        return false;
    }

    QVariantList stations = result["stations"].toList();
    foreach (QVariant station, stations) {
        QVariantMap stationData = station.toMap();

        if (stationData["code"].toString().toLower() == stationCode) {
            station_name = stationData["name"].toString();
            isSolarDataAvailable = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString().toUpper();

            if (hw == "DAVIS") {
                isSolarDataAvailable = stationData["hw_config"].toMap()["has_solar_and_uv"].toBool();
                hwType = HW_DAVIS;
            } else if (hw == "FOWH1080") {
                hwType = HW_FINE_OFFSET;
            } else {
                hwType = HW_GENERIC;
            }
            stationConfigLoaded = true;
            return true;
        }
    }

    return false;
}

void WebDataSource::ProcessStationConfig(QNetworkReply *reply) {
    using namespace QtJson;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        LoadStationConfigData(reply->readAll());

        qDebug() << "Station" << station_name << "has solar data available?" << isSolarDataAvailable;

        emit stationName(station_name);
        emit isSolarDataEnabled(isSolarDataAvailable);
    }

    liveDataPoll();
}

void WebDataSource::liveDataReady(QNetworkReply *reply) {
#if QT_VERSION < 0x050600
    /*
     * Reply is finished!
     * We'll ask for the reply about the Redirection attribute
     * http://doc.trolltech.com/qnetworkrequest.html#Attribute-enum
     */
    QVariant possibleRedirectUrl =
             reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    /* We'll deduct if the redirection is valid in the redirectUrl function */
    liveRedirect = redirectUrl(possibleRedirectUrl.toUrl(),
                                         liveRedirect);

    /* If the URL is not empty, we're being redirected. */
    if(!liveRedirect.isEmpty()) {
        switch(reply->operation()) {
        case QNetworkAccessManager::GetOperation:
            this->taskQueueNetworkAccessManager->get(QNetworkRequest(liveRedirect));
            break;
        case QNetworkAccessManager::HeadOperation:
            this->taskQueueNetworkAccessManager->head(QNetworkRequest(liveRedirect));
            break;
        default:
            qWarning() << "Unsupported Operation" << reply->operation();
        }

        reply->deleteLater();
        return;
    }

    /*
     * We weren't redirected anymore
     * so we arrived to the final destination...
     */

    /* ...so this can be cleared. */
    liveRedirect.clear();


#endif






    using namespace QtJson;

    if (!stationConfigLoaded) {
        ProcessStationConfig(reply);
        return;
    }


    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        LiveDataSet lds;
        bool ok;

        QVariantMap result = Json::parse(reply->readAll(), ok).toMap();

        if (!ok) {
            emit error("JSON parsing failed");
            return;
        }

        lds.windDirection = result["wind_direction"].toFloat();
        lds.windSpeed = result["average_wind_speed"].toFloat();
        lds.temperature = result["temperature"].toFloat();
        lds.dewPoint = result["dew_point"].toFloat();
        lds.windChill = result["wind_chill"].toFloat();
        lds.humidity = result["relative_humidity"].toInt();
        lds.timestamp = QDateTime::fromString(
                             result["time_stamp"].toString(), "HH:mm:ss");
        lds.apparentTemperature = result["apparent_temperature"].toFloat();
        lds.pressure = result["absolute_pressure"].toFloat();

        QString hw_type_str = result["hw_type"].toString();

        if (hw_type_str == "DAVIS")
            lds.hw_type = HW_DAVIS;
        else if (hw_type_str == "FOWH1080")
            lds.hw_type = HW_FINE_OFFSET;
        else
            lds.hw_type = HW_GENERIC;

        if (lds.hw_type == HW_DAVIS) {
            QVariantMap dd = result["davis"].toMap();

            lds.davisHw.barometerTrend = dd["bar_trend"].toInt();
            lds.davisHw.rainRate = dd["rain_rate"].toFloat();
            lds.davisHw.stormRain = dd["storm_rain"].toFloat();
            lds.davisHw.stormDateValid = !dd["current_storm_date"].isNull();
            if (lds.davisHw.stormDateValid)
                lds.davisHw.stormStartDate = QDate::fromString(
                            dd["current_storm_date"].toString(), "yyyy-MM-dd");
            lds.davisHw.txBatteryStatus = dd["tx_batt"].toInt();
            lds.davisHw.consoleBatteryVoltage = dd["console_batt"].toFloat();
            lds.davisHw.forecastIcon = dd["forecast_icon"].toInt();
            lds.davisHw.forecastRule = dd["forecast_rule"].toInt();
            lds.davisHw.uvIndex = dd["uv_index"].toFloat();
            lds.davisHw.solarRadiation = dd["solar_radiation"].toFloat();
        }

        // Indoor data is not currently available from the website data feed.
        lds.indoorDataAvailable = false;

        emit liveData(lds);

        // now for samples!
        if (result.contains("sample") && !result["sample"].isNull()) {
            QVariantMap s = result["sample"].toMap();
            if (s["id"] != lastSampleId) {
                Sample sample;
                lastSampleId = s["id"].toInt();
                sample.timestamp = s["time_stamp"].toDateTime();
                sample.dewPoint = s["dew_point"].toDouble();
                sample.apparentTemperature = s["apparent_temperature"].toDouble();
                sample.windChill = s["wind_chill"].toDouble();
                sample.indoorTemperature = s["indoor_temperature"].toDouble();
                sample.humidity = s["humidity"].toInt();
                sample.indoorHumidity = s["indoor_humidity"].toInt();
                sample.pressure = s["pressure"].toDouble();
                sample.rainfall = s["rainfall"].toDouble();
                sample.averageWindSpeed = s["average_wind_speed"].toDouble();
                sample.gustWindSpeed = s["gust_wind_speed"].toDouble();
                sample.windDirectionValid = !s["wind_direction"].isNull();
                sample.windDirection = s["wind_direction"].toInt();
                sample.solarRadiationValid = !s["solar_radiation"].isNull();
                sample.solarRadiation = s["solar_radiation"].toDouble();
                sample.uvIndexValid = !s["uv_index"].isNull();
                sample.uvIndex = s["uv_index"].toDouble();
                emit newSample(sample);
            }
        }

        // And images...
        if (result.contains("images") && !result["images"].isNull()) {
            QVariantList images = result["images"].toList();
            foreach (QVariant imageData, images) {
                QVariantMap imageMap = imageData.toMap();
                NewImageInfo i;
                i.imageId = imageMap["id"].toInt();
                i.timestamp = imageMap["time_stamp"].toDateTime();
                i.stationCode = stationCode;
                i.imageSourceCode = imageMap["source"].toString();

                bool isNew = false;

                if (lastImageIds.contains(i.imageSourceCode)) {
                    if (lastImageIds[i.imageSourceCode] != i.imageId) {
                        lastImageIds[i.imageSourceCode] = i.imageId;
                        isNew = true;
                    }
                } else {
                    isNew = true;
                }

                if (isNew) {
                    emit newImage(i);
                }
            }
        }
    }
}

void WebDataSource::liveDataPoll() {

    if (!stationConfigLoaded) {        
        QNetworkRequest request;
        request.setUrl(baseURL + "data/sysconfig.json");
        request.setRawHeader("User-Agent", Constants::USER_AGENT);

#if QT_VERSION < 0x050900 && QT_VERSION >= 0x050600
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

        liveNetAccessManager->get(request);

        // Figure out if the station also has active image sources.
        hasActiveImageSources();
    } else {
        QNetworkRequest request;
        request.setUrl(liveDataUrl);
        request.setRawHeader("User-Agent", Constants::USER_AGENT);

#if QT_VERSION < 0x050900 && QT_VERSION >= 0x050600
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

        liveNetAccessManager->get(request);
    }

    if (!imagePollTimer.isActive()) {
        // Polling for new images is currently disabled because we don't know
        // about any active image sources. On startup the application will
        // grab a list of all the latest images and use this to decide if there
        // are any. Metadata for those images is stored in the cache DB so we
        // can look them up there and make that same decision.
        QString url = baseURL + "data/" + stationCode + "/";
        QVector<ImageInfo> latestImages = WebCacheDB::getInstance().getMostRecentImages(url);

        QDateTime minTs = QDateTime();
        minTs.addDays(-1);

        // Look through the latest images in the cache database. If any are
        // less than 24 hours old we'll assume there are active image sources
        // and start polling for new images.
        foreach (ImageInfo img, latestImages) {
            if (img.timeStamp > minTs) {
                imagePollTimer.start();
                qDebug() << "Found active image source in cache DB. Starting image poll timer.";
                return;
            }
        }
    }
}

void WebDataSource::imagesPoll() {
    qDebug() << QDateTime().toString() << "Poll for latest images...";
    fetchLatestImages();
}

void WebDataSource::enableLiveData() {
    liveDataPoll();
    livePollTimer.start();
}

void WebDataSource::disableLiveData() {
    livePollTimer.stop();
}

QString WebDataSource::stationURL() const {
    Settings& settings = Settings::getInstance();
    QString baseURL = settings.webInterfaceUrl();

    QString dataRootUrl = baseURL + "data/";
    QString stationBaseUrl = dataRootUrl + stationCode + "/";
#ifdef USE_GNUPLOT_DATA
    QString stationDataUrl = baseURL + "b/" + stationCode + "/";
#else
    QString stationDataUrl = stationBaseUrl;
#endif

    return stationDataUrl;
}

hardware_type_t WebDataSource::getHardwareType() {
    // we cant determine the hardware type on demand like this. We won't
    // know until after live data is turned on or we've processed a request
    // for samples.
    QString typ = WebCacheDB::getInstance().hw_type(stationURL());

    if (typ == "generic") {
        return HW_GENERIC;
    } else if (typ == "fowh1080") {
        return HW_FINE_OFFSET;
    } else if (typ == "davis") {
        return HW_DAVIS;
    }

    return HW_GENERIC;
}

bool WebDataSource::solarAvailable() {
    return WebCacheDB::getInstance().solarAvailable(stationURL());
}

station_info_t WebDataSource::getStationInfo() {
    return WebCacheDB::getInstance().getStationInfo(stationURL());
}

sample_range_t WebDataSource::getSampleRange() {
    // TODO: Get this from the Web UI instead as the cache DB probably doesn't
    // represent the full range of data available.
    sample_range_t range;
    range.isValid = false;
    return range;
    //return WebCacheDB::getInstance().getSampleRange(stationURL());
}

/*****************************************************************************
 **** IMAGES *****************************************************************
 *****************************************************************************/



void WebDataSource::fetchImageDateList() {
    FetchImageDateListWebTask *task = new FetchImageDateListWebTask(
                baseURL, stationCode, this);
    connect(task, SIGNAL(dateListReady(QList<ImageDate>,QList<ImageSource>)),
            this, SLOT(fetchImageDateListComplete(QList<ImageDate>,QList<ImageSource>)));
    queueTask(task);
}

void WebDataSource::fetchImageDateListComplete(QList<ImageDate> imageDates,
                   QList<ImageSource> imageSources) {
    emit imageDatesReady(imageDates, imageSources);
}

void WebDataSource::fetchImageList(QDate date, QString imageSourceCode) {
    ListDayImagesWebTask *task = new ListDayImagesWebTask(
                baseURL, stationCode, this, date, imageSourceCode);
    connect(task, SIGNAL(imageListReady(QList<ImageInfo>)),
            this, SLOT(imageListComplete(QList<ImageInfo>)));

    // Queue as a high priority task to ensure if multiple image lists are
    // requested they're processed before any resulting image requests.
    // We do this to prevent weird behaviour in the progress dialog where the
    // bar just jumps backwards and fowards until the entire job is done.
    queueTask(task, true, true);
}

void WebDataSource::imageListComplete(QList<ImageInfo> images) {
    emit imageListReady(images);
}

/*
 * NOTE:
 *   fetchImage(int) and fetchThumbnails(QList<int>) will only work for images
 *   that have previously had their MetaData retrieved via
 *   fetchImageList(QDate,QString). Images obtained through fetchLatestImages()
 *   or any other 'live' source don't have their metadata stored in the cache
 *   database and so aren't able to be looked up by id. Actual image data for
 *   these 'live' images *will* be cached however - all thats missing is a row
 *   in the cache DB.
 */

void WebDataSource::fetchImage(int imageId) {
    FetchImageWebTask *task = new FetchImageWebTask(
                baseURL,
                stationCode,
                this,
                imageId);

    queueTask(task);
}

void WebDataSource::fetchThumbnails(QList<int> imageIds) {    
    foreach (int imageId, imageIds) {
        FetchThumbnailWebTask *task = new FetchThumbnailWebTask(
                    baseURL,
                    stationCode,
                    this,
                    imageId);
        queueTask(task, false);
    }

    startQueueProcessing();
}


/*
 * hasActiveImageSources()
 *
 *     +---------------------------+
 *     | ActiveImageSourcesWebTask |
 *     +---------------------------+
 *
 * To slots:
 *   foundActiveImageSources()
 *   foundArchivedImages()
 */


void WebDataSource::hasActiveImageSources() {

    ActiveImageSourcesWebTask* task = new ActiveImageSourcesWebTask(
                baseURL,
                stationCode,
                this);

    connect(task, SIGNAL(activeImageSourcesAvailable()),
            this, SLOT(foundActiveImageSource()));
    connect(task, SIGNAL(archivedImagesAvailable()),
            this, SLOT(foundArchivedImages()));

    queueTask(task);
}

void WebDataSource::foundActiveImageSource() {
    emit activeImageSourcesAvailable();
}

void WebDataSource::foundArchivedImages() {
    emit archivedImagesAvailable();
}

void WebDataSource::fetchLatestImages() {
    LatestImagesWebTask* task = new LatestImagesWebTask(
                baseURL,
                stationCode,
                this);

    queueTask(task);
}

void WebDataSource::fireImageReady(ImageInfo imageInfo, QImage image,
                                   QString cacheFile) {
    emit imageReady(imageInfo, image, cacheFile);

    if (imagePollTimer.isActive()) {
        // This data source is being used for live data. If the supplied image
        // is new, say so
        if (imageInfo.id != lastImageId) {
            lastImageId = imageInfo.id;

            NewImageInfo info;
            info.imageId = imageInfo.id;
            info.stationCode = stationCode;
            info.timestamp = imageInfo.timeStamp;
            info.imageSourceCode = imageInfo.imageSource.code;

            emit newImage(info);
        }
    }
}

void WebDataSource::fireThumbnailReady(int imageId, QImage thumbnail) {
    emit thumbnailReady(imageId, thumbnail);
}


void WebDataSource::updateStation(QString title, QString description, QString type_code,
                                  int interval, float latitude, float longitude, float altitude,
                                  bool solar, int davis_broadcast_id) {


    WebCacheDB::getInstance().updateStation(
                stationURL(), title, description, type_code, interval, latitude, longitude,
                altitude, solar, davis_broadcast_id);
}

void WebDataSource::finishedCaching() {
    emit cachingFinished();
}

/*****************************************************************************
 **** TASK QUEUE *************************************************************
 *****************************************************************************/
#ifdef QT_DEBUG
//#define TQLOG qDebug() << QString("Progress[%1/%2] Subtask[%3/%4] Task[%5]") \
//                            .arg(progressListener->value()).arg(progressListener->maximum()) \
//                            .arg(currentSubtask) \
//                            .arg(currentTask == NULL ? 0 : currentTask->subtasks()) \
//                            .arg(currentTask == NULL ? "" : currentTask->taskName()).toLatin1()
#define TQLOG qDebug() << "Progress[" << progressListener->value() \
                       << "/" << progressListener->maximum() \
                       << "] Subtask[" << currentSubtask << "/" \
                       << (currentTask == NULL ? 0 : currentTask->subtasks()) \
                       << "] Task[" << (currentTask == NULL ? "no-task" : currentTask->taskName()) << "]"
#else
#define TQLOG qDebug()
#endif

void WebDataSource::makeProgress(QString message) {
    progressListener->setSubtaskName(message);
    int value = progressListener->value() + 1;
    int maxValue = progressListener->maximum();
    progressListener->setValue(value);

    TQLOG << "Making progress:" << message;
}

void WebDataSource::moveGoalpost(int distance, QString reason) {

    int curVal = progressListener->value();
    int curMax = progressListener->maximum();

    TQLOG << "Antiprogress: [" << curVal << "/" << curMax << "] to ["
             << curVal << "/" << curMax + distance << "] - " << reason;

    progressListener->setMaximum(curMax + distance);
}

void WebDataSource::queueTask(AbstractWebTask *task) {
    queueTask(task,true);
}


void WebDataSource::queueTask(AbstractWebTask *task, bool startProcessing,
                              bool priority, bool lowPriority) {

    if (highPriorityTaskQueue.isEmpty() && taskQueue.isEmpty() && lowPriorityQueue.isEmpty() && !processingQueue) {
        progressListener->reset();
        progressListener->setMaximum(1);
        progressListener->setValue(0);
    }

    // Move the goalposts - +1 for the task itself
    int taskProgressPoints = task->subtasks() + 1;
    TQLOG << "Queue Task " << task->taskName() << " worth " <<taskProgressPoints << "points!";
    moveGoalpost( taskProgressPoints, "Queued new task ");

    // Hook up events
    connect(task, SIGNAL(queueTask(AbstractWebTask*)),
            this, SLOT(queueTask(AbstractWebTask*)));
    connect(task, SIGNAL(subtaskChanged(QString)),
            this, SLOT(subtaskChanged(QString)));
    connect(task, SIGNAL(httpGet(QNetworkRequest)),
            this, SLOT(httpGet(QNetworkRequest)));
    connect(task, SIGNAL(httpHead(QNetworkRequest)),
            this, SLOT(httpHead(QNetworkRequest)));
    connect(task, SIGNAL(finished()), this, SLOT(taskFinished()));
    connect(task, SIGNAL(failed(QString)), this, SLOT(taskFailed(QString)));

    // Put the task in the queue
    if (priority) {
        // High priority - do before anything else
        highPriorityTaskQueue.enqueue(task);
    } else if (lowPriority) {
        lowPriorityQueue.enqueue(task);
    } else {
        taskQueue.enqueue(task);
    }

    TQLOG << "Queue Length now: " << taskQueue.length() + highPriorityTaskQueue.length() + lowPriorityQueue.length();

    if (startProcessing) {
        startQueueProcessing();
    }
}

void WebDataSource::startQueueProcessing() {
    if (processingQueue) {
        return;
    }

    TQLOG << "******* Start queue processing! *******";

    progressListener->show();
    processingQueue = true;

    processNextTask();
}

void WebDataSource::processNextTask() {

    if (!highPriorityTaskQueue.isEmpty()) {
        currentTask = highPriorityTaskQueue.dequeue();
    } else if (!taskQueue.isEmpty()) {
        currentTask = taskQueue.dequeue();
    } else if (!lowPriorityQueue.isEmpty()){
        currentTask = lowPriorityQueue.dequeue();
    } else {
        return;
    }

    TQLOG << "Processing task: " << currentTask->taskName();

    if (!currentTask->supertaskName().isNull()) {
        progressListener->setTaskName(currentTask->supertaskName());
    }

    makeProgress(currentTask->taskName());

    currentTask->beginTask();
}

void WebDataSource::subtaskChanged(QString name) {
    currentSubtask++;
    makeProgress(name);
}

void WebDataSource::httpGet(QNetworkRequest request)  {
    qDebug() << ":: Issuing HTTP GET for task - URL: " << request.url();
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

#if QT_VERSION < 0x050900 && QT_VERSION >= 0x050600
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

    taskQueueNetworkAccessManager->get(request);
}

void WebDataSource::httpHead(QNetworkRequest request) {
    qDebug() << ":: Issuing HTTP HEAD for task - URL: " << request.url();
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

#if QT_VERSION < 0x050900 && QT_VERSION >= 0x050600
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

    taskQueueNetworkAccessManager->head(request);
}

void WebDataSource::taskFinished() {
    TQLOG << " Task Finished: " << ((AbstractWebTask*)QObject::sender())->taskName();
    // Current task has finished its work.

    // Increment the progress dialog to skip over any skipped subtasks
    int remainingSubtasks = currentTask->subtasks() - currentSubtask;

    TQLOG << "Progress jump: " << remainingSubtasks;
    progressListener->setValue(progressListener->value() + remainingSubtasks);
    TQLOG << "Skipping subtasks";

    // Stop processing the queue
    currentTask->deleteLater();
    currentTask = 0;
    currentSubtask = 0;


    if (taskQueue.isEmpty() && highPriorityTaskQueue.isEmpty() && lowPriorityQueue.isEmpty()) {
        // And we're finished!
        progressListener->reset();
        progressListener->setMaximum(0);
        processingQueue = false;

        TQLOG << "******* Queue processing complete! *******";
    } else {
        // Still more work to do!
        processNextTask();
    }
}

void WebDataSource::taskFailed(QString error) {
    TQLOG << " Task failed: " << error;
    progressListener->hide();

    // Clear the queue - we're canceling
    while (!taskQueue.isEmpty()) {
        taskQueue.dequeue()->deleteLater();
    }

    // Toss the current task - it failed us
    if (currentTask != 0) {
        currentTask->deleteLater();
        currentTask = 0;
    }

    // No longer processing
    processingQueue = false;

    // Signal an error the only way we know - by pretending we're fetching samples
    emit sampleRetrievalError(error);
}


void WebDataSource::taskQueueResponseDataReady(QNetworkReply* reply) {
#if QT_VERSION < 0x050600
    /*
     * Reply is finished!
     * We'll ask for the reply about the Redirection attribute
     * http://doc.trolltech.com/qnetworkrequest.html#Attribute-enum
     */
    QVariant possibleRedirectUrl =
             reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    /* We'll deduct if the redirection is valid in the redirectUrl function */
    previousRedirect = redirectUrl(possibleRedirectUrl.toUrl(),
                                         previousRedirect);

    /* If the URL is not empty, we're being redirected. */
    if(!previousRedirect.isEmpty()) {
        currentTask->requestRedirected(reply->request().url().toString(), previousRedirect.toString());

        switch(reply->operation()) {
        case QNetworkAccessManager::GetOperation:
            this->taskQueueNetworkAccessManager->get(QNetworkRequest(previousRedirect));
            break;
        case QNetworkAccessManager::HeadOperation:
            this->taskQueueNetworkAccessManager->head(QNetworkRequest(previousRedirect));
            break;
        default:
            qWarning() << "Unsupported Operation" << reply->operation();
        }
    }
    else {
        /*
         * We weren't redirected anymore
         * so we arrived to the final destination...
         */

        if (currentTask != 0) {
            currentTask->networkReplyReceived(reply);
        } else {
            qDebug() << "No current task!";
            reply->deleteLater(); // A reply without an owner!
        }

        /* ...so this can be cleared. */
        previousRedirect.clear();
        return;
    }
    /* Clean up. */
    reply->deleteLater();
#else
    qDebug() << ":: Task network reply ready";
    if (currentTask != 0) {
        currentTask->networkReplyReceived(reply);
    } else {
        qDebug() << "No current task!";
        reply->deleteLater(); // A reply without an owner!
    }
#endif
}

