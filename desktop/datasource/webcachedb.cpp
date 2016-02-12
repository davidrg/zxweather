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
    cacheDataSet(dataFile.samples, stationId, dataFileId, dataFile.hasSolarData);
}

void WebCacheDB::cacheDataSet(SampleSet samples,
                              int stationId,
                              int dataFileId,
                              bool hasSolarData) {
    qDebug() << "Caching dataset of" << samples.sampleCount << "samples...";
    // First up, grab the list of samples that are missing from the database.
    QVariantList timestamps, temperature, dewPoint, apparentTemperature,
            windChill, indoorTemperature, humidity, indoorHumidity, pressure,
            rainfall, stationIds, dataFileIds, averageWindSpeeds,
            gustWindSpeeds, windDirections, solarRadiations, uvIndexes;

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

        if (hasSolarData) {
            solarRadiations.append(samples.solarRadiation.at(i));
            uvIndexes.append(samples.uvIndex.at(i));
        } else {
            // The lists still need to be populated for the query. As we don't
            // have any real data to put in them we'll use 0.
            solarRadiations.append(0);
            uvIndexes.append(0);
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
                    "wind_direction, solar_radiation, uv_index) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
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
        query.addBindValue(solarRadiations);
        query.addBindValue(uvIndexes);
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

QString WebCacheDB::buildColumnList(SampleColumns columns, QString format) {
    QString query;
    if (columns.testFlag(SC_Timestamp))
        query += format.arg("timestamp");
    if (columns.testFlag(SC_Temperature))
        query += format.arg("temperature");
    if (columns.testFlag(SC_DewPoint))
        query += format.arg("dew_point");
    if (columns.testFlag(SC_ApparentTemperature))
        query += format.arg("apparent_temperature");
    if (columns.testFlag(SC_WindChill))
        query += format.arg("wind_chill");
    if (columns.testFlag(SC_IndoorTemperature))
        query += format.arg("indoor_temperature");
    if (columns.testFlag(SC_IndoorHumidity))
        query += format.arg("indoor_humidity");
    if (columns.testFlag(SC_Humidity))
        query += format.arg("humidity");
    if (columns.testFlag(SC_Pressure))
        query += format.arg("pressure");
    if (columns.testFlag(SC_AverageWindSpeed))
        query += format.arg("average_wind_speed");
    if (columns.testFlag(SC_GustWindSpeed))
        query += format.arg("gust_wind_speed");
    if (columns.testFlag(SC_Rainfall))
        query += format.arg("rainfall");
    if (columns.testFlag(SC_WindDirection))
        query += format.arg("wind_direction");
    if (columns.testFlag(SC_SolarRadiation))
        query += format.arg("solar_radiation");
    if (columns.testFlag(SC_UV_Index))
        query += format.arg("uv_index");
    return query;
}

QString WebCacheDB::buildSelectForColumns(SampleColumns columns)
{
    QString selectPart = "select timestamp";

    selectPart += buildColumnList(columns & ~SC_Timestamp, ", %1");

    return selectPart;
}

int WebCacheDB::getNonAggregatedRowCount(int stationId, QDateTime startTime, QDateTime endTime) {
    QString qry = "select count(*) from sample where station = :station_id "
            "and timestamp >= :start_time and timestamp <= :end_time";

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);
    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime.toTime_t());
    query.bindValue(":end_time", endTime.toTime_t());
    query.exec();

    if (!query.first()) {
        qWarning() << "Failed to get sample count. Error was "
                   << query.lastError();
        return -1;
    }

    return query.record().field(0).value().toInt();
}

