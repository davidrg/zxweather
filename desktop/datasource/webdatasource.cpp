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

#include <QtSql>

#define SAMPLE_CACHE "sample-cache"
#define sampleCacheDb QSqlDatabase::database(SAMPLE_CACHE)

// TODO: make the progress dialog cancel button work.

int getStationId(QString station);
void cacheDataSet(SampleSet samples, int stationId);

QStringList getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList* dataSetQueue);

WebDataSource::WebDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractDataSource(parentWidget, parent)
{
    Settings& settings = Settings::getInstance();
    baseURL = settings.webInterfaceUrl();
    stationCode = settings.stationCode();
    liveDataUrl = QUrl(baseURL + "data/" + stationCode + "/live.json");

    rangeRequest = false;
    dataFileQueuePrep = false;

    netAccessManager.reset(new QNetworkAccessManager(this));
    connect(netAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(dataReady(QNetworkReply*)));

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
}

void WebDataSource::processData() {

    progressDialog->setLabelText("Processing...");

    SampleSet samples;

    // Split & trim the data
    QList<QDateTime> timeStamps;
    QList<QStringList> sampleParts;
    while (!allData.isEmpty()) {
        QString line = allData.takeFirst();
        QStringList parts = line.split(QRegExp("\\s+"));

        if (parts.count() < 11) continue; // invalid record.

        // Build timestamp
        QString tsString = parts.takeFirst();
        tsString += " " + parts.takeFirst();
        QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODate);

        if (timestamp >= start && timestamp <= end) {
            sampleParts.append(parts);
            timeStamps.append(timestamp);
        }
    }

    // Allocate memory for the sample set
    int size = timeStamps.count();
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

    // TODO: Delete this. Or fix it. Do something with it so it doesn't suck.
    cacheDataSet(samples, getStationId(stationCode));

    progressDialog->setValue(progressDialog->value() + 1);
    progressDialog->setLabelText("Draw...");


    if (!failedDataSets.isEmpty())
        QMessageBox::warning(0,
            "Data sets failed to download",
            "The following data sets failed to download:\n" + failedDataSets.join("\n"));


    emit samplesReady(samples);
    progressDialog->reset();
}

// ** Prepares the next data file. It initialises currentDataFile with the next
// * values from urlQueue and dataSetQueue then sends an HTTP HEAD request so
// * that the remaining fields (last modified and size) can be filled in.
// *
void WebDataSource::prepareNextDataSet() {
    currentDataFile.filename = urlQueue.takeFirst();
    currentDataFile.name = dataSetQueue.takeFirst();;
    currentDataFile.last_modified = QDateTime();
    currentDataFile.size = 0;

    qDebug() << "DataSet URL: " << currentDataFile.filename;

    progressDialog->setLabelText(currentDataFile.name + "...");

    QNetworkRequest request;
    request.setUrl(QUrl(currentDataFile.filename));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    dataFileQueuePrep = true; // ugh.

    netAccessManager->head(request);
}

void WebDataSource::rangeRequestResult(QString data) {
    rangeRequest = false;
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        QMessageBox::warning(0, "Error","JSON parsing failed for timestamp range request");
        return;
    }

    minTimestamp = QDateTime::fromString(result["oldest"].toString(), Qt::ISODate);
    maxTimestamp = QDateTime::fromString(result["latest"].toString(), Qt::ISODate);

    // Clip request range if it falls outside the valid range.
    if (start < minTimestamp) start = minTimestamp;
    if (end > maxTimestamp) end = maxTimestamp;

    urlQueue = getURLList(baseURL + "b/" + stationCode + "/", start, end, &dataSetQueue);
    progressDialog->setValue(0);
    progressDialog->setRange(0, urlQueue.count() +1);

    prepareNextDataSet();
}

void WebDataSource::dataReady(QNetworkReply* reply) {
    qDebug() << "Data ready";

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {

        if (reply->error() != QNetworkReply::ContentNotFoundError || rangeRequest) {
            QMessageBox::warning(NULL, "Download failed", reply->errorString());
            progressDialog->reset();
            dataSetQueue.clear();
            urlQueue.clear();
            return;
        } else {
            failedDataSets.append(reply->request().url().toString());
        }
    } else if (progressDialog->wasCanceled()) {
        progressDialog->reset();
        dataSetQueue.clear();
        urlQueue.clear();
        return;
    } else if (rangeRequest){
        // Range request response succeeded. Process it.
        rangeRequestResult(reply->readAll());
        return;
    } else if (dataFileQueuePrep) {
        currentDataFile.size = reply->header(
                    QNetworkRequest::ContentLengthHeader).toInt();
        currentDataFile.last_modified = reply->header(
                    QNetworkRequest::LastModifiedHeader).toDateTime();
    } else {
        // Data response succeeded. Process the data.
        while(!reply->atEnd()) {
            QString line = reply->readLine();
            if (!line.startsWith("#"))
                allData.append(line.trimmed());
        }
    }

    if (dataFileQueuePrep) {
        // We're currently preparing the data file download queue. This
        // response was a result of our HEAD request on a data file.

        // So. Chuck the current datafile on to the queue
        dataFileQueue.append(currentDataFile);

        if (urlQueue.isEmpty()) {
            // And we're out of data files to prepare.
            dataFileQueuePrep = false;
            trimDataFileQueue();

        }
    } else {
        // Request more data if there is any to request
        progressDialog->setValue(progressDialog->value() + 1);
        if(urlQueue.isEmpty()) {
            // Finished preparing everything everything.
            processData();
        } else {
            // Still more to download.
            prepareNextDataSet();
        }
    }
}

