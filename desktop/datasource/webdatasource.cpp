#include "webdatasource.h"
#include "constants.h"
#include "settings.h"
#include "json/json.h"

#include <QMessageBox>
#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QVariantMap>
#include <QtDebug>
#include <float.h>
#include <QDateTime>
#include <QNetworkProxyFactory>

#include <QtSql>

#define SAMPLE_CACHE "sample-cache"
#define sampleCacheDb QSqlDatabase::database(SAMPLE_CACHE)

void getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList& urlList, QStringList& nameList);
void ReserveSampleSetSpace(SampleSet &samples, int size);

// Sample cache DB forward declarations
data_file_t getDataFileCacheInformation(QString url);
void cacheDataFile(data_file_t dataFile, QString stationUrl);
void cacheDataSet(SampleSet samples, int stationId, int dataFileId);
cache_stats_t getCacheStats(QString filename);
SampleSet retrieveDataSet(QString stationUrl, int startTime, int endTime);

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

    // Setup data cache thing.
    QNetworkDiskCache* cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(Settings::getInstance().dataSetCacheDir());
    netAccessManager->setCache(cache);

    // Open the sample data cache database.
    openCache();

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
 * @param startTime timestamp for the first record to return.
 * @param endTime timestamp for the last record to return
 */
void WebDataSource::fetchSamples(QDateTime startTime, QDateTime endTime) {

    // We can only process one fetch at a time right now.
    Q_ASSERT_X(download_state == DLS_READY, "fetchSamples",
               "A sample fetch is already in progress");

    setDownloadState(DLS_INIT);
    progressDialog->setWindowTitle("Downloading data sets...");
    progressDialog->setLabelText("Checking data range");
    progressDialog->show();
    dlStartTime = startTime;
    dlEndTime = endTime;

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
            QMessageBox::warning(NULL, "Download failed", reply->errorString());
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
        QMessageBox::warning(0, "Error",
                             "JSON parsing failed for timestamp range request");
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
    // Plus one for range request plus another for the final cache retrieve
    progressDialog->setRange(0, candidateURLs.count() * 4 + 2);
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
    data_file_t cache_info = getDataFileCacheInformation(url);

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
    cache_stats_t cacheStats = getCacheStats(url);
    if (cacheStats.isValid) {
        qDebug() << "File exists in cache db and contains"
                 << cacheStats.count << "records between"
                 << cacheStats.start << "and" << cacheStats.end;
    }

    data_file_t dataFile = loadDataFile(url, fileData, lastModified, size,
                                        cacheStats);

    makeProgress("Caching data for " + name);
    cacheDataFile(dataFile, stationUrl);

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

void ReserveSampleSetSpace(SampleSet& samples, int size)
{
    qDebug() << "Reserving space for" << size << "samples.";
    samples.timestampUnix.reserve(size);
    samples.sampleCount = size;
    samples.timestamp.reserve(size);
    samples.temperature.reserve(size);
    samples.dewPoint.reserve(size);
    samples.apparentTemperature.reserve(size);
    samples.windChill.reserve(size);
    samples.indoorTemperature.reserve(size);
    samples.humidity.reserve(size);
    samples.indoorHumidity.reserve(size);
    samples.pressure.reserve(size);
    samples.rainfall.reserve(size);
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
    ReserveSampleSetSpace(samples, size);

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

    int start = dlStartTime.toTime_t();
    int end = dlEndTime.toTime_t();

    return retrieveDataSet(stationUrl, start, end);
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

/*****************************************************************************
 **** SAMPLE CACHE DATABASE **************************************************
 *****************************************************************************/

void createTableStructure() {
    qDebug() << "Create cache database structure...";
    QFile createScript(":/cache_db/create.sql");
    createScript.open(QIODevice::ReadOnly);

    QString script;

    while(!createScript.atEnd()) {
        QString line = createScript.readLine();
        if (!line.startsWith("--"))
            script += line;
    }

    // This is really nasty.
    QStringList statements = script.split(";");

    QSqlQuery query(sampleCacheDb);

    foreach(QString statement, statements) {
        statement = statement.trimmed();

        qDebug() << statement;

        // The split above strips off the semicolons.
        query.exec(statement);

        if (query.lastError().isValid()) {
            QMessageBox::warning(0, "Cache warning",
                                 "Failed to create cache structure. Error was: "
                                 + query.lastError().driverText());
            return;
        }
    }
}

void WebDataSource::openCache() {

    if (QSqlDatabase::database(SAMPLE_CACHE,true).isValid())
        return; // Database is already open.


    // Try to open the database. If it doesn't exist then create it.
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", SAMPLE_CACHE);
    db.setDatabaseName("local-cache.db");
    if (!db.open()) {
        QMessageBox::critical(0, "Error", "Failed to open cache database");
        return;
    }


    QSqlQuery query("select * from sqlite_master "
                    "where name='db_metadata' and type='table'",
                    sampleCacheDb);
    if (!query.isActive()) {
        QMessageBox::warning(0, "Cache warning",
                             "Failed to determine cache version. Cache "
                             "functionality will be disabled.");
        return;
    }

    if (!query.next()) {
        createTableStructure(); // Its a blank database
    }
}




/** Gets the station ID in the cache database. If the station does not already
 * exist in the database it is created.
 *
 * @param station Station URL
 * @return Station ID for use when querying the sampe and cache_entries tables.
 */
int getStationId(QString station) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from station where url = :url");
    query.bindValue(":url", station);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        query.prepare("insert into station(url) values(:url)");
        query.bindValue(":url", station);
        query.exec();
        if (!query.lastError().isValid())
            return getStationId(station);
        else
            return -1; // failure
    }
}