int WebCacheDB::getAggregatedRowCount(int stationId,
                                      QDateTime startTime,
                                      QDateTime endTime,
                                      AggregateFunction aggregateFunction,
                                      AggregateGroupType groupType,
                                      uint32_t groupMinutes) {

    qDebug() << "Aggregate Function: " << aggregateFunction;
    qDebug() << "Group Type: " << groupType;
    if (groupType == AGT_Custom) {
        qDebug() << "Custom group minutes: " << groupMinutes;
    }

    QString qry = buildAggregatedSelect(SC_NoColumns,
                                        aggregateFunction,
                                        groupType);

    qry = "select count(*) as cnt from ( " + qry + " ) as x ";

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);

    query.bindValue(":station_id", stationId);
    query.bindValue(":stationIdB", stationId);
    query.bindValue(":stationIdC", stationId);
    query.bindValue(":start_time", startTime.toTime_t());
    query.bindValue(":end_time", endTime.toTime_t());

    if (groupType == AGT_Custom) {
        query.bindValue(":groupSeconds", groupMinutes * 60);
    }

    qDebug() << "\n\nQuery: " << qry;

    query.exec();

    if (!query.first()) {
        qWarning() << "Failed to get sample count. Error was "
                   << query.lastError();
        qDebug() << query.boundValues();
        return -1;
    }

    return query.record().field(0).value().toInt();
}

QSqlQuery WebCacheDB::buildBasicSelectQuery(SampleColumns columns) {
    QString sql = buildSelectForColumns(columns);
    sql += " from sample where station = :station_id "
             "and timestamp >= :start_time and timestamp <= :end_time"
             " order by timestamp asc";

    QSqlQuery query(sampleCacheDb);
    query.prepare(sql);
    return query;
}

QString WebCacheDB::buildAggregatedSelect(SampleColumns columns,
                                          AggregateFunction function,
                                          AggregateGroupType groupType) {

    QString fn = "";
    if (function == AF_Average)
        fn = "avg";
    if (function == AF_Maximum)
        fn = "max";
    if (function == AF_Minimum)
        fn = "min";
    if (function == AF_Sum || function == AF_RunningTotal)
        fn = "sum";

    /*
     * SQLite doesn't support window functions so for AF_RunningTotal we'll
     * just compute a Sum here for each grouping and calculate the running
     * total manually in C++ land later.
     */

    QString query = "select iq.quadrant as quadrant ";

    if (columns.testFlag(SC_Timestamp))
        query += ", min(iq.timestamp) as timestamp ";

    query += buildColumnList(columns & ~SC_Timestamp, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"));
    query += " from (select ";

    if (groupType == AGT_Custom) {
        query += "(cur.timestamp / :groupSeconds) AS quadrant ";
    } else if (groupType == AGT_Month) {
        query += "strftime('%s', julianday(datetime(cur.timestamp, 'unixepoch', 'start of month'))) as quadrant";
        // In postgres this would be: extract(epoch from date_trunc('month', cur.time_stamp))::integer as quadrant
    } else { // year
        query += "strftime('%s', julianday(datetime(cur.timestamp, 'unixepoch', 'start of year'))) as quadrant";
        // In postgres this would be: extract(epoch from date_trunc('year', cur.time_stamp))::integer as quadrant
    }

    query += buildColumnList(columns, ", cur.%1 ");

    query += " from sample cur, sample prev"
             " where cur.timestamp <= :end_time"
             " and cur.timestamp >= :start_time"
             " and prev.timestamp = (select max(timestamp) from sample where timestamp < cur.timestamp"
             "        and station = :station_id )"
             " and cur.station = :stationIdB "
             " and prev.station = :stationIdC "
             " order by cur.timestamp asc) as iq "
             " group by iq.quadrant "
             " order by iq.quadrant asc ";

    /*
      resulting query parameters are:
      :stationId  \
      :stationIdB  = These must be the same value.
      :stationIdC /
      :startTime
      :endTime
      :groupSeconds (only if AGT_Custom)
     */

    qDebug() << query;

    return query;

}

QSqlQuery WebCacheDB::buildAggregatedSelectQuery(SampleColumns columns,
                                                 int stationId,
                                                 AggregateFunction aggregateFunction,
                                                 AggregateGroupType groupType,
                                                 uint32_t groupMinutes) {

    qDebug() << "Aggregate Function: " << aggregateFunction;
    qDebug() << "Group Type: " << groupType;
    if (groupType == AGT_Custom) {
        qDebug() << "Custom group minutes: " << groupMinutes;
    }

    QString qry = buildAggregatedSelect(columns,
                                        aggregateFunction,
                                        groupType);

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);

    query.bindValue(":stationIdB", stationId);
    query.bindValue(":stationIdC", stationId);

    if (groupType == AGT_Custom)
        query.bindValue(":groupSeconds", groupMinutes * 60);

    return query;
}

