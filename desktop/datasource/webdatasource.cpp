#include "webdatasource.h"
#include "constants.h"
#include "settings.h"
#include "json/json.h"
#include "webcachedb.h"

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
    stationUrl = baseURL + "b/" + stationCode + "/";

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    netAccessManager.reset(new QNetworkAccessManager(this));
    connect(netAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(requestFinished(QNetworkReply*)));

    // Setup live data functionality
    liveNetAccessManager.reset(new QNetworkAccessManager(this));
    connect(liveNetAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(liveDataReady(QNetworkReply*)));
    livePollTimer.setInterval(30000);
    connect(&livePollTimer, SIGNAL(timeout()), this, SLOT(liveDataPoll()));

    setDownloadState(DLS_READY);
}

void WebDataSource::makeProgress(QString message) {
    progressDialog->setLabelText(message);
    int value = progressDialog->value() + 1;
    progressDialog->setValue(value);
    qDebug() << "Progress [" << value << "]:" << message;
}

void WebDataSource::dlReset() {
    candidateURLs.clear();
    acceptedURLs.clear();
    urlNames.clear();
    dlStartTime = QDateTime();
    dlEndTime = QDateTime();
    progressDialog->reset();
    setDownloadState(DLS_READY);
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

    // We can only process one fetch at a time right now.
    Q_ASSERT_X(download_state == DLS_READY, "fetchSamples",
               "A sample fetch is already in progress");

    setDownloadState(DLS_INIT);
    progressDialog->setWindowTitle("Downloading data sets...");
    progressDialog->setLabelText("Checking data range");
    progressDialog->show();
    dlStartTime = startTime;
    dlEndTime = endTime;
    columnsToReturn = columns;
    returnAggregate = aggregateFunction;
    returnGroupType = groupType;
    returnGroupMinutes = groupMinutes;


    QUrl rangeRequestUrl = buildRangeRequestURL();
    qDebug() << "Range request:" << rangeRequestUrl;

    QNetworkRequest request(rangeRequestUrl);
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    setDownloadState(DLS_RANGE_REQUEST);
    netAccessManager->get(request);
}

/** Builds the URL for the current stations Sample Range resource.
 *
 * @return URL for a resource that contains sample range data for the current
 * weather station
 */
QUrl WebDataSource::buildRangeRequestURL() const {
    QString url = baseURL + "data/" + stationCode + "/samplerange.json";
    return QUrl(url);
}

QString monthToName(int month) {
    switch (month) {
    case 1: return "january";
    case 2: return "february";
    case 3: return "march";
    case 4: return "april";
    case 5: return "may";
    case 6: return "june";
    case 7: return "july";
    case 8: return "august";
    case 9: return "september";
    case 10: return "october";
    case 11: return "november";
    case 12: return "december";
    default: return "";
    }
}

void WebDataSource::requestFinished(QNetworkReply *reply) {

    // TODO: Error handling
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::ContentNotFoundError ||
                download_state == DLS_RANGE_REQUEST) {
            // A fatalish error. A 404 on the range request means we have to
            // abort but any other time its fine. As such we only get here if
            // its either a range request 404 or its some other error we don't
            // check for.
            emit sampleRetrievalError(reply->errorString());
            dlReset();
            return;
        } else {
            qDebug() << "Download failed:" << reply->request().url().toString();
//            failedDataSets.append(reply->request().url().toString());
        }
    }

    qDebug() << "Network reply arrived for URL" << reply->url();

    switch(download_state) {
    case DLS_RANGE_REQUEST:
        rangeRequestFinished(reply);
        break;
    case DLS_CHECK_CACHE:
        cacheStatusRequestFinished(reply);
        break;
    case DLS_DOWNLOAD_DATA:
        downloadRequestFinished(reply);
        break;
    default:
        qWarning() << "Received unexpected reply in state " << download_state;
    }

    reply->deleteLater();
}

