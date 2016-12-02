#include "webdatasource.h"
#include "constants.h"
#include "settings.h"
#include "json/json.h"
#include "webcachedb.h"

#include "webtasks/fetchsampleswebtask.h"
#include "webtasks/activeimagesourceswebtask.h"

#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QVariantMap>
#include <QtDebug>
#include <float.h>
#include <QDateTime>
#include <QNetworkProxyFactory>


void getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList& urlList, QStringList& nameList);

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
WebDataSource::WebDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
    Settings& settings = Settings::getInstance();
    baseURL = settings.webInterfaceUrl();
    stationCode = settings.stationCode();
    liveDataUrl = QUrl(baseURL + "data/" + stationCode + "/live.json");

#ifdef USE_GNUPLOT_DATA
    stationUrl = baseURL + "b/" + stationCode + "/";
#else
    stationUrl = baseURL + "data/" + stationCode + "/";
#endif

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Setup live data functionality
    liveNetAccessManager.reset(new QNetworkAccessManager(this));
    connect(liveNetAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(liveDataReady(QNetworkReply*)));
    livePollTimer.setInterval(30000);
    stationConfigLoaded = false;
    connect(&livePollTimer, SIGNAL(timeout()), this, SLOT(liveDataPoll()));

    /*******************
     * Task Queue Stuff
     *//////////////////

    taskQueueNetworkAccessManager.reset(new QNetworkAccessManager(this));
    connect(taskQueueNetworkAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(taskQueueResponseDataReady(QNetworkReply*)));
    processingQueue = false;
    currentTask = 0;
    currentSubtask = 0;
}

void WebDataSource::fireSamplesReady(SampleSet samples) {
    emit samplesReady(samples);
}

void WebDataSource::makeProgress(QString message) {
    progressDialog->setLabelText(message);
    int value = progressDialog->value() + 1;
    int maxValue = progressDialog->maximum();
    progressDialog->setValue(value);
    qDebug() << "Progress [" << value << "/" << maxValue << "]:" << message;
}


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
                this
    );

    queueTask(task);
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

        if (stationData["code"].toString() == stationCode) {
            station_name = stationData["name"].toString();
            isSolarDataAvailable = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString();

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
        emit stationName(station_name);
        emit isSolarDataEnabled(isSolarDataAvailable);
    }

    liveDataPoll();
}

void WebDataSource::liveDataReady(QNetworkReply *reply) {
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
    }
}

void WebDataSource::liveDataPoll() {

    if (!stationConfigLoaded) {
        QNetworkRequest request;
        request.setUrl(baseURL + "data/sysconfig.json");
        request.setRawHeader("User-Agent", Constants::USER_AGENT);

        liveNetAccessManager->get(request);
    } else {
        QNetworkRequest request;
        request.setUrl(liveDataUrl);
        request.setRawHeader("User-Agent", Constants::USER_AGENT);

        liveNetAccessManager->get(request);
    }
}

void WebDataSource::enableLiveData() {
    liveDataPoll();
    livePollTimer.start();
}

hardware_type_t WebDataSource::getHardwareType() {
    // we cant determine the hardware type on demand like this. We won't
    // know until after live data is turned on or we've processed a request
    // for samples.
    return HW_GENERIC;
}

/*****************************************************************************
 **** IMAGES *****************************************************************
 *****************************************************************************/



void WebDataSource::fetchImageDateList() {
    // TODO
}

void WebDataSource::fetchImageList(QDate date, QString imageSourceCode) {
    // TODO
}

void WebDataSource::fetchImage(int imageId) {
    // TODO
}

void WebDataSource::fetchThumbnails(QList<int> imageIds) {
    // TODO
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
    // TODO
}

void WebDataSource::moveGoalpost(int distance, QString reason) {

    int curVal = progressDialog->value();
    int curMax = progressDialog->maximum();

    qDebug() << "Antiprogress: [" << curVal << "/" << curMax << "] to ["
             << curVal << "/" << curMax + distance << "] - " << reason;

    progressDialog->setMaximum(curMax + distance);
}

void WebDataSource::queueTask(AbstractWebTask *task) {

    if (!processingQueue) {
        progressDialog->reset();
        progressDialog->setMaximum(task->subtasks() + 1);
    } else {
        // Move the goalposts - +1 for the task itself
        qDebug() << "Task " << task->taskName() << " worth " << task->subtasks() + 1 << "points!";
        moveGoalpost( task->subtasks() + 1, "Queued new task ");
    }

    qDebug() << ":::: Queuing Task: " << task->taskName();

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
    taskQueue.enqueue(task);

    qDebug() << ":: Queue Length now: " << taskQueue.length();

    startQueueProcessing();
}

void WebDataSource::startQueueProcessing() {
    if (processingQueue) {
        return;
    }

    progressDialog->show();
    processingQueue = true;


    processNextTask();
}

void WebDataSource::processNextTask() {

    // One progress item per task in our queue
    currentTask = taskQueue.dequeue();

    qDebug() << ":::: Processing task: " << currentTask->taskName();

    if (!currentTask->supertaskName().isNull()) {
        progressDialog->setWindowTitle(currentTask->supertaskName());
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
    taskQueueNetworkAccessManager->get(request);
}
void WebDataSource::httpHead(QNetworkRequest request) {
    qDebug() << ":: Issuing HTTP HEAD for task - URL: " << request.url();
    request.setRawHeader("User-Agent", Constants::USER_AGENT);
    taskQueueNetworkAccessManager->head(request);
}

void WebDataSource::taskFinished() {
    qDebug() << ":: Task Finished: " << ((AbstractWebTask*)QObject::sender())->taskName();
    // Current task has finished its work.

    // Increment the progress dialog to skip over any skipped subtasks
    int remainingSubtasks = currentTask->subtasks() - currentSubtask;

    qDebug() << "Progress jump: " << remainingSubtasks;
    progressDialog->setValue(progressDialog->value() + remainingSubtasks);
    qDebug() << "Progress [" << progressDialog->value() << "/"
             << progressDialog->maximum() << "]:" << "Skipping subtasks";

    // Stop processing the queue
    currentTask->deleteLater();
    currentTask = 0;
    currentSubtask = 0;


    if (taskQueue.isEmpty()) {
        // And we're finished!
        progressDialog->reset();
        processingQueue = false;

        qDebug() << "******* Queue processing complete!";
    } else {
        // Still more work to do!
        processNextTask();
    }
}

void WebDataSource::taskFailed(QString error) {
    qDebug() << ":: Task failed: " << error;
    progressDialog->hide();

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
    qDebug() << ":: Task network reply ready";
    if (currentTask != 0) {
        currentTask->networkReplyReceived(reply);
    } else {
        qDebug() << "No current task!";
        reply->deleteLater(); // A reply without an owner!
    }
}
