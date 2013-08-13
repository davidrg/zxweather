#include "webcachedb.h"

#include <QtSql>
#include <QtDebug>
#include <QDesktopServices>

#define SAMPLE_CACHE "sample-cache"
#define sampleCacheDb QSqlDatabase::database(SAMPLE_CACHE)

WebCacheDB::WebCacheDB()
{
    openDatabase();
}

WebCacheDB::~WebCacheDB() { }

void WebCacheDB::openDatabase() {
    if (QSqlDatabase::database(SAMPLE_CACHE,true).isValid())
        return; // Database is already open.


    // Try to open the database. If it doesn't exist then create it.
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", SAMPLE_CACHE);
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);

    // Make sure the target directory actually exists.
    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += "/sample-cache.db";

    qDebug() << "Cache database:" << filename;

    db.setDatabaseName(filename);
    if (!db.open()) {
        emit criticalError("Failed to open cache database");
        return;
    }


    QSqlQuery query("select * from sqlite_master "
                    "where name='db_metadata' and type='table'",
                    sampleCacheDb);
    if (!query.isActive()) {
        // TODO: Just trash the database and re-create it in this case.
        qWarning() << "Failed to determine cache version. Cache functionality "
                      "will be disabled.";
        return;
    }

    if (!query.next()) {
        createTableStructure(); // Its a blank database
    }
}

void WebCacheDB::createTableStructure() {
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
            emit criticalError("Failed to create cache structure. Error was: "
                                 + query.lastError().driverText());
            return;
        }
    }
}

int WebCacheDB::getStationId(QString stationUrl) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from station where url = :url");
    query.bindValue(":url", stationUrl);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        query.prepare("insert into station(url) values(:url)");
        query.bindValue(":url", stationUrl);
        query.exec();
        if (!query.lastError().isValid())
            return getStationId(stationUrl);
        else
            return -1; // failure
    }
}


int WebCacheDB::getDataFileId(QString dataFileUrl) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from data_file where url = :url");
    query.bindValue(":url", dataFileUrl);
    query.exec();
    if (query.first()) {
        return query.record().value(0).toInt();
    } else {
        return -1;
    }
}

int WebCacheDB::createDataFile(data_file_t dataFile, int stationId) {
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

void WebCacheDB::updateDataFile(int fileId, QDateTime lastModified, int size) {
    qDebug() << "Updating data file details...";
    QSqlQuery query(sampleCacheDb);
    query.prepare("update data_file set last_modified = :last_modified, "
                  "size = :size where id = :id");
    query.bindValue(":last_modified", lastModified.toTime_t());
    query.bindValue(":size", size);
    query.bindValue(":id", fileId);
    query.exec();

    if (query.lastError().isValid())
        qWarning() << "Failed to update data file information. Error was "
                   << query.lastError();
}

data_file_t WebCacheDB::getDataFileCacheInformation(QString dataFileUrl) {

    data_file_t dataFile;

    qDebug() << "Querying cache stats for URL" << dataFileUrl;

    QSqlQuery query(sampleCacheDb);
    query.prepare("select last_modified, size from data_file "
                  "where url =  :url");
    query.bindValue(":url", dataFileUrl);
    query.exec();
    if (query.first()) {

        QSqlRecord record = query.record();
        dataFile.filename = dataFileUrl;
        dataFile.isValid = true;
        dataFile.last_modified = QDateTime::fromTime_t(
                    record.value(0).toInt());
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

cache_stats_t WebCacheDB::getCacheStats(QString dataFileUrl) {
    cache_stats_t cacheStats;

    int fileId = getDataFileId(dataFileUrl);
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
        cacheStats.start = QDateTime::fromTime_t(record.value(0).toInt());
        cacheStats.end = QDateTime::fromTime_t(record.value(1).toInt());
        cacheStats.count = record.field(2).value().toInt();
        cacheStats.isValid = true;
    } else {
        qWarning() << "Failed to retrieve cache stats. Error was "
                   << query.lastError();
        cacheStats.isValid = false;
    }
    return cacheStats;
}

void WebCacheDB::truncateFile(int fileId) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("delete from sample where data_file = :data_file_id");
    query.bindValue(":data_file_id", fileId);
    query.exec();

    if (query.lastError().isValid())
        qWarning() << "Failed to dump expired samples. "
                      "Cache will likely become corrupt. Error was "
                   << query.lastError();
}

void WebCacheDB::cacheDataFile(data_file_t dataFile, QString stationUrl) {

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
                       dataFile.last_modified,
                       dataFile.size);
    }

    if (dataFile.expireExisting) {
        // Trash any existing samples for this file.
        truncateFile(dataFileId);
    }

    // Cool. Data file is all ready - now insert the samples.
    cacheDataSet(dataFile.samples, stationId, dataFileId);
}

