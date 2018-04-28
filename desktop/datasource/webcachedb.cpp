#include "webcachedb.h"

#include <QtSql>
#include <QtDebug>
#include <QDesktopServices>

#define SAMPLE_CACHE "sample-cache"
#define sampleCacheDb QSqlDatabase::database(SAMPLE_CACHE)

#define TEMPORARY_IMAGE_SET "::temporary_image_set:"

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
            gustWindSpeeds, windDirections, solarRadiations, uvIndexes,
            receptions, highTemperatures, lowTemperatures, highRainRates,
            gustWindDirections, evapotranspirations, highSolarRadiations,
            highUVIndexes, forecastRuleIds;

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
        receptions.append(samples.reception.at(i));
        highTemperatures.append(samples.highTemperature.at(i));
        lowTemperatures.append(samples.lowTemperature.at(i));
        highRainRates.append(samples.highRainRate.at(i));
        forecastRuleIds.append(samples.forecastRuleId.at(i));

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
            solarRadiations.append(samples.solarRadiation.at(i));
            uvIndexes.append(samples.uvIndex.at(i));
            highSolarRadiations.append(samples.highSolarRadiation.at(i));
            highUVIndexes.append(samples.highUVIndex.at(i));
            evapotranspirations.append(samples.evapotranspiration.at(i));
        } else {
            // The lists still need to be populated for the query. As we don't
            // have any real data to put in them we'll use 0.
            solarRadiations.append(0);
            uvIndexes.append(0);
            highSolarRadiations.append(0);
            highUVIndexes.append(0);
            evapotranspirations.append(0);
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
                    "wind_direction, solar_radiation, uv_index,reception, "
                    "high_temperature, low_temperature, high_rain_rate, "
                    "gust_wind_direction, evapotranspiration, "
                    "high_solar_radiation, high_uv_index, forecast_rule_id) "
                    "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                    "?, ?, ?, ?, ?, ?, ?, ?, ?);");
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
        query.addBindValue(receptions);
        query.addBindValue(highTemperatures);
        query.addBindValue(lowTemperatures);
        query.addBindValue(highRainRates);
        query.addBindValue(gustWindDirections);
        query.addBindValue(evapotranspirations);
        query.addBindValue(highSolarRadiations);
        query.addBindValue(highUVIndexes);
        query.addBindValue(forecastRuleIds);
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
    if (columns.testFlag(SC_GustWindDirection))
        query += format.arg("gust_wind_direction");
    if (columns.testFlag(SC_Evapotranspiration))
        query += format.arg("evapotranspiration");
    if (columns.testFlag(SC_HighTemperature))
        query += format.arg("high_temperature");
    if (columns.testFlag(SC_LowTemperature))
        query += format.arg("low_temperature");
    if (columns.testFlag(SC_HighRainRate))
        query += format.arg("high_rain_rate");
    if (columns.testFlag(SC_HighSolarRadiation))
        query += format.arg("high_solar_radiation");
    if (columns.testFlag(SC_HighUVIndex))
        query += format.arg("high_uv_index");
    if (columns.testFlag(SC_ForecastRuleId))
        query += format.arg("forecast_rule_id");
    if (columns.testFlag(SC_Reception))
        query += format.arg("reception");

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

    // It doesn't make sense to sum certain fields (like temperature).
    // So when AF_Sum or AF_RunningTotal is specified we'll apply that only
    // to the columns were it makes sense and select an average for all the
    // others.
    if (function == AF_Sum || function == AF_RunningTotal) {
        // Figure out which columns we can sum
        SampleColumns summables = columns & SUMMABLE_COLUMNS;

        // And which columns we can't
        SampleColumns nonSummables = columns & ~SUMMABLE_COLUMNS;
        nonSummables = nonSummables & ~SC_Timestamp; // we don't want timestamp either

        // Sum the summables
        if (summables != SC_NoColumns) {
            query += buildColumnList(summables, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"));
        }

        // And just average the nonsummables(we have to apply some sort of
        // aggregate or the grouping will fail)
        if (nonSummables != SC_NoColumns) {
            query += buildColumnList(nonSummables, ", avg(iq.%1) as %1 ");
        }
    } else {
        query += buildColumnList(columns & ~SC_Timestamp, QString(", %1(iq.%2) as %2 ").arg(fn).arg("%1"));
    }

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
        double previousEvapotranspiration = 0.0;

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

            if (columns.testFlag(SC_GustWindDirection))
                // Wind direction can be null.
                if (!record.value("gust_wind_direction").isNull())
                    samples.gustWindDirection[timeStamp] =
                            record.value("gust_wind_direction").toUInt();

            if (columns.testFlag(SC_SolarRadiation))
                samples.solarRadiation.append(record.value("solar_radiation").toDouble());

            if (columns.testFlag(SC_UV_Index))
                samples.uvIndex.append(record.value("uv_index").toDouble());

            if (columns.testFlag(SC_Reception))
                samples.reception.append(record.value("reception").toDouble());

            if (columns.testFlag(SC_HighTemperature))
                samples.highTemperature.append(record.value("high_temperature").toDouble());

            if (columns.testFlag(SC_LowTemperature))
                samples.lowTemperature.append(record.value("low_temperature").toDouble());

            if (columns.testFlag(SC_HighRainRate))
                samples.highRainRate.append(record.value("high_rain_rate").toDouble());

            if (columns.testFlag(SC_Evapotranspiration)) {
                double value = record.value("evapotranspiration").toDouble();

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

            if (columns.testFlag(SC_HighSolarRadiation))
                samples.highSolarRadiation.append(record.value("high_solar_radiation").toDouble());

            if (columns.testFlag(SC_HighUVIndex))
                samples.highUVIndex.append(record.value("high_uv_index").toDouble());

            if (columns.testFlag(SC_ForecastRuleId))
                samples.forecastRuleId.append(record.value("forecast_rule_id").toInt());

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

void WebCacheDB::clearSamples() {
    QSqlQuery query(sampleCacheDb);
    query.exec("delete from sample");
    query.exec("delete from data_file");;
    sampleCacheDb.close();
    sampleCacheDb.open();
    query.exec("vacuum");
}

void WebCacheDB::clearImages() {
    QSqlQuery query(sampleCacheDb);
    query.exec("delete from image");
    query.exec("delete from image_set");
    sampleCacheDb.close();
    sampleCacheDb.open();
    query.exec("vacuum");
}