/** Handles the range request response. This contains the earliest and latest
 * time stamps available from the remote server. This information is used to
 * clip the requested date range to what is available on the server. It then
 * generates all candidate URLs and kicks off the cache status requests.
 *
 * @param reply Network reply containing the range request data.
 */
void WebDataSource::rangeRequestFinished(QNetworkReply *reply) {
    QString data = reply->readAll();

    using namespace QtJson;

    bool ok;

    qDebug() << "Range request completed.";

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        emit sampleRetrievalError("JSON parsing failed for timestamp range "
                                  "request. Download aborted.");
        qWarning() << "Failed parsing JSON response from timestamp range request";

        // abort.
        dlReset();
        return;
    }

    QDateTime minTimestamp = QDateTime::fromString(
                result["oldest"].toString(), Qt::ISODate);
    QDateTime maxTimestamp = QDateTime::fromString(
                result["latest"].toString(), Qt::ISODate);

    qDebug() << "Valid time range on remote server is"
             << minTimestamp << "to" << maxTimestamp;

    // If the requested range is greater than what the server can provide then
    // clip it to what the server has available.
    if (dlStartTime < minTimestamp) dlStartTime = minTimestamp;
    if (dlEndTime > maxTimestamp) dlEndTime = maxTimestamp;

    QStringList names;
    getURLList(stationUrl,
               dlStartTime,
               dlEndTime,
               candidateURLs /*OUT*/,
               names /*OUT*/
               );

    // Chuck the names in a hashtable for use later.
    for (int i = 0; i < candidateURLs.count(); i++) {
        QString url = candidateURLs[i];
        QString name = names[i];
        urlNames[url] = name;
    }

    // 4 for each url (cache check, download, process, cache store)
    // Plus one for range request plus another for the final cache retrieve.
    // Plus one more for good luck because the dialog was disappearing before
    // the dataset was selected.
    progressDialog->setRange(0, candidateURLs.count() * 4 + 3);
    progressDialog->setValue(1);

    setDownloadState(DLS_CHECK_CACHE);
    makeNextCacheStatusRequest();
}

void WebDataSource::makeNextCacheStatusRequest() {

    QString url = candidateURLs.takeFirst();
    QString name = urlNames[url];

    qDebug() << "Checking cache status of" << name << "[" << url << "]...";

    makeProgress("Checking cache status of " + name);

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    netAccessManager->head(request);
}

void WebDataSource::cacheStatusRequestFinished(QNetworkReply *reply) {

    QString url = reply->request().url().toString();
    data_file_t cache_info =
            WebCacheDB::getInstance().getDataFileCacheInformation(url);

    qDebug() << "Cache status request for url [" << url << "] finished.";

    if (reply->hasRawHeader("X-Cache-Lookup")) {
        QString upstreamStatus = QString(reply->rawHeader("X-Cache-Lookup"));
        qDebug() << "Upstream cache status: " << upstreamStatus;
        // Squid inserts headers containing strings such as:
        // HIT from gatekeeper.zx.net.nz:3128
    }

    QDateTime lastModified = reply->header(
                QNetworkRequest::LastModifiedHeader).toDateTime();
    qDebug() << "File on server was last modified" << lastModified;

    if (lastModified != cache_info.last_modified) {
        // Last modified date has changed. We need to investigate
        // further. I used to check content-length here too but something kept
        // resetting it to zero on HEAD requests (likely just when using gzip)
        // so it doesn't seem a reliable option.
        qDebug() << "Last modified date changed (database is"
                 << cache_info.last_modified << "). Full download required.";
        acceptedURLs.append(url);
    } else {
        // else the data file we have cached sounds the same as what is on the
        // server. We won't bother redownloading it.

        // We won't be downloading, processing or caching anything for this
        // file so we can skip forward a bit.
        qDebug() << "Cache copy seems ok. Skiping download.";
        progressDialog->setValue(progressDialog->value() + 3);
    }

    if (!candidateURLs.isEmpty())
        makeNextCacheStatusRequest();
    else {
        // No more cache status checks to perform.
        if (!acceptedURLs.isEmpty()) {
            qDebug() << "Cache checks finished. Beginning download...";
            setDownloadState(DLS_DOWNLOAD_DATA);
            makeNextDataRequest();
        } else {
            qDebug() << "Nothing to download.";
            completeDataRequest();
        }
    }
}