SampleSet WebCacheDB::retrieveDataSet(QString stationUrl,
                                      QDateTime startTime,
                                      QDateTime endTime,
                                      SampleColumns columns,
                                      AggregateFunction aggregateFunction,
                                      AggregateGroupType aggregateGroupType,
                                      uint32_t groupMinutes) {
    QSqlQuery query(sampleCacheDb);
    SampleSet samples;

    int stationId = getStationId(stationUrl);

    int count;

    if (aggregateFunction == AF_None || aggregateGroupType == AGT_None) {
        count = getNonAggregatedRowCount(stationId, startTime, endTime);
    } else {
        count = getAggregatedRowCount(stationId, startTime, endTime,
                                      aggregateFunction, aggregateGroupType,
                                      groupMinutes);
    }

    if (count == -1) {
        return samples; /* error */
    }

    qDebug() << "There are" << count << "samples within the date range:"
             << startTime << "to" << endTime;

    ReserveSampleSetSpace(samples, count, columns);

    if (aggregateFunction == AF_None || aggregateGroupType == AGT_None) {
        query = buildBasicSelectQuery(columns);
    } else {
        // Aggregated queries always require the timestamp column.
        query = buildAggregatedSelectQuery(columns | SC_Timestamp, stationId,
                                           aggregateFunction,
                                           aggregateGroupType, groupMinutes);
    }

    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", startTime.toTime_t());
    query.bindValue(":end_time", endTime.toTime_t());
    query.exec();

    if (query.first()) {

        double previousRainfall = 0.0;

        // At least one record came back. Go pull all of them out and dump
        // them in the SampleSet.
        do {
            QSqlRecord record = query.record();

            int timeStamp = record.value("timestamp").toInt();
            samples.timestampUnix.append(timeStamp);
            samples.timestamp.append(timeStamp);

            if (columns.testFlag(SC_Temperature))
                samples.temperature.append(
                            record.value("temperature").toDouble());

            if (columns.testFlag(SC_DewPoint))
                samples.dewPoint.append(record.value("dew_point").toDouble());

            if (columns.testFlag(SC_ApparentTemperature))
                samples.apparentTemperature.append(
                            record.value("apparent_temperature").toDouble());

            if (columns.testFlag(SC_WindChill))
                samples.windChill.append(
                            record.value("wind_chill").toDouble());

            if (columns.testFlag(SC_IndoorTemperature))
                samples.indoorTemperature.append(
                            record.value("indoor_temperature").toDouble());

            if (columns.testFlag(SC_Humidity))
                samples.humidity.append(record.value("humidity").toDouble());

            if (columns.testFlag(SC_IndoorHumidity))
                samples.indoorHumidity.append(
                            record.value("indoor_humidity").toDouble());

            if (columns.testFlag(SC_Pressure))
                samples.pressure.append(record.value("pressure").toDouble());

            if (columns.testFlag(SC_Rainfall)) {
                double value = record.value("rainfall").toDouble();

                // Because SQLite doesn't support window functions we have to
                // calculate the running total manually. We'll only bother doing
                // it for rainfall as it doesn't really make sense for the rest
                // (the PostgreSQL backend supports it on everyhing only because
                // its easier than doing it on one column only).
                if (aggregateFunction == AF_RunningTotal) {
                    previousRainfall += value;
                    samples.rainfall.append(previousRainfall);
                } else {
                    samples.rainfall.append(value);
                }
            }

            if (columns.testFlag(SC_AverageWindSpeed))
                samples.averageWindSpeed.append(
                            record.value("average_wind_speed").toDouble());

            if (columns.testFlag(SC_GustWindSpeed))
                samples.gustWindSpeed.append(
                            record.value("gust_wind_speed").toDouble());

            if (columns.testFlag(SC_WindDirection))
                // Wind direction can be null.
                if (!record.value("wind_direction").isNull())
                    samples.windDirection[timeStamp] =
                            record.value("wind_direction").toUInt();

            if (columns.testFlag(SC_SolarRadiation))
                samples.solarRadiation.append(record.value("solar_radiation").toDouble());

            if (columns.testFlag(SC_UV_Index))
                samples.uvIndex.append(record.value("uv_index").toDouble());

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
