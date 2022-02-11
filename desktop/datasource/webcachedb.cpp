#include "webcachedb.h"
#include "compat.h"

#include <QtSql>
#include <QtDebug>
#include <QDesktopServices>
#include <cfloat>
#include <QMap>
#include <QElapsedTimer>

#define SAMPLE_CACHE "sample-cache"
#define sampleCacheDb QSqlDatabase::database(SAMPLE_CACHE)

#define TEMPORARY_IMAGE_SET "::temporary_image_set:"

QSqlQuery WebCacheDB::query() {
    return QSqlQuery(QSqlDatabase::database(SAMPLE_CACHE,false));
}

WebCacheDB::WebCacheDB()
{
    ready = false;
    openDatabase();
}

WebCacheDB::~WebCacheDB() { }

void WebCacheDB::openDatabase() {
    if (QSqlDatabase::database(SAMPLE_CACHE,true).isValid())
        return; // Database is already open.

    qDebug() << "Open cache database...";

    // Try to open the database. If it doesn't exist then create it.
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", SAMPLE_CACHE);

#if QT_VERSION >= 0x050000
    QString filename = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    // Make sure the target directory actually exists.
    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += "/sample-cache.db";

    qDebug() << "Cache database:" << filename;

    db.setDatabaseName(filename);
    if (!db.open()) {
        emit criticalError(tr("Failed to open cache database"));
        return;
    }

    QSqlQuery qv("select sqlite_version()", sampleCacheDb);
    if (qv.isActive()) {
        qv.next();
        qDebug() << "SQLite version: " << qv.record().value(0).toString();
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
        qDebug() << "Creating initial schema...";
        runDbScript(":/cache_db/create.sql");
        //runDbScript(":/cache_db/trig_lookup.sql");
    } else {
        qDebug() << "Checking version...";
        query.exec("select v from db_metadata where k = 'v'");
        if (query.first()) {
            int version = query.value(0).toInt();
            qDebug() << "Cache DB at version" << version;

            // This script has some special needs
            if (version < 2) {
                if (runUpgradeScript(2, ":/cache_db/v2.sql", filename)) { // v1 -> v3
                    QSqlQuery q(db);
                    if (!q.exec("drop table sample_old;")) {
                        qWarning() << "Failed to drop sample_old";
                        emit criticalError("Failed to drop obsolete sample_old table");
                    }
                    if (!q.exec("drop table station_old;")) {
                        qWarning() << "Failed to drop station_old";
                        emit criticalError("Failed to drop obsolete station_old table");
                    }
                    db.commit();
                } else {
                    return; // we failed
                }
            }

            // Run other upgrade scripts
            if (!runUpgradeScript(3, ":/cache_db/v3.sql", filename)) return; // v2 -> v3
            if (!runUpgradeScript(4, ":/cache_db/v4.sql", filename)) return; // v3 -> v4
            if (!runUpgradeScript(5, ":/cache_db/v5.sql", filename)) return; // v4 -> v5
            if (!runUpgradeScript(6, ":/cache_db/v6.sql", filename)) return; // v5 -> v6
            if (!runUpgradeScript(7, ":/cache_db/v7.sql", filename)) return; // v6 -> v7
            if (!runUpgradeScript(8, ":/cache_db/v8.sql", filename)) return; // v7 -> v8
            if (!runUpgradeScript(9, ":/cache_db/v9.sql", filename)) return; // v8 -> v9

        } else {
            emit criticalError(tr("Failed to determine version of cache database"));
        }
    }
    ready = true;
}

bool WebCacheDB::runUpgradeScript(int version, QString script, QString filename) {
    QSqlQuery query("select v from db_metadata where k = 'v'", sampleCacheDb);

    int currentVersion = -1;
    if (query.first()) {
        currentVersion = query.value(0).toInt();
    } else {
        qWarning() << "Failed to determine database version.";
        return false;
    }

    if (currentVersion >= version) {
        return true; // Nothing to do.
    }

    qDebug() << "Cache DB is out of date. Upgrading to v" + QString::number(version) + "...";


    QSqlDatabase db = QSqlDatabase::database(SAMPLE_CACHE,true);
    db.commit();
    db.close();
    db.open();

    if (!runDbScript(script)) {
        qWarning() << "v" + QString::number(version) + " upgrade failed.";
        emit criticalError(QString(tr("Failed to upgrade cache database. Delete file %1 manually to correct error.")).arg(filename));
        return false;
    }

    // Reopen the database to clear locks
    db = QSqlDatabase::database(SAMPLE_CACHE,true);
    db.commit();
    db.close();
    db.open();
    return true;
}

bool WebCacheDB::runDbScript(QString filename) {
    QFile scriptFile(filename);
    scriptFile.open(QIODevice::ReadOnly);

    QString script;

    while(!scriptFile.atEnd()) {
        QString line = scriptFile.readLine();
        if (!line.startsWith("--"))
            script += line;
    }

    // This is really nasty.
    QStringList statements = script.split(";");

    QSqlDatabase::database(SAMPLE_CACHE,false).transaction();

    QSqlQuery query(sampleCacheDb);

    foreach(QString statement, statements) {
        qDebug() << "-------------------------------------------------------------";
        statement = statement.trimmed();

        qDebug() << statement;

        // The split above strips off the semicolons.
        query.exec(statement);

        if (query.lastError().isValid()) {
            qWarning() << "Cache DB Upgrade failure:"
                       << query.lastError().driverText()
                       << query.lastError().databaseText();
            emit criticalError("Database error. Error was: "
                                 + query.lastError().driverText());
            QSqlDatabase::database(SAMPLE_CACHE,false).rollback();
            qDebug() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^";
            return false;
        }
    }

    QSqlDatabase::database(SAMPLE_CACHE,false).commit();
    return true;
}

int WebCacheDB::getStationId(QString stationUrl) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select station_id from station where code = :url");
    query.bindValue(":url", stationUrl);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        query.prepare("insert into station(code) values(:url)");
        query.bindValue(":url", stationUrl);
        query.exec();
        if (!query.lastError().isValid())
            return getStationId(stationUrl);
        else
            qDebug() << "Failed to get stationId for URL:" << stationUrl;
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
    query.prepare("insert into data_file(station, url, last_modified, size, is_complete, "
                  "                      start_contiguous_to, end_contiguous_from, "
                  "                      start_time, end_time) "
                  "values(:station, :url, :last_modified, :size, :is_complete, "
                  "       :start_contig_to, :end_contig_from, :start_time, :end_time)");
    query.bindValue(":station", stationId);
    query.bindValue(":url", dataFile.filename);
    query.bindValue(":last_modified", TO_UNIX_TIME(dataFile.last_modified));
    query.bindValue(":size", dataFile.size);
    query.bindValue(":is_complete", dataFile.isComplete);
    if (dataFile.start_contiguous_to.isValid())
        query.bindValue(":start_contig_to", TO_UNIX_TIME(dataFile.start_contiguous_to));
    else
        query.bindValue(":start_contig_to", QVariant());
    if (dataFile.end_contiguous_from.isValid())
        query.bindValue(":end_contig_from", TO_UNIX_TIME(dataFile.end_contiguous_from));
    else
        query.bindValue(":end_contig_from", QVariant());
    query.bindValue(":start_time", TO_UNIX_TIME(dataFile.start_time));
    query.bindValue(":end_time", TO_UNIX_TIME(dataFile.end_time));
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

void WebCacheDB::updateDataFile(int fileId, QDateTime lastModified, int size, bool isComplete, QDateTime startContiguousTo, QDateTime endContiguousFrom) {
    qDebug() << "Updating data file details...";
    QSqlQuery query(sampleCacheDb);
    query.prepare("update data_file set last_modified = :last_modified, "
                  "size = :size, is_complete = :is_complete, "
                  "start_contiguous_to = :start_contig_to, "
                  "end_contiguous_from = :end_contig_from where id = :id");
    query.bindValue(":last_modified", TO_UNIX_TIME(lastModified));
    query.bindValue(":size", size);
    query.bindValue(":id", fileId);
    query.bindValue(":is_complete", isComplete);
    if (startContiguousTo.isValid())
        query.bindValue(":start_contig_to", TO_UNIX_TIME(startContiguousTo));
    else
        query.bindValue(":start_contig_to", QVariant());
    if (endContiguousFrom.isValid())
        query.bindValue(":end_contig_from", TO_UNIX_TIME(endContiguousFrom));
    else
        query.bindValue(":end_contig_from", QVariant());

    query.exec();

    if (query.lastError().isValid())
        qWarning() << "Failed to update data file information. Error was "
                   << query.lastError();
}