void WebDataSource::makeNextDataRequest() {
    QString url = acceptedURLs.takeFirst();
    QString name = urlNames[url];

    qDebug() << "Downloading data for" << name << "[" << url << "]";
    makeProgress("Downloading data for " + name);

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    netAccessManager->get(request);
}

void WebDataSource::downloadRequestFinished(QNetworkReply *reply) {
    QStringList fileData;

    QString url = reply->url().toString();
    QString name = urlNames[url];

    qDebug() << "Download completed for" << name << "[" << url << "]";

    while (!reply->atEnd()) {
        QString line = reply->readLine().trimmed();
        if (!line.startsWith("#"))
            fileData.append(line);
    }

    qDebug() << "File contains" << fileData.count() << "records.";

    QDateTime lastModified = reply->header(
                QNetworkRequest::LastModifiedHeader).toDateTime();
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();


    qDebug() << "Checking db cache status...";
    cache_stats_t cacheStats = WebCacheDB::getInstance().getCacheStats(url);
    if (cacheStats.isValid) {
        qDebug() << "File exists in cache db and contains"
                 << cacheStats.count << "records between"
                 << cacheStats.start << "and" << cacheStats.end;
    }

    data_file_t dataFile = loadDataFile(url, fileData, lastModified, size,
                                        cacheStats);

    makeProgress("Caching data for " + name);
    WebCacheDB::getInstance().cacheDataFile(dataFile, stationUrl);

    if (!acceptedURLs.isEmpty())
        makeNextDataRequest();
    else {
        // No more data to request. Time to finish up.
        completeDataRequest();
    }
}

void WebDataSource::completeDataRequest() {
    makeProgress("Selecting dataset");
    SampleSet data = selectRequestedData();
    emit samplesReady(data);
    dlReset();
    // Done!
}