/** Gets the cache file ID in the cache database. If the file does not already
 * exist then -1 is returned.
 *
 * @param filename File URL
 * @return File ID or -1 if file does not exist.
 */
int getDataFileId(QString filename) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from data_file where url = :url");
    query.bindValue(":url", filename);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        return -1;
    }
}

int createDataFile(data_file_t dataFile, int stationId) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("insert into data_file(station, url, last_modified, size) "
                  "values(:station, :url, :last_modified, :size)");
    query.bindValue(":station", stationId);
    query.bindValue(":url", dataFile.filename);
    query.bindValue(":last_modified", dataFile.last_modified.toTime_t());
    query.bindValue(":size", dataFile.size);
    query.exec();

    if (!query.lastError().isValid()) {
        return query.lastInsertId().toInt();
    } else {
        // Failed to isnert.
        qWarning() << "Failed to create data file in database. Error was: "
                   << query.lastError();
        return -1;
    }
}

void updateDataFile(int fileId, int lastModified, int size) {
    qDebug() << "Updating data file details...";
    QSqlQuery query(sampleCacheDb);
    query.prepare("update data_file set last_modified = :last_modified, "
                  "size = :size where id = :id");
    query.bindValue(":last_modified", lastModified);
    query.bindValue(":size", size);
    query.bindValue(":id", fileId);
    query.exec();

    if (query.lastError().isValid())
        qWarning() << "Failed to update data file information. Error was "
                   << query.lastError();
}

/** Gets some basic cache related information about the specified data file.
 * This is the last modified timestamp and the data files original size when
 * last downloaded.
 *
 * @param url
 * @return
 */
data_file_t getDataFileCacheInformation(QString url) {

    data_file_t dataFile;

    qDebug() << "Querying cache stats for URL" << url;

    QSqlQuery query(sampleCacheDb);
    query.prepare("select last_modified, size from data_file "
                  "where url =  :url");
    query.bindValue(":url", url);
    query.exec();
    if (query.first()) {

        QSqlRecord record = query.record();
        dataFile.filename = url;
        dataFile.isValid = true;
        dataFile.last_modified = QDateTime::fromTime_t(record.value(0).toInt());
        dataFile.size = record.value(1).toInt();

        qDebug() << "Cache stats loaded from DB:"
                 << dataFile.last_modified << dataFile.size;

        return dataFile;
    } else if (query.lastError().isValid()) {
        qWarning() << "Failed to get cache stats:" << query.lastError();
    }

    qDebug() << "URL not found in database. NO CACHE STATS AVAILABLE.";

    // Probably doesn't exist in the database.
    dataFile.isValid = false;
    return dataFile;
}