data_file_t WebCacheDB::getDataFileCacheInformation(QString dataFileUrl) {

    data_file_t dataFile;


    if (!ready) {
        dataFile.isValid = false;
        dataFile.size = 0;
        return dataFile;
    }

    qDebug() << "Querying cache stats for URL" << dataFileUrl;

    QSqlQuery query(sampleCacheDb);
    query.prepare("select last_modified, size, is_complete from data_file "
                  "where url =  :url");
    query.bindValue(":url", dataFileUrl);
    query.exec();
    if (query.first()) {

        QSqlRecord record = query.record();
        dataFile.filename = dataFileUrl;
        dataFile.isValid = true;
        dataFile.last_modified = FROM_UNIX_TIME(
                    record.value(0).toLongLong());
        dataFile.size = record.value(1).toInt();
        dataFile.isComplete = record.value(2).toBool();

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

    if (!ready) {
        cacheStats.count = 0;
        cacheStats.isValid = false;
        return cacheStats;
    }

    int fileId = getDataFileId(dataFileUrl);
    if (fileId == -1) {
        // File doesn't exist. No stats for you.
        cacheStats.isValid = false;
        return cacheStats;
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("select min(time_stamp), max(time_stamp), count(*) "
                  "from sample where data_file = :data_file_id "
                  "group by data_file");
    query.bindValue(":data_file_id", fileId);
    query.exec();

    if (query.first()) {
        QSqlRecord record = query.record();
        cacheStats.start = FROM_UNIX_TIME(record.value(0).toLongLong());
        cacheStats.end = FROM_UNIX_TIME(record.value(1).toLongLong());
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

    if (!ready) {
        return;
    }

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
                       dataFile.size,
                       dataFile.isComplete,
                       dataFile.start_contiguous_to,
                       dataFile.end_contiguous_from);
    }

    if (dataFile.expireExisting) {
        // Trash any existing samples for this file.
        truncateFile(dataFileId);
    }

    // Cool. Data file is all ready - now insert the samples.
    cacheDataSet(dataFile.samples, stationId, dataFileId, dataFile.hasSolarData);
}

QVariant nullableDouble(double d) {
    if (d == qQNaN()) {
        return QVariant(QVariant::Double);
    }
    return d;
}

void WebCacheDB::cacheDataSet(SampleSet samples,
                              int stationId,
                              int dataFileId,
                              bool hasSolarData) {
    if (samples.sampleCount == 0) {
        qDebug() << "Data set is empty! Nothing to cache.";
        return;
    }

    qDebug() << "Caching dataset of" << samples.sampleCount << "samples..."; 

    // First up, grab the list of samples that are missing from the database.
    QVariantList timestamps, temperature, dewPoint, apparentTemperature,
            windChill, indoorTemperature, humidity, indoorHumidity, absolutePressure,
            rainfall, stationIds, dataFileIds, averageWindSpeeds,
            gustWindSpeeds, windDirections, solarRadiations, uvIndexes,
            receptions, highTemperatures, lowTemperatures, highRainRates,
            gustWindDirections, evapotranspirations, highSolarRadiations,
            highUVIndexes, forecastRuleIds, soilMoisture1, soilMoisture2,
            soilMoisture3, soilMoisture4, soilTemperature1, soilTemperature2,
            soilTemperature3, soilTemperature4, leafWetness1, leafWetness2,
            leafTemperature1, leafTemperature2, extraTemperature1,
            extraTemperature2, extraTemperature3, extraHumidity1,
            extraHumidity2, mslPressures;

    // This depends on the station
    bool mslPressureEnabled = !samples.meanSeaLevelPressure.isEmpty();

    // These are (at the moment) all specific to DAVIS hardware.
//    bool uvIndexEnabled = !samples.uvIndex.isEmpty();
//    bool solarRadiationEnabled = !samples.solarRadiation.isEmpty();
    bool receptionEnabled = !samples.reception.isEmpty();
    bool highTempEnabled = !samples.highTemperature.isEmpty();
    bool lowTempEnabled = !samples.lowTemperature.isEmpty();
    bool highRainRateEnabled = !samples.highRainRate.isEmpty();
//    bool evapotranspirationsEnabled = !samples.evapotranspiration.isEmpty();
//    bool highSolarRadiationEnabled = !samples.highSolarRadiation.isEmpty();
//    bool highUVIndexEnabled = !samples.highUVIndex.isEmpty();
    bool forecastRuleEnabled = !samples.forecastRuleId.isEmpty();

    bool soilMoisture1Enabled = !samples.soilMoisture1.isEmpty();
    bool soilMoisture2Enabled = !samples.soilMoisture2.isEmpty();
    bool soilMoisture3Enabled = !samples.soilMoisture3.isEmpty();
    bool soilMoisture4Enabled = !samples.soilMoisture4.isEmpty();

    bool soilTemperature1Enabled = !samples.soilTemperature1.isEmpty();
    bool soilTemperature2Enabled = !samples.soilTemperature2.isEmpty();
    bool soilTemperature3Enabled = !samples.soilTemperature3.isEmpty();
    bool soilTemperature4Enabled = !samples.soilTemperature4.isEmpty();

    bool leafWetness1Enabled = !samples.leafWetness1.isEmpty();
    bool leafWetness2Enabled = !samples.leafWetness2.isEmpty();

    bool leafTemperature1Enabled = !samples.leafTemperature1.isEmpty();
    bool leafTemperature2Enabled = !samples.leafTemperature2.isEmpty();

    bool extraTemperature1Enabled = !samples.extraTemperature1.isEmpty();
    bool extraTemperature2Enabled = !samples.extraTemperature2.isEmpty();
    bool extraTemperature3Enabled = !samples.extraTemperature3.isEmpty();

    bool extraHumidity1Enabled = !samples.extraHumidity1.isEmpty();
    bool extraHumidity2Enabled = !samples.extraHumidity2.isEmpty();

    qDebug() << "Preparing list of samples to insert...";

    QElapsedTimer  timer;
    timer.start();

    for (unsigned int i = 0; i < samples.sampleCount; i++) {
        uint timestamp = samples.timestampUnix.at(i);

        stationIds << stationId;   // Kinda pointless but the API wants it.
        dataFileIds << dataFileId; // Likewise

        // These columns are mandatory for all weather station hardware types so will
        // always be present.
        timestamps.append(timestamp);
        temperature.append(nullableDouble(samples.temperature.at(i)));
        dewPoint.append(nullableDouble(samples.dewPoint.at(i)));
        apparentTemperature.append(nullableDouble(samples.apparentTemperature.at(i)));
        windChill.append(nullableDouble(samples.windChill.at(i)));
        indoorTemperature.append(nullableDouble(samples.indoorTemperature.at(i)));
        humidity.append(nullableDouble(samples.humidity.at(i)));
        indoorHumidity.append(nullableDouble(samples.indoorHumidity.at(i)));
        absolutePressure.append(nullableDouble(samples.absolutePressure.at(i)));
        rainfall.append(nullableDouble(samples.rainfall.at(i)));
        averageWindSpeeds.append(nullableDouble(samples.averageWindSpeed.at(i)));
        gustWindSpeeds.append(nullableDouble(samples.gustWindSpeed.at(i)));

        if (mslPressureEnabled) mslPressures.append(samples.meanSeaLevelPressure.at(i));
        else mslPressures.append(QVariant(QVariant::Double));

        if (receptionEnabled) receptions.append(nullableDouble(samples.reception.at(i)));
        else receptions.append(QVariant(QVariant::Double));

        if (highTempEnabled) highTemperatures.append(nullableDouble(samples.highTemperature.at(i)));
        else highTemperatures.append(QVariant(QVariant::Double));

        if (lowTempEnabled) lowTemperatures.append(nullableDouble(samples.lowTemperature.at(i)));
        else lowTemperatures.append(QVariant(QVariant::Double));

        if (highRainRateEnabled) highRainRates.append(nullableDouble(samples.highRainRate.at(i)));
        else highRainRates.append(QVariant(QVariant::Double));

        if (forecastRuleEnabled) forecastRuleIds.append(samples.forecastRuleId.at(i));
        else forecastRuleIds.append(0); // TODO: this shouldn't be zero.

        if (soilMoisture1Enabled) soilMoisture1.append(samples.soilMoisture1.at(i));
        else soilMoisture1.append(QVariant(QVariant::Double));

        if (soilMoisture2Enabled) soilMoisture2.append(samples.soilMoisture2.at(i));
        else soilMoisture2.append(QVariant(QVariant::Double));

        if (soilMoisture3Enabled) soilMoisture3.append(samples.soilMoisture3.at(i));
        else soilMoisture3.append(QVariant(QVariant::Double));

        if (soilMoisture4Enabled) soilMoisture4.append(samples.soilMoisture4.at(i));
        else soilMoisture4.append(QVariant(QVariant::Double));

        if (soilTemperature1Enabled) soilTemperature1.append(samples.soilTemperature1.at(i));
        else soilTemperature1.append(QVariant(QVariant::Double));

        if (soilTemperature2Enabled) soilTemperature2.append(samples.soilTemperature2.at(i));
        else soilTemperature2.append(QVariant(QVariant::Double));

        if (soilTemperature3Enabled) soilTemperature3.append(samples.soilTemperature3.at(i));
        else soilTemperature3.append(QVariant(QVariant::Double));

        if (soilTemperature4Enabled) soilTemperature4.append(samples.soilTemperature4.at(i));
        else soilTemperature4.append(QVariant(QVariant::Double));

        if (leafWetness1Enabled) leafWetness1.append(samples.leafWetness1.at(i));
        else leafWetness1.append(QVariant(QVariant::Double));

        if (leafWetness2Enabled) leafWetness2.append(samples.leafWetness2.at(i));
        else leafWetness2.append(QVariant(QVariant::Double));

        if (leafTemperature1Enabled) leafTemperature1.append(samples.leafTemperature1.at(i));
        else leafTemperature1.append(QVariant(QVariant::Double));

        if (leafTemperature2Enabled) leafTemperature2.append(samples.leafTemperature2.at(i));
        else leafTemperature2.append(QVariant(QVariant::Double));

        if (extraTemperature1Enabled) extraTemperature1.append(samples.extraTemperature1.at(i));
        else extraTemperature1.append(QVariant(QVariant::Double));

        if (extraTemperature2Enabled) extraTemperature2.append(samples.extraTemperature2.at(i));
        else extraTemperature2.append(QVariant(QVariant::Double));

        if (extraTemperature3Enabled) extraTemperature3.append(samples.extraTemperature3.at(i));
        else extraTemperature3.append(QVariant(QVariant::Double));

        if (extraHumidity1Enabled) extraHumidity1.append(samples.extraHumidity1.at(i));
        else extraHumidity1.append(QVariant(QVariant::Double));

        if (extraHumidity2Enabled) extraHumidity2.append(samples.extraHumidity2.at(i));
        else extraHumidity2.append(QVariant(QVariant::Double));

        if (samples.windDirection.contains(timestamp))
            windDirections.append(samples.windDirection[timestamp]);
        else {
            // Append null (no wind speed at this timestamp).
            QVariant val;
            val.convert(QVariant::Int);
            windDirections.append(val);
        }

        if (samples.gustWindDirection.contains(timestamp)) {
            gustWindDirections.append(samples.gustWindDirection[timestamp]);
        } else {
            QVariant val;
            val.convert(QVariant::Int);
            gustWindDirections.append(val);
        }

        if (hasSolarData) {
            solarRadiations.append(nullableDouble(samples.solarRadiation.at(i)));
            uvIndexes.append(nullableDouble(samples.uvIndex.at(i)));
            highSolarRadiations.append(nullableDouble(samples.highSolarRadiation.at(i)));
            highUVIndexes.append(nullableDouble(samples.highUVIndex.at(i)));
            evapotranspirations.append(nullableDouble(samples.evapotranspiration.at(i)));
        } else {
            // The lists still need to be populated for the query. As we don't
            // have any real data to put in them we'll use 0.
            solarRadiations.append(QVariant(QVariant::Double));
            uvIndexes.append(QVariant(QVariant::Double));
            highSolarRadiations.append(QVariant(QVariant::Double));
            highUVIndexes.append(QVariant(QVariant::Double));
            evapotranspirations.append(QVariant(QVariant::Double));
        }
    }

    // Wrapping bulk inserts in a transaction cuts total time by orders of
    // magnitude
    QSqlDatabase db = sampleCacheDb;
    db.transaction();

    qDebug() << "Inserting" << stationIds.count() << "samples...";
    timer.restart();
    if (!timestamps.isEmpty()) {
        // We have data to store.
        QSqlQuery query(db);
        query.prepare(
                    "insert into sample(station_id, time_stamp, temperature, "
                    "dew_point, apparent_temperature, wind_chill, relative_humidity, "
                    "absolute_pressure, indoor_temperature, indoor_relative_humidity, rainfall, "
                    "data_file, average_wind_speed, gust_wind_speed, "
                    "wind_direction, solar_radiation, uv_index,reception, "
                    "high_temperature, low_temperature, high_rain_rate, "
                    "gust_wind_direction, evapotranspiration, "
                    "high_solar_radiation, high_uv_index, forecast_rule_id, "
                    "soil_moisture_1, soil_moisture_2, soil_moisture_3, soil_moisture_4, "
                    "soil_temperature_1, soil_temperature_2, soil_temperature_3, soil_temperature_4, "
                    "leaf_wetness_1, leaf_wetness_2, leaf_temperature_1, leaf_temperature_2, "
                    "extra_temperature_1, extra_temperature_2, extra_temperature_3, "
                    "extra_humidity_1, extra_humidity_2, mean_sea_level_pressure) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                    "?, ?, ?, ?, ?, ?, ?);");
        query.addBindValue(stationIds);
        query.addBindValue(timestamps);
        query.addBindValue(temperature);
        query.addBindValue(dewPoint);
        query.addBindValue(apparentTemperature);
        query.addBindValue(windChill);
        query.addBindValue(humidity);
        query.addBindValue(absolutePressure);
        query.addBindValue(indoorTemperature);
        query.addBindValue(indoorHumidity);
        query.addBindValue(rainfall);
        query.addBindValue(dataFileIds);
        query.addBindValue(averageWindSpeeds);
        query.addBindValue(gustWindSpeeds);
        query.addBindValue(windDirections);
        query.addBindValue(solarRadiations);
        query.addBindValue(uvIndexes);
        query.addBindValue(receptions);
        query.addBindValue(highTemperatures);
        query.addBindValue(lowTemperatures);
        query.addBindValue(highRainRates);
        query.addBindValue(gustWindDirections);
        query.addBindValue(evapotranspirations);
        query.addBindValue(highSolarRadiations);
        query.addBindValue(highUVIndexes);
        query.addBindValue(forecastRuleIds);
        query.addBindValue(soilMoisture1);
        query.addBindValue(soilMoisture2);
        query.addBindValue(soilMoisture3);
        query.addBindValue(soilMoisture4);
        query.addBindValue(soilTemperature1);
        query.addBindValue(soilTemperature2);
        query.addBindValue(soilTemperature3);
        query.addBindValue(soilTemperature4);
        query.addBindValue(leafWetness1);
        query.addBindValue(leafWetness2);
        query.addBindValue(leafTemperature1);
        query.addBindValue(leafTemperature2);
        query.addBindValue(extraTemperature1);
        query.addBindValue(extraTemperature2);
        query.addBindValue(extraTemperature3);
        query.addBindValue(extraHumidity1);
        query.addBindValue(extraHumidity2);
        query.addBindValue(mslPressures);
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

    optimise();

    qDebug() << "Cache insert completed.";
}

bool WebCacheDB::timespanIsCached(QString stationUrl, QDateTime startTime, QDateTime endTime) {
    int stationId = getStationId(stationUrl);
    qDebug() << "------------------------\n"
             << "Checking timespan" << startTime
             << "to" << endTime
             << "for station" << stationId
             << "is covered by cached data";

    // Fetch out all the data file information
    QSqlQuery query(sampleCacheDb);
    query.prepare("select start_time, end_time, is_complete, start_contiguous_to,"
                  "       end_contiguous_from, next_datafile_start "
                  "from data_file_times where station = :station");
    query.bindValue(":station", stationId);
    query.exec();

    bool foundInitialDataset = false;

    if (query.first()) {
        QDateTime expectedNextDataStartTime;
        do {
            QSqlRecord record = query.record();
            QDateTime dataStartTime = FROM_UNIX_TIME(record.field(0).value().toLongLong());
            QDateTime dataEndTime = FROM_UNIX_TIME(record.field(1).value().toLongLong());
            bool isComplete = record.field(2).value().toBool();
            QDateTime startContiguousTo, endContiguousFrom;
            if (!record.field(3).value().isNull())
                startContiguousTo = FROM_UNIX_TIME(record.field(3).value().toLongLong());
            if (!record.field(4).value().isNull())
                endContiguousFrom = FROM_UNIX_TIME(record.field(4).value().toLongLong());
            QDateTime nextDataFileStart = FROM_UNIX_TIME(record.field(5).value().toLongLong());

            if (dataEndTime < startTime) {
                qDebug() << "Skip data file for period starting" << dataStartTime << "- period predates requested timespan";
                continue; // Not interested in this data file - too old
            }

            if (dataStartTime > endTime) {
                qDebug() << "Skip data file for period starting" << dataStartTime << "- period predates requested timespan";
                continue; // Not interested in this data file - too new
            }

            // Data file is within the specified timespan. Check its sufficiently complete.

            if (dataStartTime <= startTime && endTime <= dataEndTime) {
                // Data file is greater than the timespan!
                qDebug() << "Requested timespan is covered by single data file period starting" << dataStartTime;
                if (isComplete) {
                    qDebug() << "Single data file is complete! Timespan is cached.";
                    return true;
                } else {
                    qDebug() << "Single data file is incomplete. Not possible to determine if gap falls within requested timespan. Possibly timespan is uncached. Failing.";
                    qDebug() << "Timespan is not fully covered by cache - failed on incomplete covering datafile.";
                    return false;
                }
            }
            // Else timespan is covered by multiple data files.
            //  * The first data file must be either complete
            //    or contiguous from before startTime to the end of the period
            //  * The last data file must be either complete or contiguous
            //    from the start of the file to the end of the period
            //  * All intermediate data files must be complete.


            if (!foundInitialDataset) {
                // First data file we're interested in!
                qDebug() << "First data file starts at" << dataStartTime;

                foundInitialDataset = true;
                expectedNextDataStartTime = dataStartTime;

                // Check the start of the timespan falls within this file. If it doesn't
                // then its probably in a missing data file.
                if (dataStartTime > startTime) {
                    qDebug() << "Timespan is not fully covered by cache - starting data file appears to be missing.";
                    return false;
                }

                // This file contains the start of the requested timespan.
                // So it needs to be either:
                //  (A) Complete
                // OR
                //  (B) Have endContiguousFrom < startTime

                if (!isComplete && (endContiguousFrom.isNull() || endContiguousFrom > startTime)) {
                    qDebug() << "Data file for period starting"
                             << dataStartTime
                             << "is incomplete and there is a gap in the data somewhere between"
                             << startTime << "and the file end. End contiguous from:"
                             << endContiguousFrom;
                    qDebug() << "Timespan is not fully covered by cache - failed on first datafile";
                    return false;
                }
            } else if (dataStartTime > startTime && dataEndTime >= endTime) {
                // Its the final data file we're interested in!
                qDebug() << "Final data file covers the period" << dataStartTime << "to" << dataEndTime;

                if (!isComplete && (startContiguousTo.isNull() || startContiguousTo < endTime)) {
                    qDebug() << "Data file for period starting"
                             << dataStartTime
                             << "is incomplete and there is a gap in the data somewhere between"
                             << "the start of the file and" << endTime
                             << " - start is contiguous to:" << startContiguousTo;
                    qDebug() << "Timespan not fully covered by cache - failed on final datafile";
                    return false;
                }
            } else if (!isComplete) {
                // If its not the
                qDebug() << "Intermediate data file for period starting"
                         << dataStartTime
                         << "is incomplete.";
                qDebug() << "Timespan is not fully covered by cache - failed on only or intermediate datafile";

                return false;
            }

            if (dataStartTime != expectedNextDataStartTime) {
                qDebug() << "Data file has period starting at" << dataStartTime
                         << " - expected start time" << expectedNextDataStartTime;
                qDebug() << "Timespan is not fully covered by cache - missing data file";
                return false;
            }

            qDebug() << "Data file starting at" << dataStartTime << "OK!";

            expectedNextDataStartTime = nextDataFileStart;

            // If we got here then this data file is fine. On to the next.

        } while (query.next());
    } else {
        qDebug() << "No data files!";
        return false;
    }

    if(!foundInitialDataset) {
        qDebug() << "Found no datasets covering requested timespan! Timespan is not covered by cache at all!";
        return false;
    }

    qDebug() << "Timespan is covered by the cache!";
    return true;
}


int WebCacheDB::getImageSourceId(int stationId, QString code) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from image_source where station = :station "
                  "and code = :code");
    query.bindValue(":station", stationId);
    query.bindValue(":code", code.toLower());
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        return -1; // doesn't exist
    }
}