data_file_t WebDataSource::loadDataFile(QString url, QStringList fileData,
                                        QDateTime lastModified, int fileSize,
                                        cache_stats_t cacheStats) {
    QString name = urlNames[url];
    makeProgress("Processing data for " + name);

    SampleSet samples;

    // Split & trim the data
    QList<QDateTime> timeStamps;
    QList<QStringList> sampleParts;

    QList<QDateTime> ignoreTimeStamps;
    QList<QStringList> ignoreSampleParts;

    while (!fileData.isEmpty()) {
        QString line = fileData.takeFirst();
        QStringList parts = line.split(QRegExp("\\s+"));

        if (parts.count() < 11) continue; // invalid record.

        // Build timestamp
        QString tsString = parts.takeFirst();
        tsString += " " + parts.takeFirst();
        QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODate);

        if (!cacheStats.isValid) {
            // No ignore range. Let it through.
            sampleParts.append(parts);
            timeStamps.append(timestamp);
        } else if (timestamp < cacheStats.start || timestamp > cacheStats.end) {
            // Timestamp falls outside the ignore range.
            sampleParts.append(parts);
            timeStamps.append(timestamp);
        } else {
            // We're apparently supposed to ignore these
            ignoreSampleParts.append(parts);
            ignoreTimeStamps.append(timestamp);
        }
    }

    // Now we need to decide what to do with the stuff we're supposed to
    // ignore. Will we really ignore it?

    bool expireCache = false;

    if (ignoreTimeStamps.count() == cacheStats.count) {
        // There is the same number of records between those timestamps in
        // both the cache database and the data file. Probably safe to assume
        // none of them were changed so we'll just ignore them.
        ignoreTimeStamps.clear();
        ignoreSampleParts.clear();
    } else {
        // One or more samples were added or removed between the date ranges
        // available in the cache database. We'll take the downloaded data
        // file as authorative and dump what we currently have in the database
        // for this file.
        sampleParts.append(ignoreSampleParts);
        timeStamps.append(ignoreTimeStamps);
        expireCache = true;
    }

    // Allocate memory for the sample set
    int size = timeStamps.count();
    ReserveSampleSetSpace(samples, size, ALL_SAMPLE_COLUMNS);

    while (!timeStamps.isEmpty()) {

        uint timestamp = timeStamps.takeFirst().toTime_t();
        samples.timestamp.append(timestamp);
        samples.timestampUnix.append(timestamp);

        QStringList values = sampleParts.takeFirst();

        samples.temperature.append(values.takeFirst().toDouble());
        samples.dewPoint.append(values.takeFirst().toDouble());
        samples.apparentTemperature.append(values.takeFirst().toDouble());
        samples.windChill.append(values.takeFirst().toDouble());
        samples.humidity.append(values.takeFirst().toDouble());
        samples.pressure.append(values.takeFirst().toDouble());
        samples.indoorTemperature.append(values.takeFirst().toDouble());
        samples.indoorHumidity.append(values.takeFirst().toDouble());
        samples.rainfall.append(values.takeFirst().toDouble());
        samples.averageWindSpeed.append(values.takeFirst().toDouble());
        samples.gustWindSpeed.append(values.takeFirst().toDouble());

        QVariant val = values.takeFirst();
        if (val != "None")
            samples.windDirection[timestamp] = val.toUInt();
    }

    data_file_t dataFile;
    dataFile.filename = url;
    dataFile.isValid = true;
    dataFile.last_modified = lastModified;
    dataFile.size = fileSize;
    dataFile.samples = samples;
    dataFile.expireExisting = expireCache;

    return dataFile;
}

SampleSet WebDataSource::selectRequestedData() {
    // TODO: fetch the requested data from the sample cache and return it.

    SampleSet samples = WebCacheDB::getInstance().retrieveDataSet(
                stationUrl,
                dlStartTime,
                dlEndTime,
                columnsToReturn,
                returnAggregate,
                returnGroupType,
                returnGroupMinutes);
    qDebug() << "Got" << samples.timestamp.count() << "samples back";
    return samples;
}

void getURLList(QString baseURL, QDateTime startTime, QDateTime endTime,
                QStringList& urlList, QStringList& nameList) {
    QDate startDate = startTime.date();
    QDate endDate = endTime.date();

    int startYear = startDate.year();
    int startMonth = startDate.month();

    int endYear = endDate.year();
    int endMonth = endDate.month();

    //TODO: consider trying to make use of day-level data sources if it makes
    // sense.

    for(int year = startYear; year <= endYear; year++) {
        qDebug() << "Year:" << year;
        int month = year == startYear ? startMonth : 1;
        for(; month <= 12; month++) {

            QString monthName = monthToName(month);

            QString url = baseURL + QString::number(year) + "/" +
                    monthName + "/gnuplot_data.dat";

            urlList.append(url);
            nameList.append(monthName + " " + QString::number(year));

            // We're finished.
            if (year == endYear && month == endMonth)
                break;
        }
    }
}

/*****************************************************************************
 **** LIVE DATA **************************************************************
 *****************************************************************************/

void WebDataSource::liveDataReady(QNetworkReply *reply) {
    using namespace QtJson;

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
        }

        // Indoor data is not currently available from the website data feed.
        lds.indoorDataAvailable = false;

        emit liveData(lds);
    }
}

void WebDataSource::liveDataPoll() {
    QNetworkRequest request;
    request.setUrl(liveDataUrl);
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    liveNetAccessManager->get(request);
}

void WebDataSource::enableLiveData() {
    liveDataPoll();
    livePollTimer.start();
}

hardware_type_t WebDataSource::getHardwareType() {
    return HW_GENERIC;
}