/* How caching works:
 *  - Only download data file if either:
 *     + Its not present in the DB at all
 *     + Its timestamp (use HTTP HEAD command) does not match that stored
 *       in the database
 *  - To decide what needs caching:
 *     1. Grab the min and max timestamp for that file in the DB as well
 *        as the record count.
 *     2. Ensure the record count between those timestamps matches what
 *        is in the data file. If it matches then that range does not need
 *        to be cached.
 *     3. If the range count does not match then drop the entire data file
 *        from the DB and re-cache it
 *     4. If the range count *DOES* match then cache all samples falling
 *        outside the range only.
 * This should mean we don't need to check for individual samples. It
 * should also make it easier to decide what needs downloading and what
 * doesn't.
 */

cache_stats_t getCacheStats(QString filename) {
    cache_stats_t cacheStats;

    int fileId = getDataFileId(filename);
    if (fileId == -1) {
        // File doesn't exist. No stats for you.
        cacheStats.isValid = false;
        return cacheStats;
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("select min(timestamp), max(timestamp), count(*) "
                  "from sample where data_file = :data_file_id "
                  "group by data_file");
    query.bindValue(":data_file_id", fileId);
    query.exec();

    if (query.first()) {
        QSqlRecord record = query.record();
        cacheStats.start = QDateTime::fromTime_t(record.field(0).value().toInt());
        cacheStats.end = QDateTime::fromTime_t(record.field(1).value().toInt());
        cacheStats.count = record.field(2).value().toInt();
        cacheStats.isValid = true;
    } else {
        qWarning() << "Failed to retrieve cache stats. Error was " << query.lastError();
        cacheStats.isValid = false;
    }
    return cacheStats;
}

void truncateFile(int fileId) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("delete from sample where data_file = :data_file_id");
    query.bindValue(":data_file_id", fileId);
    query.exec();

    if (query.lastError().isValid())
        qWarning() << "Failed to dump expired samples. "
                      "Cache will likely become corrupt. Error was "
                   << query.lastError();
}


void cacheDataFile(data_file_t dataFile, QString stationUrl) {

    int stationId = getStationId(stationUrl);

    int dataFileId = getDataFileId(dataFile.filename);

    if (dataFileId == -1) {
        // new file.
        dataFileId = createDataFile(dataFile, stationId);

        if (dataFileId == -1) {
            // Oops! something went wrong.
            qWarning() << "createDataFile() failed. Aborting cache store.";
            return;
        }

    } else {
        // data file exists. Update it.
        updateDataFile(dataFileId,
                       dataFile.last_modified.toTime_t(),
                       dataFile.size);
    }

    if (dataFile.expireExisting) {
        // Trash any existing samples for this file.
        truncateFile(dataFileId);
    }

    // Cool. Data file is all ready - now insert the samples.
    cacheDataSet(dataFile.samples, stationId, dataFileId);
}

/** Stores the supplied dataset in the cache database.
 *
 */