int WebCacheDB::createImageSource(int stationId, ImageSource source) {
    int imageSourceId = getImageSourceId(stationId, source.code);

    // Image source doesn't exist? Create it and try again
    if (imageSourceId == -1) {
        QSqlQuery query(sampleCacheDb);
        query.prepare("insert into image_source(station, code, name, description) "
                      "values(:station, :code, :name, :description)");
        query.bindValue(":station", stationId);
        query.bindValue(":code", source.code.toLower());
        query.bindValue(":name", source.name);
        query.bindValue(":description", source.description);
        query.exec();
        if (!query.lastError().isValid()) {
            return getImageSourceId(stationId, source.code);
        } else {
            qDebug() << "Failed to insert image source record. ImageSet will "
                        "not be cached.";
            qDebug() << query.lastError().databaseText();
            return -1;
        }
    }
    return imageSourceId;
}

void WebCacheDB::updateImageSourceInfo(int imageSourceId, QString name,
                                       QString description) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("update image_source set name = :name, "
                  "description = :description "
                  "where id = :id");
    query.bindValue(":id", imageSourceId);
    query.bindValue(":name", name);
    query.bindValue(":description", description);
    query.exec();

    if (query.lastError().isValid()) {
        qDebug() << "Failed to update image source: " << query.lastError().databaseText();
    }
}

int WebCacheDB::getImageSetId(QString url) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select id from image_set where url = :url");
    query.bindValue(":url", url);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toInt();
    } else {
        qDebug() << "No stored image set data for url " << url;
        return -1;
    }
}

void WebCacheDB::updateImageSetInfo(image_set_t imageSet) {
    int imageSetId = getImageSetId(imageSet.filename);

    QSqlQuery query(sampleCacheDb);

    qDebug() << "Updating image set...";

    if (imageSet.is_valid) {
        query.prepare("update image_set set last_modified = :last_modified, "
                      "size = :size, is_valid = :is_valid where id = :id");
        query.bindValue(":id", imageSetId);
        query.bindValue(":last_modified", imageSet.last_modified);
        query.bindValue(":size", imageSet.size);
        query.bindValue(":is_valid", true);
    } else {
        query.prepare("update image_set set is_valid = :is_valid "
                      "where id = :id");
        query.bindValue(":id", imageSetId);
        query.bindValue(":is_valid", false);
    }

    query.exec();

    if (query.lastError().isValid()) {
        qDebug() << "Failed to update image set: " << query.lastError().databaseText();
    } else {
        qDebug() << "Image set updated.";
    }
}

int WebCacheDB::storeImageSetInfo(image_set_t imageSet, int imageSourceId) {
    int imageSetId = getImageSetId(imageSet.filename);


    // Image source doesn't exist? Create it and try again
    if (imageSetId == -1) {
        QSqlQuery query(sampleCacheDb);
        query.prepare("insert into image_set(image_source, url, last_modified, "
                      "size, is_valid) "
                      "values(:image_source, :url, :last_modified, "
                      ":size, :is_valid)");
        query.bindValue(":image_source", imageSourceId);
        query.bindValue(":url", imageSet.filename);
        query.bindValue(":last_modified", imageSet.last_modified);
        query.bindValue(":size", imageSet.size);
        query.bindValue(":is_valid", imageSet.is_valid);
        query.exec();
        if (!query.lastError().isValid()) {
            return getImageSetId(imageSet.filename);
        } else {
            qDebug() << "Failed to insert image set record. ImageSet will "
                        "not be cached.";
            qDebug() << query.lastError().databaseText();
            return -1;
        }
    }
    return imageSetId;
}