void WebCacheDB::cacheDataSet(SampleSet samples,
                              int stationId,
                              int dataFileId) {
    qDebug() << "Caching dataset of" << samples.sampleCount << "samples...";
    // First up, grab the list of samples that are missing from the database.
    QVariantList timestamps, temperature, dewPoint, apparentTemperature,
            windChill, indoorTemperature, humidity, indoorHumidity, pressure,
            rainfall, stationIds, dataFileIds, averageWindSpeeds,
            gustWindSpeeds, windDirections;

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
        averageWindSpeeds.append(samples.averageWindSpeed.at(i));
        gustWindSpeeds.append(samples.gustWindSpeed.at(i));

        if (samples.windDirection.contains(timestamp))
            windDirections.append(samples.windDirection[timestamp]);
        else {
            // Append null (no wind speed at this timestamp).
            QVariant val;
            val.convert(QVariant::Int);
            windDirections.append(val);
        }

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
                    "data_file, average_wind_speed, gust_wind_speed, "
                    "wind_direction) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
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
        query.addBindValue(averageWindSpeeds);
        query.addBindValue(gustWindSpeeds);
        query.addBindValue(windDirections);
        if (!query.execBatch()) {
            qWarning() << "Sample insert failed: " << query.lastError();
        } else {
            qDebug() << "Inserted: " << query.numRowsAffected();
        }
    }
    qDebug() << "Insert finished at " << timer.elapsed()
             << "msecs. Committing transaction...";
    if (!db.commit()) {
        qWarning() << "Transaction commit failed. Data not cached.";
    }
    qDebug() << "Transaction committed at " << timer.elapsed() << "msecs";

    qDebug() << "Cache insert completed.";
}

SampleSet WebCacheDB::retrieveDataSet(QString stationUrl,
                                      QDateTime startTime,
                                      QDateTime endTime) {
    QSqlQuery query(sampleCacheDb);
    SampleSet samples;

    int stationId = getStationId(stationUrl);

    QString where_clause = "where station = :station_id "
            "and timestamp >= :start_time and timestamp <= :end_time";

    query.prepare("select count(*) from sample " + where_clause);
    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime.toTime_t());
    query.bindValue(":end_time", endTime.toTime_t());
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
                  "humidity, indoor_humidity, pressure, rainfall, "
                  "average_wind_speed, gust_wind_speed, wind_direction "
                  "from sample " + where_clause + " order by timestamp asc");

    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime.toTime_t());
    query.bindValue(":end_time", endTime.toTime_t());
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
            samples.indoorTemperature.append(record.value(5).toDouble());
            samples.humidity.append(record.value(6).toDouble());
            samples.indoorHumidity.append(record.value(7).toDouble());
            samples.pressure.append(record.value(8).toDouble());
            samples.rainfall.append(record.value(9).toDouble());
            samples.averageWindSpeed.append(record.value(10).toDouble());
            samples.gustWindSpeed.append(record.value(11).toDouble());

            if (!record.value(12).isNull()) // Wind direction can be null.
                samples.windDirection[timeStamp] = record.value(12).toUInt();

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