void cacheDataSet(SampleSet samples, int stationId, int dataFileId) {
    qDebug() << "Caching dataset of" << samples.sampleCount << "samples...";
    // First up, grab the list of samples that are missing from the database.
    QVariantList timestamps, temperature, dewPoint, apparentTemperature,
            windChill, indoorTemperature, humidity, indoorHumidity, pressure,
            rainfall, stationIds, dataFileIds;

    qDebug() << "Preparing list of samples to insert...";

    QTime timer;
    timer.start();

    for (unsigned int i = 0; i < samples.sampleCount; i++) {
        uint timestamp = samples.timestampUnix.at(i);            

        stationIds << stationId;   // Kinda pointless but the API wants it.
        dataFileIds << dataFileId; // Likewise

        timestamps.append(timestamp);
        temperature.append(samples.temperature.at(i));
        dewPoint.append(samples.dewPoint.at(i));
        apparentTemperature.append(samples.apparentTemperature.at(i));
        windChill.append(samples.windChill.at(i));
        indoorTemperature.append(samples.indoorTemperature.at(i));
        humidity.append(samples.humidity.at(i));
        indoorHumidity.append(samples.indoorHumidity.at(i));
        pressure.append(samples.pressure.at(i));
        rainfall.append(samples.rainfall.at(i));
    }

    // Wrapping bulk inserts in a transaction cuts total time by orders of
    // magnitude
    QSqlDatabase db = sampleCacheDb;
    db.transaction();

    qDebug() << "Inserting" << stationIds.count() << "samples...";
    timer.start();
    if (!timestamps.isEmpty()) {
        // We have data to store.
        QSqlQuery query(db);
        query.prepare(
                    "insert into sample(station, timestamp, temperature, "
                    "dew_point, apparent_temperature, wind_chill, humidity, "
                    "pressure, indoor_temperature, indoor_humidity, rainfall, "
                    "data_file) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        query.addBindValue(stationIds);
        query.addBindValue(timestamps);
        query.addBindValue(temperature);
        query.addBindValue(dewPoint);
        query.addBindValue(apparentTemperature);
        query.addBindValue(windChill);
        query.addBindValue(humidity);
        query.addBindValue(pressure);
        query.addBindValue(indoorTemperature);
        query.addBindValue(indoorHumidity);
        query.addBindValue(rainfall);
        query.addBindValue(dataFileIds);
        if (!query.execBatch()) {
            qWarning() << "Sample insert failed: " << query.lastError();
        } else {
            qDebug() << "Inserted: " << query.numRowsAffected();
        }
    }
    qDebug() << "Insert finished at " << timer.elapsed() << "msecs. Committing transaction...";
    if (!db.commit()) {
        qWarning() << "Transaction commit failed. Data not cached.";
    }
    qDebug() << "Transaction committed at " << timer.elapsed() << "msecs";

    qDebug() << "Cache insert completed.";
}

SampleSet retrieveDataSet(QString stationUrl, int startTime, int endTime) {
    QSqlQuery query(sampleCacheDb);
    SampleSet samples;

    int stationId = getStationId(stationUrl);

    QString where_clause = "where station = :station_id "
            "and timestamp >= :start_time and timestamp <= :end_time";

    query.prepare("select count(*) from sample " + where_clause);
    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime);
    query.bindValue(":end_time", endTime);
    query.exec();

    if (!query.first()) {
        qWarning() << "Failed to get sample count. Error was "
                   << query.lastError();
        return samples;
    }

    int count = query.record().field(0).value().toInt();

    qDebug() << "There are" << count << "samples within the date range:"
             << startTime << "to" << endTime;

    ReserveSampleSetSpace(samples, count);

    query.prepare("select timestamp, temperature, dew_point, "
                  "apparent_temperature, wind_chill, indoor_temperature, "
                  "humidity, indoor_humidity, pressure, rainfall "
                  "from sample " + where_clause + " order by timestamp asc");

    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime);
    query.bindValue(":end_time", endTime);
    query.exec();

    if (query.first()) {
        // At least one record came back. Go pull all of them out and dump
        // them in the SampleSet.
        do {
            QSqlRecord record = query.record();

            int timeStamp = record.value(0).toInt();
            samples.timestampUnix.append(timeStamp);
            samples.timestamp.append(timeStamp);

            samples.temperature.append(record.value(1).toDouble());
            samples.dewPoint.append(record.value(2).toDouble());
            samples.apparentTemperature.append(record.value(3).toDouble());
            samples.windChill.append(record.value(4).toDouble());
            samples.humidity.append(record.value(5).toDouble());
            samples.pressure.append(record.value(6).toDouble());
            samples.indoorTemperature.append(record.value(7).toDouble());
            samples.indoorHumidity.append(record.value(8).toDouble());
            samples.rainfall.append(record.value(8).toDouble());
        } while (query.next());
    } else if (query.lastError().isValid()) {
        qWarning() << "Failed to get sample set. Error was "
                   << query.lastError();
        SampleSet blank;
        return blank;
    } else {
        qDebug() << "Apparently there were no samples for the time range. "
                    "Cache store failed?";
    }


    return samples;
}