bool WebCacheDB::imageExists(int stationId, int id) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select i.id "
                  "from image i "
                  "inner join image_source src on src.id = i.source "
                  "where src.station = :station "
                  "and i.id = :id");
    query.bindValue(":station", stationId);
    query.bindValue(":id", id);
    query.exec();
    if (query.first()) {
        return true;
    } else {
        return false;
    }
}

void WebCacheDB::storeImageInfo(QString stationUrl, ImageInfo image) {

    if (!ready) {
        return;
    }

    qDebug() << "Store single image against temporary image set...";

    // Grab station ID (this will create the station if it doesn't exist)
    int stationId = getStationId(stationUrl);

    if (imageExists(stationId, image.id)) {

        qDebug() << "Skip: Image metadata already exists - not caching against temporary set";
        return;
    }

    // This will just return the sources ID if it already exists
    int imageSourceId = createImageSource(stationId, image.imageSource);

    if (imageSourceId < 0 || stationId < 0) {
        qDebug() << "Image source or station not stored. StationId: "
                 << stationId << ", sourceId: " << imageSourceId;
        return; // can't cache if the above inserts failed.
    }

    // We don't update the image source information here as we don't really
    // care all that much about it and we probably don't have the full details
    // anyway.

    // We don't know what image set this random image belongs in so we'll
    // assign it to a temporary image set for station. The image may later be
    // moved to the correct image set.
    int imageSetId = getImageSetId(TEMPORARY_IMAGE_SET + stationUrl);
    if (imageSetId == -1) {

        image_set_t imageSet;
        imageSet.filename = TEMPORARY_IMAGE_SET + stationUrl;
        imageSet.size = 1;
        imageSet.station_url = stationUrl;
        imageSet.source = image.imageSource;
        imageSet.is_valid = true;
        imageSet.last_modified = QDateTime::currentDateTime();


        imageSetId = storeImageSetInfo(imageSet, imageSourceId);
        if (imageSetId < 0) {
            qDebug() << "Failed to store image set information";
            return;
        }

    }

    // Store the image metadata.
    storeImage(image, imageSetId, stationId, imageSourceId);
}