void WebDataSource::trimDataFileQueue() {
    // TODO: Go through dataFileQueue removing any entries that
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

QStringList getURLList(
        QString baseURL, QDateTime startTime, QDateTime endTime,
        QStringList* dataSetQueue) {
    QDate startDate = startTime.date();
    QDate endDate = endTime.date();

    int startYear = startDate.year();
    int startMonth = startDate.month();

    int endYear = endDate.year();
    int endMonth = endDate.month();

    QStringList urlsToFetch;

    //TODO: consider trying to make use of day-level data sources if it makes
    // sense.

    for(int year = startYear; year <= endYear; year++) {
        qDebug() << "Year:" << year;
        int month = year == startYear ? startMonth : 1;
        for(; month <= 12; month++) {

            QString monthName = monthToName(month);

            QString url = baseURL + QString::number(year) + "/" +
                    monthName + "/gnuplot_data.dat";

            urlsToFetch.append(url);
            dataSetQueue->append(monthName + " " + QString::number(year));

            // We're finished.
            if (year == endYear && month == endMonth)
                break;
        }
    }
    return urlsToFetch;
}


void WebDataSource::fetchSamples(QDateTime startTime, QDateTime endTime) {
    start = startTime;
    end = endTime;

    progressDialog->setWindowTitle("Downloading data sets...");
    progressDialog->show();

    QNetworkRequest request;
    request.setUrl(QUrl(baseURL + "data/" + stationCode + "/samplerange.json"));
    request.setRawHeader("User-Agent", Constants::USER_AGENT);

    rangeRequest = true;
    netAccessManager->get(request);
}



//--------------------------------------------------------------------------

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

//--------------------------------------------------------------------------

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

//bool WebDataSource::isCached(QString station, QDate date) {
//    // Check to see if an entry exists in the cache_entries table for this
//    // station and date combination.

//    int stationId = getStationId(station);

//    if (stationId == -1) {
//        qWarning() << "Failed to get or create station ID in cache database";
//        return false;
//    }

//    quint32 dateInt = date.year() * 10000;
//    dateInt += date.month() * 100;
//    dateInt += date.day();

//    QSqlQuery query(sampleCacheDb);
//    query.prepare("select date from cache_entries "
//                  "where station = :stationId and date = :date");
//    query.bindValue(":stationId", stationId);
//    query.bindValue(":date", dateInt);
//    query.exec();

//    if (query.first())
//        return true;

//    return false;
//}

bool sampleExistsInDatabase(int stationId, uint timestamp) {
    /* This way of deciding what needs caching is way too slow. We need another
     * solution that doesn't execute tens of thousands of selects.
     *
     * One Possible solution:
     *  - Only download data file if either:
     *     + Its not present in the DB at all
     *     + Its timestamp (use HTTP HEAD command) does not match that stored
     *       in the database
     *  - To decide what needs caching:
     *     1. Grab the min and max timestamp for that file in the DB as well
     *        as the record count between those timestamps
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
    QSqlQuery query(sampleCacheDb);
    query.prepare("select 1 from sample where station = :stationId "
                  "and timestamp = :timestamp limit 1");
    query.bindValue(":stationId", stationId);
    query.bindValue(":timestamp", timestamp);
    if (!query.exec()) {
        qWarning() << query.lastError();
        return false;
    }

    bool exists = query.next();

    //qDebug() << "sample " << timestamp << " for station " << stationId
    //         << " exists = " << exists;

    return exists;
}

/** Stores the supplied dataset in the cache database.
 *
 */
void cacheDataSet(SampleSet samples, int stationId) {
    qDebug() << "Caching dataset of" << samples.sampleCount << "samples...";
    // First up, grab the list of samples that are missing from the database.
    QVariantList timestamps, temperature, dewPoint, apparentTemperature,
            windChill, indoorTemperature, humidity, indoorHumidity, pressure,
            rainfall, stationIds;

    qDebug() << "Preparing list of samples to insert...";

    QTime timer;
    timer.start();

    for (unsigned int i = 0; i < samples.sampleCount; i++) {
        uint timestamp = samples.timestampUnix.at(i);
        if (!sampleExistsInDatabase(stationId, timestamp)) {
            stationIds << stationId; // Kinda pointless but the API wants it.
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
    }
    qDebug() << "Existance check took" << timer.elapsed() << "msecs";

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
                    "pressure, indoor_temperature, indoor_humidity, rainfall) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
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