void WebCacheDB::updateImageInfo(QString stationUrl, ImageInfo imageInfo) {

    if (!ready) {
        return;
    }

    int stationId = getStationId(stationUrl);

    if (!imageExists(stationId, imageInfo.id)) {
        qDebug() << "Cant update image: no record of image exists";
        return;
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("update image set timestamp = :timestamp, date = :date, "
                  "type_code = :type_code, title = :title, "
                  "description = :description, mime_type = :mime_type, "
                  "url = :url, metadata = :metadata, meta_url = :meta_url,"
                  "type_name = :type_name "
                  "where id = :id");

    query.bindValue(":id", imageInfo.id);
    query.bindValue(":timestamp", imageInfo.timeStamp);
    query.bindValue(":date", imageInfo.timeStamp.date());
    query.bindValue(":type_code", imageInfo.imageTypeCode);
    query.bindValue(":title", imageInfo.title);
    query.bindValue(":description", imageInfo.description);
    query.bindValue(":mime_type", imageInfo.mimeType);
    query.bindValue(":url", imageInfo.fullUrl);
    query.bindValue(":metadata", imageInfo.metadata);
    query.bindValue(":meta_url", imageInfo.metaUrl);
    query.bindValue(":type_name", imageInfo.imageTypeName);

    query.exec();
    if (query.lastError().isValid()) {
        qDebug() << "Failed to update image " << imageInfo.fullUrl
                 << ", database error: " << query.lastError().databaseText()
                 << ", driver error: " << query.lastError().driverText();
    }
}

void WebCacheDB::storeImage(ImageInfo image, int imageSetId, int stationId,
                            int imageSourceId) {
    if (imageExists(stationId, image.id)) {
        // If the image already exists it might exist against some other
        // (temporary) image set. Delete it so that it can be inserted properly
        // for the specified set.

        qDebug() << "Image " << stationId << ":" << image.id << " already exists. Deleting...";
        QSqlQuery query(sampleCacheDb);
        query.prepare("delete from image "
                      "where id = :id "
                      "and source in (select id from image_source "
                      "               where station = :station)");
        query.bindValue(":id", image.id);
        query.bindValue(":station", stationId);
        query.exec();
        if (query.lastError().isValid()) {
            qDebug() << "Failed to delete image. Insert will be skipped. "
                        "Database error: "
                     << query.lastError().driverText();
            return;
        }
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("insert into image(id, image_set, source, timestamp, date, "
                  "type_code, title, description, mime_type, url, metadata, "
                  "meta_url, type_name) "
                  "values(:id, :image_set, :source, :timestamp, :date, "
                  ":type_code, :title, :description, :mime_type, :url,"
                  ":metadata, :meta_url, :type_name)");
    query.bindValue(":id", image.id);
    query.bindValue(":image_set", imageSetId);
    query.bindValue(":source", imageSourceId);
    query.bindValue(":timestamp", image.timeStamp);
    query.bindValue(":date", image.timeStamp.date());
    query.bindValue(":type_code", image.imageTypeCode);
    query.bindValue(":title", image.title);
    query.bindValue(":description", image.description);
    query.bindValue(":mime_type", image.mimeType);
    query.bindValue(":url", image.fullUrl);
    query.bindValue(":metadata", image.metadata);
    query.bindValue(":meta_url", image.metaUrl);
    query.bindValue(":type_name", image.imageTypeName);
    query.exec();
    if (query.lastError().isValid()) {
        qDebug() << "Failed to insert image " << image.fullUrl
                 << ", database error: " << query.lastError().databaseText();
    }
}

void WebCacheDB::cacheImageSet(image_set_t imageSet) {

    if (!ready) {
        return;
    }

    // Grab station ID (this will create the station if it doesn't exist)
    int stationId = getStationId(imageSet.station_url);

    // This will just return the sources ID if it already exists
    int imageSourceId = createImageSource(stationId, imageSet.source);

    if (imageSourceId < 0 || stationId < 0) {
        qDebug() << "Image source or station not stored. StationId: "
                 << stationId << ", sourceId: " << imageSourceId;
        return; // can't cache if the above inserts failed.
    }

    // Update image source details
    updateImageSourceInfo(imageSourceId, imageSet.source.name,
                          imageSet.source.description);

    // Make sure the image set exists in the database with current details
    int imageSetId = getImageSetId(imageSet.filename);
    if (imageSetId == -1) {
        imageSetId = storeImageSetInfo(imageSet, imageSourceId);
        if (imageSetId < 0) {
            qDebug() << "Failed to store image set information";
            return;
        }

    } else {
        // Create the image set
        updateImageSetInfo(imageSet);
    }

    qDebug() << "Caching images for set " << imageSetId;

    // Insert all the image info records
    foreach (ImageInfo image, imageSet.images) {
        storeImage(image, imageSetId, stationId, imageSourceId);
    }
}

ImageInfo RecordToImageInfo(QSqlRecord record) {
    ImageInfo result;
    result.id = record.field(0).value().toInt();
    result.timeStamp = record.field(1).value().toDateTime();
    result.imageTypeCode = record.field(2).value().toString();
    result.title = record.field(3).value().toString();
    result.description = record.field(4).value().toString();
    result.mimeType = record.field(5).value().toString();
    result.fullUrl = record.field(6).value().toString();

    result.imageSource.code = record.field(7).value().toString();
    result.imageSource.name = record.field(8).value().toString();
    result.imageSource.description = record.field(9).value().toString();
    result.hasMetadata = !record.field(10).isNull();
    result.metadata = record.field(10).value().toString();
    result.metaUrl = record.field(11).value().toString();
    result.imageTypeName = record.field(12).value().toString();
    return result;
}

ImageInfo WebCacheDB::getImageInfo(QString stationUrl, int id) {

    if (!ready) {
        ImageInfo fail;
        return fail;
    }

    int stationId = getStationId(stationUrl);

    QSqlQuery query(sampleCacheDb);
    query.prepare("select i.id, i.timestamp, i.type_code, i.title, "
                  "i.description, i.mime_type, i.url, src.code, src.name, "
                  "src.description, i.metadata, i.meta_url, i.type_name "
                  "from image i "
                  "inner join image_source src on src.id = i.source "
                  "where src.station = :station "
                  "  and i.id = :id");
    query.bindValue(":id", id);
    query.bindValue(":station", stationId);
    query.exec();
    if (query.first()) {
        return RecordToImageInfo(query.record());
    } else {
        qDebug() << "Metadata for iamge not found: " << id << stationUrl;
        ImageInfo fail;
        return fail;
    }
}

ImageSource WebCacheDB::getImageSource(QString stationUrl, QString sourceCode) {

    if (!ready) {
        ImageSource src;
        return src;
    }

    int stationId = getStationId(stationUrl);
    QSqlQuery query(sampleCacheDb);
    query.prepare("select code, name, description "
                  "from image_source "
                  "where station = :station "
                  "and code = :code");
    query.bindValue(":station", stationId);
    query.bindValue(":code", sourceCode.toLower());
    query.exec();

    ImageSource src;

    if (query.first()) {
        QSqlRecord record = query.record();

        src.code = record.field(0).value().toString();
        src.name = record.field(1).value().toString();
        src.description = record.field(2).value().toString();
    }

    return src;
}

QVector<ImageInfo> WebCacheDB::getImagesForDate(QDate date, QString stationUrl,
                                         QString imageSourceCode) {

    if (!ready) {
        QVector<ImageInfo> vec;
        return vec;
    }

    int stationId = getStationId(stationUrl);

    qDebug() << "Station Id: " << stationId;

    QSqlQuery query(sampleCacheDb);
    query.prepare("select i.id, i.timestamp, i.type_code, i.title, "
                  "i.description, i.mime_type, i.url, src.code, src.name, "
                  "src.description, i.metadata, i.meta_url, i.type_name "
                  "from image i "
                  "inner join image_source src on src.id = i.source "
                  "where src.station = :station "
                  "  and src.code = :src_code "
                  "  and i.date = :date "
                  "order by date asc");
    query.bindValue(":station", stationId);
    query.bindValue(":src_code", imageSourceCode.toLower());
    query.bindValue(":date", date);
    query.exec();

    qDebug() << query.executedQuery();

    QVector<ImageInfo> images;

    if (query.first()) {
        do {
            QSqlRecord record = query.record();
            images.append(RecordToImageInfo(record));
        } while (query.next());
    } else {
        qDebug() << "No cached image metadata for " << date << stationUrl << imageSourceCode;
    }

    return images;
}

QVector<ImageInfo> WebCacheDB::getMostRecentImages(QString stationUrl) {

    if (!ready) {
        QVector<ImageInfo> fail;
        return fail;
    }

    int stationId = getStationId(stationUrl);

    qDebug() << "Station Id: " << stationId;

    QSqlQuery query(sampleCacheDb);
    query.prepare("select i.id, i.timestamp, i.type_code, i.title, "
                  "i.description, i.mime_type, i.url, src.code, src.name, "
                  "src.description, i.metadata, i.meta_url, i.type_name "
           "from image i "
           "inner join image_source src on src.id = i.source "
           "inner join (select max(timestamp) as max_ts, src.id as src_id "
           "    from image i "
           "    inner join image_source src on src.id = i.source "
           "    where src.station = :station_a "
           "    group by src.code) as mx "
           "  on mx.max_ts = i.timestamp and mx.src_id = i.source "
           "where src.station = :station_b");
    query.bindValue(":station_a", stationId);
    query.bindValue(":station_b", stationId);
    query.exec();

    qDebug() << query.executedQuery();

    QVector<ImageInfo> images;

    if (query.first()) {
        do {
            QSqlRecord record = query.record();
            images.append(RecordToImageInfo(record));
        } while (query.next());
    } else {
        qDebug() << "No cached image metadata for " << stationUrl;
    }

    return images;

}

image_set_t WebCacheDB::getImageSetCacheInformation(QString imageSetUrl) {

    if (!ready) {
        image_set_t fail;
        return fail;
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("select last_modified, size, is_valid "
                  "from image_set "
                  "where url = :url ");
    query.bindValue(":url", imageSetUrl);
    query.exec();

    image_set_t setinfo;
    setinfo.is_valid = false;

    if (query.first()) {
        QSqlRecord record = query.record();

        setinfo.last_modified = record.field(0).value().toDateTime();
        setinfo.size = record.field(0).value().toInt();
        setinfo.is_valid = record.field(0).value().toBool();
        return setinfo;
    } else {
        qDebug() << "Failed to load image set information.";
        if (query.lastError().isValid()) {
            qDebug() << "Database error: " << query.lastError().databaseText();
        }
        return setinfo;
    }
}

QString WebCacheDB::buildColumnList(SampleColumns columns, QString format) {
    QString query;
    if (columns.standard.testFlag(SC_Timestamp))
        query += format.arg("time_stamp");
    if (columns.standard.testFlag(SC_Temperature))
        query += format.arg("temperature");
    if (columns.standard.testFlag(SC_DewPoint))
        query += format.arg("dew_point");
    if (columns.standard.testFlag(SC_ApparentTemperature))
        query += format.arg("apparent_temperature");
    if (columns.standard.testFlag(SC_WindChill))
        query += format.arg("wind_chill");
    if (columns.standard.testFlag(SC_IndoorTemperature))
        query += format.arg("indoor_temperature");
    if (columns.standard.testFlag(SC_IndoorHumidity))
        query += format.arg("indoor_relative_humidity");
    if (columns.standard.testFlag(SC_Humidity))
        query += format.arg("relative_humidity");
    // SC_Pressure is computed from both absolute_pressure and mean_sea_level_pressure
    // so when SC_Pressure is requested we need to ensure those two columns
    // are included so that aggregated queries work properly.
    if (columns.standard.testFlag(SC_AbsolutePressure) || columns.standard.testFlag(SC_Pressure))
        query += format.arg("absolute_pressure");
    if (columns.standard.testFlag(SC_MeanSeaLevelPressure) || columns.standard.testFlag(SC_Pressure))
        query += format.arg("mean_sea_level_pressure");
    if (columns.standard.testFlag(SC_AverageWindSpeed))
        query += format.arg("average_wind_speed");
    if (columns.standard.testFlag(SC_GustWindSpeed))
        query += format.arg("gust_wind_speed");
    if (columns.standard.testFlag(SC_Rainfall))
        query += format.arg("rainfall");
    if (columns.standard.testFlag(SC_WindDirection))
        query += format.arg("wind_direction");
    if (columns.standard.testFlag(SC_SolarRadiation))
        query += format.arg("solar_radiation");
    if (columns.standard.testFlag(SC_UV_Index))
        query += format.arg("uv_index");
    if (columns.standard.testFlag(SC_GustWindDirection))
        query += format.arg("gust_wind_direction");
    if (columns.standard.testFlag(SC_Evapotranspiration))
        query += format.arg("evapotranspiration");
    if (columns.standard.testFlag(SC_HighTemperature))
        query += format.arg("high_temperature");
    if (columns.standard.testFlag(SC_LowTemperature))
        query += format.arg("low_temperature");
    if (columns.standard.testFlag(SC_HighRainRate))
        query += format.arg("high_rain_rate");
    if (columns.standard.testFlag(SC_HighSolarRadiation))
        query += format.arg("high_solar_radiation");
    if (columns.standard.testFlag(SC_HighUVIndex))
        query += format.arg("high_uv_index");
    if (columns.standard.testFlag(SC_ForecastRuleId))
        query += format.arg("forecast_rule_id");
    if (columns.standard.testFlag(SC_Reception))
        query += format.arg("reception");
    if (columns.extra.testFlag(EC_LeafWetness1))
        query += format.arg("leaf_wetness_1");
    if (columns.extra.testFlag(EC_LeafWetness2))
        query += format.arg("leaf_wetness_2");
    if (columns.extra.testFlag(EC_LeafTemperature1))
        query += format.arg("leaf_temperature_1");
    if (columns.extra.testFlag(EC_LeafTemperature2))
        query += format.arg("leaf_temperature_2");
    if (columns.extra.testFlag(EC_SoilMoisture1))
        query += format.arg("soil_moisture_1");
    if (columns.extra.testFlag(EC_SoilMoisture2))
        query += format.arg("soil_moisture_2");
    if (columns.extra.testFlag(EC_SoilMoisture3))
        query += format.arg("soil_moisture_3");
    if (columns.extra.testFlag(EC_SoilMoisture4))
        query += format.arg("soil_moisture_4");
    if (columns.extra.testFlag(EC_SoilMoisture4))
        query += format.arg("soil_moisture_4");
    if (columns.extra.testFlag(EC_SoilTemperature1))
        query += format.arg("soil_temperature_1");
    if (columns.extra.testFlag(EC_SoilTemperature2))
        query += format.arg("soil_temperature_2");
    if (columns.extra.testFlag(EC_SoilTemperature3))
        query += format.arg("soil_temperature_3");
    if (columns.extra.testFlag(EC_SoilTemperature4))
        query += format.arg("soil_temperature_4");
    if (columns.extra.testFlag(EC_ExtraHumidity1))
        query += format.arg("extra_humidity_1");
    if (columns.extra.testFlag(EC_ExtraHumidity2))
        query += format.arg("extra_humidity_2");
    if (columns.extra.testFlag(EC_ExtraTemperature1))
        query += format.arg("extra_temperature_1");
    if (columns.extra.testFlag(EC_ExtraTemperature2))
        query += format.arg("extra_temperature_2");
    if (columns.extra.testFlag(EC_ExtraTemperature3))
        query += format.arg("extra_temperature_3");
    return query;
}

QString WebCacheDB::buildSelectForColumns(SampleColumns columns)
{
    QString selectPart = "select time_stamp";

    SampleColumns cols;
    cols.standard = columns.standard & ~SC_Timestamp;
    cols.extra = columns.extra;

    selectPart += buildColumnList(cols, ", %1");

    if (columns.standard.testFlag(SC_Pressure)) {
        selectPart += ", coalesce(mean_sea_level_pressure, absolute_pressure) as pressure";
    }

    return selectPart;
}

int WebCacheDB::getNonAggregatedRowCount(int stationId, QDateTime startTime, QDateTime endTime) {
    QString qry = "select count(*) from sample where station_id = :station_id "
            "and time_stamp >= :start_time and time_stamp <= :end_time";

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);
    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", TO_UNIX_TIME(startTime));
    query.bindValue(":end_time", TO_UNIX_TIME(endTime));
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

    SampleColumns noCols;
    noCols.standard = SC_NoColumns;
    noCols.extra = EC_NoColumns;

    QString qry = buildAggregatedSelect(noCols,
                                        aggregateFunction,
                                        groupType);

    qry = "select count(*) as cnt from ( " + qry + " ) as x ";

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);

    query.bindValue(":station_id", stationId);
    query.bindValue(":stationIdB", stationId);
    query.bindValue(":stationIdC", stationId);
    query.bindValue(":start_time", TO_UNIX_TIME(startTime));
    query.bindValue(":end_time", TO_UNIX_TIME(endTime));

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
    sql += " from sample where station_id = :station_id "
             "and time_stamp >= :start_time and time_stamp <= :end_time"
             " order by time_stamp asc";

    qDebug() << "WCDB Simple Select: " << sql;

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

    if (columns.standard.testFlag(SC_Timestamp))
        query += ", min(iq.time_stamp) as time_stamp ";

    // It doesn't make sense to sum certain fields (like temperature).
    // So when AF_Sum or AF_RunningTotal is specified we'll apply that only
    // to the columns were it makes sense and select an average for all the
    // others.
    if (function == AF_Sum || function == AF_RunningTotal) {
        // Figure out which columns we can sum
        SampleColumns summables;
        summables.standard = columns.standard & SUMMABLE_COLUMNS;
        summables.extra = columns.extra & EXTRA_SUMMABLE_COLUMNS;

        // And which columns we can't
        SampleColumns nonSummables;
        nonSummables.standard = columns.standard & ~SUMMABLE_COLUMNS;
        nonSummables.extra = columns.extra & ~EXTRA_SUMMABLE_COLUMNS;
        nonSummables.standard = nonSummables.standard & ~SC_Timestamp; // we don't want timestamp either

        // Sum the summables
        if (summables.standard != SC_NoColumns || summables.extra != EC_NoColumns) {
            query += buildColumnList(summables, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"));
        }

        // And just average the nonsummables(we have to apply some sort of
        // aggregate or the grouping will fail)
        if (nonSummables.standard != SC_NoColumns || nonSummables.extra != EC_NoColumns) {
            query += buildColumnList(nonSummables, ", avg(iq.%1) as %1 ");
        }

        // Handle SC_pressure specially
        if (nonSummables.standard.testFlag(SC_Pressure)) {
            query += ", avg(coalesce(iq.mean_sea_level_pressure, iq.absolute_pressure)) as pressure ";
        }
    } else {
        SampleColumns cols;
        cols.standard = columns.standard & ~SC_Timestamp;
        cols.extra = columns.extra;
        query += buildColumnList(cols, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"));

        // Handle SC_pressure specially
        if (cols.standard.testFlag(SC_Pressure)) {
            query += QString(", %1(coalesce(iq.mean_sea_level_pressure, iq.absolute_pressure)) as pressure ").arg(fn);
        }
    }

    query += " from (select ";

    if (groupType == AGT_Custom) {
        query += "(cur.time_stamp / :groupSeconds) AS quadrant ";
    } else if (groupType == AGT_Hour) {
        query += "strftime('%Y-%m-%d %H:00:00', cur.time_stamp, 'unixepoch', 'localtime') as quadrant";
    } else if (groupType == AGT_Day) {
        query += "date(cur.time_stamp, 'unixepoch', 'localtime') as quadrant";
    } else if (groupType == AGT_Month) {
        //query += "strftime('%s', julianday(datetime(cur.time_stamp, 'unixepoch', 'start of month'))) as quadrant";
        query += "strftime('%Y-%m-01 00:00:00', cur.time_stamp, 'unixepoch', 'localtime') as quadrant";
        // In postgres this would be: extract(epoch from date_trunc('month', cur.time_stamp))::integer as quadrant
    } else { // year
        //query += "strftime('%s', julianday(datetime(cur.time_stamp, 'unixepoch', 'start of year'))) as quadrant";
        query += "strftime('%Y-01-01 00:00:00', cur.time_stamp, 'unixepoch', 'localtime') as quadrant";
        // In postgres this would be: extract(epoch from date_trunc('year', cur.time_stamp))::integer as quadrant
    }

    query += buildColumnList(columns, ", cur.%1 ");

    query += " from sample cur, sample prev"
             " where cur.time_stamp <= :end_time"
             " and cur.time_stamp >= :start_time"
             " and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp"
             "        and station_id = :station_id )"
             " and cur.station_id = :stationIdB "
             " and prev.station_id = :stationIdC "
             " order by cur.time_stamp asc) as iq "
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

    qDebug() << "WCDB Aggregated Select:" << qry;

    QSqlQuery query(sampleCacheDb);
    query.prepare(qry);

    query.bindValue(":stationIdB", stationId);
    query.bindValue(":stationIdC", stationId);

    if (groupType == AGT_Custom)
        query.bindValue(":groupSeconds", groupMinutes * 60);

    return query;
}

double nullableVariantDouble(QVariant v) {
    if (v.isNull()) {
        return qQNaN();
    }
    bool ok;
    double result = v.toDouble(&ok);
    if (!ok) {
        return qQNaN();
    }
    return result;
}

int WebCacheDB::getSampleInterval(QString url) {
    return getSampleInterval(getStationId(url));
}

int WebCacheDB::getSampleInterval(int stationId) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select sample_interval * 60 from station where station_id = :id");
    query.bindValue(":id", stationId);
    query.exec();

    if (!query.isActive()) {
        qWarning() << "Sample interval lookup failed.";
        qWarning() << query.lastError().driverText() << query.lastError().databaseText();
        return -1;
    }
    else {
        query.first();
        return query.value(0).toInt();
    }

    qWarning() << "Sample interval lookup failed - multiple rows returned";
    return -1;
}

SampleSet WebCacheDB::retrieveDataSet(QString stationUrl,
                                      QDateTime startTime,
                                      QDateTime endTime,
                                      SampleColumns columns,
                                      AggregateFunction aggregateFunction,
                                      AggregateGroupType aggregateGroupType,
                                      uint32_t groupMinutes,
                                      AbstractProgressListener *progressListener) {
    QSqlQuery query(sampleCacheDb);
    SampleSet samples;

    if (!ready) {
        samples.sampleCount = 0;
        return samples;
    }

    int stationId = getStationId(stationUrl);
    sample_range_t range = getSampleRange(stationUrl);

    if (range.isValid) {
        if (startTime < range.start) {
            startTime = range.start;
        }
        if (endTime > range.end) {
            endTime = range.end;
        }
    } else {
        qWarning() << "Sample range invalid";
    }

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

    int interval = -1;
    if (aggregateFunction == AF_None || aggregateGroupType == AGT_None) {
        query = buildBasicSelectQuery(columns);
        interval = getSampleInterval(stationId);
    } else {
        // Aggregated queries always require the timestamp column.
        SampleColumns cols;
        cols.standard = columns.standard | SC_Timestamp;
        cols.extra = columns.extra;
        query = buildAggregatedSelectQuery(cols, stationId,
                                           aggregateFunction,
                                           aggregateGroupType, groupMinutes);
        interval = groupMinutes * 60;
    }

    query.bindValue(":station_id", stationId);
    query.bindValue(":start_time", TO_UNIX_TIME(startTime));
    query.bindValue(":end_time", TO_UNIX_TIME(endTime));
    query.exec();

    QDateTime lastTs = startTime;
    bool gapGeneration = interval > 0;
    int thresholdSeconds = 2*interval;
    qDebug() << "Threshold" << thresholdSeconds << "interval" << interval << "gap generation" << gapGeneration;
    qDebug() << "Loading query result set...";
    int currentRow = 0;
    double totalRows = count;
    qDebug() << "Total Rows: " << query.numRowsAffected();

    if (progressListener != NULL) {
        progressListener->setSubtaskName(tr("Loading Results..."));
        progressListener->setMaximum(progressListener->maximum() + count / 100);
    }

    if (query.first()) {

        double previousRainfall = 0.0;
        double previousEvapotranspiration = 0.0;

        // At least one record came back. Go pull all of them out and dump
        // them in the SampleSet.
        do {
            currentRow++;
            double position = currentRow / totalRows * 100.0;
            if (currentRow % 100 == 0) {
                qDebug() << "Loading rows: " << position << "%";
                if (progressListener != NULL) {
                    progressListener->setValue(progressListener->value() + 1);
                }
                QCoreApplication::processEvents();
            }
            QSqlRecord record = query.record();

            auto timeStamp = record.value("time_stamp").toLongLong();

            QDateTime ts = FROM_UNIX_TIME(timeStamp);
            if (gapGeneration) {
                if (ts > lastTs.addSecs(thresholdSeconds)) {
                    // We skipped at least one sample! Generate same fake null samples.
                    qDebug() << "Inserting null samples from" << lastTs << "to" << ts << "...";
                    AppendNullSamples(samples,
                                      columns,
                                      lastTs.addSecs(interval),
                                      ts.addSecs(-1 * interval),
                                      interval);
                }
            }
            lastTs = ts;

            samples.timestampUnix.append(timeStamp);
            samples.timestamp.append(timeStamp);

            if (columns.standard.testFlag(SC_Temperature))
                samples.temperature.append(nullableVariantDouble(record.value("temperature")));

            if (columns.standard.testFlag(SC_DewPoint))
                samples.dewPoint.append(nullableVariantDouble(record.value("dew_point")));

            if (columns.standard.testFlag(SC_ApparentTemperature))
                samples.apparentTemperature.append(nullableVariantDouble(record.value("apparent_temperature")));

            if (columns.standard.testFlag(SC_WindChill))
                samples.windChill.append(nullableVariantDouble(record.value("wind_chill")));

            if (columns.standard.testFlag(SC_IndoorTemperature))
                samples.indoorTemperature.append(nullableVariantDouble(record.value("indoor_temperature")));

            if (columns.standard.testFlag(SC_Humidity))
                samples.humidity.append(nullableVariantDouble(record.value("relative_humidity")));

            if (columns.standard.testFlag(SC_IndoorHumidity))
                samples.indoorHumidity.append(nullableVariantDouble(record.value("indoor_relative_humidity")));

            if (columns.standard.testFlag(SC_Pressure))
                samples.pressure.append(nullableVariantDouble(record.value("pressure")));

            if (columns.standard.testFlag(SC_AbsolutePressure))
                samples.absolutePressure.append(nullableVariantDouble(record.value("absolute_pressure")));

            if (columns.standard.testFlag(SC_MeanSeaLevelPressure))
                samples.meanSeaLevelPressure.append(nullableVariantDouble(record.value("mean_sea_level_pressure")));

            if (columns.standard.testFlag(SC_Rainfall)) {
                double value = nullableVariantDouble(record.value("rainfall"));

                // Because SQLite doesn't support window functions we have to
                // calculate the running total manually. We'll only bother doing
                // it for rainfall & evapotranspiration as it doesn't really make
                // sense for the rest (the PostgreSQL backend supports it on
                // everyhing only because its easier than doing it on one column
                // only).
                if (aggregateFunction == AF_RunningTotal) {
                    previousRainfall += value;
                    samples.rainfall.append(previousRainfall);
                } else {
                    samples.rainfall.append(value);
                }
            }

            if (columns.standard.testFlag(SC_AverageWindSpeed))
                samples.averageWindSpeed.append(nullableVariantDouble(record.value("average_wind_speed")));

            if (columns.standard.testFlag(SC_GustWindSpeed))
                samples.gustWindSpeed.append(nullableVariantDouble(record.value("gust_wind_speed")));

            if (columns.standard.testFlag(SC_WindDirection))
                // Wind direction can be null.
                if (!record.value("wind_direction").isNull())
                    samples.windDirection[timeStamp] =
                            record.value("wind_direction").toUInt();

            if (columns.standard.testFlag(SC_GustWindDirection))
                // Wind direction can be null.
                if (!record.value("gust_wind_direction").isNull())
                    samples.gustWindDirection[timeStamp] =
                            record.value("gust_wind_direction").toUInt();

            if (columns.standard.testFlag(SC_SolarRadiation))
                samples.solarRadiation.append(nullableVariantDouble(record.value("solar_radiation")));

            if (columns.standard.testFlag(SC_UV_Index))
                samples.uvIndex.append(nullableVariantDouble(record.value("uv_index")));

            if (columns.standard.testFlag(SC_Reception))
                samples.reception.append(nullableVariantDouble(record.value("reception")));

            if (columns.standard.testFlag(SC_HighTemperature))
                samples.highTemperature.append(nullableVariantDouble(record.value("high_temperature")));

            if (columns.standard.testFlag(SC_LowTemperature))
                samples.lowTemperature.append(nullableVariantDouble(record.value("low_temperature")));

            if (columns.standard.testFlag(SC_HighRainRate))
                samples.highRainRate.append(nullableVariantDouble(record.value("high_rain_rate")));

            if (columns.standard.testFlag(SC_Evapotranspiration)) {
                double value = nullableVariantDouble(record.value("evapotranspiration"));

                // Because SQLite doesn't support window functions we have to
                // calculate the running total manually. We'll only bother doing
                // it for rainfall & evapotranspiration as it doesn't really make
                // sense for the rest (the PostgreSQL backend supports it on
                // everyhing only because its easier than doing it on one column
                // only).
                if (aggregateFunction == AF_RunningTotal) {
                    previousEvapotranspiration += value;
                    samples.evapotranspiration.append(previousEvapotranspiration);
                } else {
                    samples.evapotranspiration.append(value);
                }
            }

            if (columns.standard.testFlag(SC_HighSolarRadiation))
                samples.highSolarRadiation.append(nullableVariantDouble(record.value("high_solar_radiation")));

            if (columns.standard.testFlag(SC_HighUVIndex))
                samples.highUVIndex.append(nullableVariantDouble(record.value("high_uv_index")));

            if (columns.standard.testFlag(SC_ForecastRuleId))
                samples.forecastRuleId.append(record.value("forecast_rule_id").toInt());

            if (columns.extra.testFlag(EC_LeafWetness1))
                samples.leafWetness1.append(nullableVariantDouble(record.value("leaf_wetness_1")));

            if (columns.extra.testFlag(EC_LeafWetness2))
                samples.leafWetness2.append(nullableVariantDouble(record.value("leaf_wetness_2")));

            if (columns.extra.testFlag(EC_LeafTemperature1))
                samples.leafTemperature1.append(nullableVariantDouble(record.value("leaf_temperature_1")));

            if (columns.extra.testFlag(EC_LeafTemperature2))
                samples.leafTemperature2.append(nullableVariantDouble(record.value("leaf_temperature_2")));

            if (columns.extra.testFlag(EC_SoilMoisture1))
                samples.soilMoisture1.append(nullableVariantDouble(record.value("soil_moisture_1")));

            if (columns.extra.testFlag(EC_SoilMoisture2))
                samples.soilMoisture2.append(nullableVariantDouble(record.value("soil_moisture_2")));

            if (columns.extra.testFlag(EC_SoilMoisture3))
                samples.soilMoisture3.append(nullableVariantDouble(record.value("soil_moisture_3")));

            if (columns.extra.testFlag(EC_SoilMoisture4))
                samples.soilMoisture4.append(nullableVariantDouble(record.value("soil_moisture_4")));

            if (columns.extra.testFlag(EC_SoilTemperature1))
                samples.soilTemperature1.append(nullableVariantDouble(record.value("soil_temperature_1")));

            if (columns.extra.testFlag(EC_SoilTemperature2))
                samples.soilTemperature2.append(nullableVariantDouble(record.value("soil_temperature_2")));

            if (columns.extra.testFlag(EC_SoilTemperature3))
                samples.soilTemperature3.append(nullableVariantDouble(record.value("soil_temperature_3")));

            if (columns.extra.testFlag(EC_SoilTemperature4))
                samples.soilTemperature4.append(nullableVariantDouble(record.value("soil_temperature_4")));

            if (columns.extra.testFlag(EC_ExtraHumidity1))
                samples.extraHumidity1.append(nullableVariantDouble(record.value("extra_humidity_1")));

            if (columns.extra.testFlag(EC_ExtraHumidity2))
                samples.extraHumidity2.append(nullableVariantDouble(record.value("extra_humidity_2")));

            if (columns.extra.testFlag(EC_ExtraTemperature1))
                samples.extraTemperature1.append(nullableVariantDouble(record.value("extra_temperature_1")));

            if (columns.extra.testFlag(EC_ExtraTemperature2))
                samples.extraTemperature2.append(nullableVariantDouble(record.value("extra_temperature_2")));

            if (columns.extra.testFlag(EC_ExtraTemperature3))
                samples.extraTemperature3.append(nullableVariantDouble(record.value("extra_temperature_3")));

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

    qDebug() << "Finished loading result set. Returning...";
    if (progressListener != NULL) {
        progressListener->setSubtaskName("Loading complete.");
    }

    return samples;
}

void WebCacheDB::clearSamples() {

    if (!ready) {
        return;
    }

    QSqlQuery query(sampleCacheDb);
    query.exec("delete from sample");
    query.exec("delete from data_file");;
    sampleCacheDb.close();
    sampleCacheDb.open();
    query.exec("vacuum");
}

void WebCacheDB::clearImages() {

    if (!ready) {
        return;
    }

    QSqlQuery query(sampleCacheDb);
    query.exec("delete from image");
    query.exec("delete from image_set");
    sampleCacheDb.close();
    sampleCacheDb.open();
    query.exec("vacuum");
}

bool WebCacheDB::stationKnown(QString url) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select station_id from station where code = :url");
    query.bindValue(":url", url);
    query.exec();
    return query.first();
}

bool WebCacheDB::stationIsArchived(QString url) {
    QSqlQuery query(sampleCacheDb);
    query.prepare("select archived from station where code = :url");
    query.bindValue(":url", url);
    query.exec();
    if (query.first()) {
        return query.record().field(0).value().toBool();
    }
    return false;
}

void WebCacheDB::updateStation(QString url, QString title, QString description,
                               QString type_code, int interval, float latitude,
                               float longitude, float altitude, bool solar,
                               int davis_broadcast_id,
                               QMap<ExtraColumn, QString> extraColumnNames,
                               bool archived, QDateTime archivedTime,
                               QString archivedMessage, unsigned int apiLevel) {
    if (!ready) {
        return;
    }

    /* TODO:
     * Might be worth checking that this station update won't un-archive the station.
     * If the DB is currently archived and the incoming station data says its not archived
     * then we've no idea of the validity of our cache and we really out to either error
     * or dump the entire cache for this station.
     *
     * if (stationIsArchived(url) && !archived) {
     *      qWarning() << "Station has been unarchived! This is not allowed - forcing cache purge";
     *      // delete all samples, data files and images for the station
     * }
     *
     */


    qDebug() << "Updating station info for" << url;

    int stationId = getStationId(url);
    station_info_t info = getStationInfo(url);

    QSqlQuery query(sampleCacheDb);

    if (info.apiLevel == 0 && apiLevel >= 20220210) {
        // 20220210 marks mean sea level pressure and absolute pressure being
        // stored separately. We don't know what sort of data we're currently
        // storing for this station so we need to dump the whole lot.

        qDebug() << "Station API level has changed from 0 - need to drop samples due to ABS/MSL pressure separation...";

        query.prepare("delete from sample where station_id = :station_id");
        query.bindValue(":station_id", stationId);
        if (!query.exec()) {
            qWarning() << "Failed to drop samples for station! Error:" << query.lastError().databaseText();
        } else {
            query.prepare("delete from data_file where station = :station_id");
            query.bindValue(":station_id", stationId);
            if (!query.exec()) {
                qWarning() << "Failed to drop data files for station! Error:" << query.lastError().databaseText();
            } else {
                qDebug() << "Sample cache cleared successfully for station!";
            }
        }
    }

    query.prepare("update station set title = :title, description = :description, "
                  "station_type_id = (select station_type_id from station_type where lower(code) = :type_code), "
                  "sample_interval = :interval, "
                  "latitude = :latitude, longitude = :longitude, altitude = :altitude, "
                  "solar_available = :solar, davis_broadcast_id = :broadcast_id,  "
                  "archived = :archived, archived_time = :archived_time, "
                  "archived_message = :archived_message, api_level = :api_level "
                  "where station_id = :station_id");
    query.bindValue(":title", title);
    query.bindValue(":description", description);
    query.bindValue(":type_code", type_code.toLower());
    query.bindValue(":interval", interval);

    query.bindValue(":altitude", altitude);
    query.bindValue(":solar", solar);
    query.bindValue(":station_id", stationId);

    if (latitude == FLT_MAX) {
        query.bindValue(":latitude", QVariant(QVariant::Double));
    } else {
        query.bindValue(":latitude", latitude);
    }

    if (longitude == FLT_MAX) {
        query.bindValue(":longitude", QVariant(QVariant::Double));
    } else {
        query.bindValue(":longitude", longitude);
    }

    if (davis_broadcast_id > 0) {
        query.bindValue(":broadcast_id", davis_broadcast_id);
    } else {
        query.bindValue(":broadcast_id", QVariant(QVariant::Int));
    }

    query.bindValue(":archived", archived);
    query.bindValue(":archived_time", TO_UNIX_TIME(archivedTime));
    query.bindValue(":archived_message", archivedMessage);
    query.bindValue(":api_level", apiLevel);

    if (!query.exec()) {
        qWarning() << "Failed to update station config! Error:" << query.lastError().databaseText();
    }

    qDebug() << "Update sensor config";
    qDebug() << "Disable all sensors...";
    query.prepare("delete from sensor_config where station_id = :station_id");
    query.bindValue(":station_id", stationId);
    query.exec();

    qDebug() << "Enable currently configured sensors...";
    query.prepare("insert into sensor_config(station_id, sensor, enabled, name) "
                  "values(:station_id, :sensor, 1, :name) ");

    foreach (ExtraColumn col, extraColumnNames.keys()) {
        QString name = extraColumnNames[col];
        QString sensorName = "";
        switch (col) {
        case EC_ExtraHumidity1:
            sensorName = "extra_humidity_1";
            break;
        case EC_ExtraHumidity2:
            sensorName = "extra_humidity_2";
            break;
        case EC_ExtraTemperature1:
            sensorName = "extra_temperature_1";
            break;
        case EC_ExtraTemperature2:
            sensorName = "extra_temperature_2";
            break;
        case EC_ExtraTemperature3:
            sensorName = "extra_temperature_3";
            break;
        case EC_LeafTemperature1:
            sensorName = "leaf_temperature_1";
            break;
        case EC_LeafTemperature2:
            sensorName = "leaf_temperature_2";
            break;
        case EC_LeafWetness1:
            sensorName = "leaf_wetness_1";
            break;
        case EC_LeafWetness2:
            sensorName = "leaf_wetness_2";
            break;
        case EC_SoilMoisture1:
            sensorName = "soil_moisture_1";
            break;
        case EC_SoilMoisture2:
            sensorName = "soil_moisture_2";
            break;
        case EC_SoilMoisture3:
            sensorName = "soil_moisture_3";
            break;
        case EC_SoilMoisture4:
            sensorName = "soil_moisture_4";
            break;
        case EC_SoilTemperature1:
            sensorName = "soil_temperature_1";
            break;
        case EC_SoilTemperature2:
            sensorName = "soil_temperature_2";
            break;
        case EC_SoilTemperature3:
            sensorName = "soil_temperature_3";
            break;
        case EC_SoilTemperature4:
            sensorName = "soil_temperature_4";
            break;
        case EC_NoColumns:
            continue;
        }

        qDebug() << "Enable sensor" << sensorName << "with name" << name;
        query.bindValue(":station_id", stationId);
        query.bindValue(":name", name);
        query.bindValue(":sensor", sensorName);
        query.exec();
    }

    qDebug() << "Sensor config complete.";

    sampleCacheDb.commit();
    qDebug() << "Station updated";
}

void WebCacheDB::updateStationGaps(QString url, QList<sample_gap_t> gaps) {
    int station_id = getStationId(url);
    QString query = "insert into sample_gap(station_id, start_time, end_time, "
                    "                       missing_sample_count, label) "
                    "values(?, ?, ?, ?, ?) "
                    "on conflict (station_id, start_time, end_time) "
                    "do update set label=excluded.label";

    QVariantList stationIds;
    QVariantList startTimes;
    QVariantList endTimes;
    QVariantList missingSampleCounts;
    QVariantList labels;

    QElapsedTimer timer;
    timer.start();
    QSqlDatabase db = sampleCacheDb;
    db.transaction();

    foreach (sample_gap_t gap, gaps) {
        stationIds << station_id;
        startTimes << TO_UNIX_TIME(gap.start_time);
        endTimes << TO_UNIX_TIME(gap.end_time);
        missingSampleCounts << gap.missing_samples;
        labels << gap.label;
    }

    QSqlQuery q(sampleCacheDb)  ;
    q.prepare(query);
    q.addBindValue(stationIds);
    q.addBindValue(startTimes);
    q.addBindValue(endTimes);
    q.addBindValue(missingSampleCounts);
    q.addBindValue(labels);

    if (!q.execBatch()) {
        qWarning() << "Failed to store one or more station gaps!";
    }
    if (!db.commit()) {
        qWarning() << "Transaction commit failed. Gaps not stored.";
    }
    qDebug() << "Transaction committed at " << timer.elapsed() << "msecs";

    optimise();
}


QList<sample_gap_t> WebCacheDB::getStationGaps(QString url) {
    QList<sample_gap_t> gaps;

    QSqlQuery q(sampleCacheDb);
    q.prepare("select sg.start_time, sg.end_time, sg.missing_sample_count, sg.text "
              "from sample_gap sg "
              "inner join station s on sg.station_id = s.station_id "
              "where lower(s.code) = lower(:station_code)");
    q.bindValue(":station_code", url);

    if (q.exec()) {
        do {
            QSqlRecord rec = q.record();
            sample_gap_t gap;
            gap.start_time = FROM_UNIX_TIME(rec.field(0).value().toLongLong());
            gap.end_time = FROM_UNIX_TIME(rec.field(1).value().toLongLong());
            gap.missing_samples = rec.field(2).value().toInt();
            gap.label = rec.field(3).value().toString();
            gaps.append(gap);
        } while (q.next());
    }

    return gaps;
}


bool WebCacheDB::sampleGapIsKnown(QString url, QDateTime gapStart, QDateTime gapEnd) {
    QSqlQuery q(sampleCacheDb);
    q.prepare("select sample_gap_id "
              "from sample_gap sg "
              "inner join station s on s.station_id = sg.station_id "
              "where lower(s.code) = lower(:station_code)"
              "  and sg.start_time <= :start_ts "
              "  and sg.end_time >= :end_ts");
    q.bindValue(":station_code", url);
    q.bindValue(":start_ts", TO_UNIX_TIME(gapStart));
    q.bindValue(":end_ts", TO_UNIX_TIME(gapEnd));
    q.exec();
    return q.first();
}


QMap<ExtraColumn, QString> WebCacheDB::getExtraColumnNames(QString url) {
    int station_id = getStationId(url);

    QMap<ExtraColumn, QString> columns;

    QSqlQuery q(sampleCacheDb);
    q.prepare("select name, sensor from sensor_config where station_id = :station_id and enabled = 1");
    q.bindValue(":station_id", station_id);
    if (q.exec()) {
        do {
            QSqlRecord record = q.record();
            QString name = record.field(0).value().toString();
            QString sensor = record.field(1).value().toString();

            ExtraColumn column = EC_NoColumns;
            if (sensor == "extra_humidity_1")         column = EC_ExtraHumidity1;
            else if (sensor == "extra_humidity_2")    column = EC_ExtraHumidity2;
            else if (sensor == "extra_temperature_1") column = EC_ExtraTemperature1;
            else if (sensor == "extra_temperature_2") column = EC_ExtraTemperature2;
            else if (sensor == "extra_temperature_3") column = EC_ExtraTemperature3;
            else if (sensor == "leaf_wetness_1")      column = EC_LeafWetness1;
            else if (sensor == "leaf_wetness_2")      column = EC_LeafWetness2;
            else if (sensor == "leaf_temperature_1")  column = EC_LeafTemperature1;
            else if (sensor == "leaf_temperature_2")  column = EC_LeafTemperature2;
            else if (sensor == "soil_moisture_1")     column = EC_SoilMoisture1;
            else if (sensor == "soil_moisture_2")     column = EC_SoilMoisture2;
            else if (sensor == "soil_moisture_3")     column = EC_SoilMoisture3;
            else if (sensor == "soil_moisture_4")     column = EC_SoilMoisture4;
            else if (sensor == "soil_temperature_1")  column = EC_SoilTemperature1;
            else if (sensor == "soil_temperature_2")  column = EC_SoilTemperature2;
            else if (sensor == "soil_temperature_3")  column = EC_SoilTemperature3;
            else if (sensor == "soil_temperature_4")  column = EC_SoilTemperature4;
            else continue;

            columns[column] = name;
        } while (q.next());
    }

    return columns;
}

bool WebCacheDB::solarAvailable(QString url) {
    QSqlQuery q(sampleCacheDb);
    q.prepare("select solar_available from station where code = :url");
    q.bindValue(":url", url);
    if (q.exec()) {
        if (q.first()) {
            return q.record().field(0).value().toBool();
        }
    }
    return false;
}

QString WebCacheDB::hw_type(QString url) {
    QSqlQuery q(sampleCacheDb);
    q.prepare("select hwt.code from station_type hwt "
              "inner join station stn on stn.station_type_id = hwt.station_type_id "
              "where stn.code = :url");
    q.bindValue(":url", url);
    if (q.exec()) {
        if (q.first()) {
            return q.record().field(0).value().toString().toLower();
        }
    }
    return "generic";
}

station_info_t WebCacheDB::getStationInfo(QString url) {
    station_info_t info;
    info.isValid = false;

    if (!ready) {
        return info;
    }

    // TODO: Populate hardware type!

    QSqlQuery query(sampleCacheDb);
    query.prepare("select s.title, s.description, s.latitude, s.longitude, "
                  "s.altitude, s.solar_available, s.davis_broadcast_id, "
                  "st.code as type_code, s.api_level "
                  "from station s inner join station_type st on st.station_type_id = s.station_type_id "
                  "where s.code = :url");
    query.bindValue(":url", url);
    if (query.exec()) {
        if (query.first()) {
            info.isValid = true;
            if (query.record().value("latitude").isNull() || query.record().value("longitude").isNull()) {
                info.coordinatesPresent = false;
            } else {
                info.coordinatesPresent = true;
                info.latitude = query.record().value("latitude").toFloat();
                info.longitude = query.record().value("longitude").toFloat();
            }

            info.title = query.record().value("title").toString();
            info.description = query.record().value("description").toString();
            info.altitude = query.record().value("altitude").toFloat();
            info.hasSolarAndUV = query.record().value("solar_available").toBool();

            info.apiLevel = query.record().value("api_level").toUInt();

            QVariant broadcastId = query.record().value("davis_broadcast_id");
            info.isWireless = !broadcastId.isNull() && broadcastId.toInt() != -1;

            QString typeCode = query.record().value("type_code").toString().toUpper();
            if (typeCode == "DAVIS") {
                info.hardwareType = HW_DAVIS;
            } else if (typeCode == "FOWH1080") {
                info.hardwareType = HW_FINE_OFFSET;
            } else if (typeCode == "GENERIC") {
                info.hardwareType = HW_GENERIC;
            } else {
                qWarning() << "Unrecognised hardware type code" << typeCode << ". Treating has GENERIC.";
                info.hardwareType = HW_GENERIC;
            }
        }
    }

    return info;
}

sample_range_t WebCacheDB::getSampleRange(QString url) {
    sample_range_t info;
    info.isValid = false;

    int id = getStationId(url);
    if (id < 0) {
        return info;
    }

    QSqlQuery query(sampleCacheDb);
    query.prepare("select max(time_stamp) as end, min(time_stamp) as start from sample where station_id = :id");
    query.bindValue(":id", id);
    if (query.exec()) {
        if (query.first()) {
            info.start = FROM_UNIX_TIME(query.record().value("start").toLongLong());
            info.end = FROM_UNIX_TIME(query.record().value("end").toLongLong());
            info.isValid = info.start < info.end;
            qDebug() << "Start" << info.start << "End" << info.end << "Valid" << info.isValid << "Station" << id;
            return info;
        }
    }

    return info;
}

void WebCacheDB::optimise() {
    QSqlQuery q(sampleCacheDb);
    if (!q.exec("pragma optimize;")) {
        qWarning() << "DB Optimisation failed";
    }
}

void WebCacheDB::updateImageDateList(QString stationCode, QMap<QString, QMap<QDate, int> > dates) {
    int stationId = getStationId(stationCode);

    qDebug() << "Station" << stationCode << "ID" << stationId;

    QVariantList sourceIds;
    QVariantList dateValues;
    QVariantList dateCounts;

    QElapsedTimer timer;
    timer.start();
    QSqlDatabase db = sampleCacheDb;
    db.transaction();

    foreach(QString sourceCode, dates.keys()) {
        int sourceId = getImageSourceId(stationId, sourceCode);
        QSqlQuery q(sampleCacheDb);
        q.prepare("delete from image_dates where image_source_id = :id");
        q.bindValue(":id", sourceId);
        if (!q.exec()) {
            qWarning() << "Failed to drop cached dates for source " << sourceCode << "with id" << sourceId;
        }


        foreach(QDate date, dates[sourceCode].keys()) {
            sourceIds.append(sourceId);
            dateValues.append(date.toString(Qt::ISODate));

            int count = dates[sourceCode][date];
            if (count < 0) {
                dateCounts.append(QVariant());
            } else {
                dateCounts.append(count);
            }

//            qDebug() << "Station Code " << stationCode << "has ID" << stationId
//                     << "source code" << sourceCode << "has ID" << sourceId
//                     << "date" << date;
        }
    }

    QSqlQuery q(sampleCacheDb);
    q.prepare("insert into image_dates(image_source_id, date, server_image_count) values(?, ?, ?);");
    q.addBindValue(sourceIds);
    q.addBindValue(dateValues);
    q.addBindValue(dateCounts);
    if (!q.execBatch()) {
        qWarning() << "Failed to store image dates";
    }

    if (!db.commit()) {
        qWarning() << "Transaction commit failed. Data not cached.";
    }
    qDebug() << "Transaction committed at " << timer.elapsed() << "msecs";

    optimise();

}

bool WebCacheDB::imageSourceDateIsCached(QString stationUrl, QString sourceCode, QDate date) {
    int stationId = getStationId(stationUrl);
    int sourceId = getImageSourceId(stationId, sourceCode);

    QSqlQuery q(sampleCacheDb);
    q.prepare("select server_image_count from image_dates where image_source_id = :sourceId and date = :date");
    q.bindValue(":sourceId", sourceId);
    q.bindValue(":date", date.toString(Qt::ISODate));
    q.exec();
    if (!q.first()) {
        qWarning() << "Failed to get server image count from cache database";
        return false;
    }

    QVariant serverCountV = q.record().value("server_image_count");
    if (serverCountV.isNull()) {
        return false;
    }
    int serverCount = serverCountV.toInt();

    // We're not interested in any images from the temporary set as they've only
    // got partial metadata.
    int temporarySetId = getImageSetId(TEMPORARY_IMAGE_SET + stationUrl);

    q.prepare("select count(*) as count from image i where date = :date and source = :sourceId and image_set <> :temporarySetId");
    q.bindValue(":date", date.toString(Qt::ISODate));
    q.bindValue(":sourceId", sourceId);
    q.bindValue(":temporarySetId", temporarySetId);
    q.exec();
    if (!q.first()) {
        qDebug() << "Failed to get day image count from cache DB";
        return false;
    }

    QVariant cacheCountV = q.record().value("count");
    if (cacheCountV.isNull()) {
        qDebug() << "No day image count from cache.";
        return false;
    }

    int cacheCount = cacheCountV.toInt();

    qDebug() << "Server count" << serverCount << "Cache count" << cacheCount;

    return cacheCount == serverCount;
}
