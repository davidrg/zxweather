#include "datasource/webtasks/datafilewebtask.h"
#include "datasource/webtasks/request_data.h"
#include "datasource/webcachedb.h"
#include "compat.h"

#include <QtDebug>
#include <QTimer>

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QRegularExpression>
#endif


#if (QT_VERSION < QT_VERSION_CHECK(5,2,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

QMap<QString, enum DataFileWebTask::DataFileColumn> DataFileWebTask::labelColumns;

// When processing the data file columns for optional sensors may in the
// future be omitted from the data file. This macro makes it easy to check
// for the columns presence before fetching its value.
#define APPEND_OPT_NULLABLE_DOUBLE(column, destination) if (columnPositions.contains(column)) destination.append(nullableDouble(values.at(columnPositions[column])));

DataFileWebTask::DataFileWebTask(QString baseUrl, QString stationCode,
                                 request_data_t requestData, QString name,
                                 QString url, bool forceDownload,
                                 int sampleInterval, WebDataSource *ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {
    _requestData = requestData;
    _name = name;
    _url = url;
    _downloadingDataset = false; // We check the cache first.
    _forceDownload = forceDownload;
    _sampleInterval = sampleInterval;

    // TODO: Once Qt 5.2 and C++11 are the minimum use an initialiser list instead.
    // ref: https://stackoverflow.com/questions/8157625/how-do-i-populate-values-of-a-static-qmap-in-c-qt
    if (labelColumns.isEmpty()) {
        labelColumns["timestamp"] = DFC_TimeStamp;
        labelColumns["temperature"] = DFC_Temperature;
        labelColumns["dew point"] = DFC_DewPoint;
        labelColumns["apparent temperature"] = DFC_ApparentTemperature;
        labelColumns["wind chill"] = DFC_WindChill;
        labelColumns["relative humidity"] = DFC_RelHumidity;
        labelColumns["absolute pressure"] = DFC_AbsolutePressure;
        labelColumns["mean sea level pressure"] = DFC_MSLPressure;
        labelColumns["indoor temperature"] = DFC_IndoorTemperature;
        labelColumns["indoor relative humidity"] = DFC_IndoorRelHumidity;
        labelColumns["rainfall"] = DFC_Rainfall;
        labelColumns["average wind speed"] = DFC_AvgWindSpeed;
        labelColumns["gust wind speed"] = DFC_GustWindSpeed;
        labelColumns["wind direction"] = DFC_WindDirection;
        labelColumns["uv index"] = DFC_UVIndex;
        labelColumns["solar radiation"] = DFC_SolarRadiation;
        labelColumns["reception"] = DFC_Reception;
        labelColumns["high temp"] = DFC_HighTemp;
        labelColumns["low temp"] = DFC_LowTemp;
        labelColumns["high rain rate"] = DFC_HighRainRate;
        labelColumns["gust direction"] = DFC_GustDirection;
        labelColumns["evapotranspiration"] = DFC_Evapotranspiration;
        labelColumns["high solar radiation"] = DFC_HighSolarRadiation;
        labelColumns["high uv index"] = DFC_HighUVIndex;
        labelColumns["forecast rule id"] = DFC_ForecastRuleID;
        labelColumns["soil moisture 1"] = DFC_SoilMoisture1;
        labelColumns["soil moisture 2"] = DFC_SoilMoisture2;
        labelColumns["soil moisture 3"] = DFC_SoilMoisture3;
        labelColumns["soil moisture 4"] = DFC_SoilMoisture4;
        labelColumns["soil temperature 1"] = DFC_SoilTemperature1;
        labelColumns["soil temperature 2"] = DFC_SoilTemperature2;
        labelColumns["soil temperature 3"] = DFC_SoilTemperature3;
        labelColumns["soil temperature 4"] = DFC_SoilTemperature4;
        labelColumns["leaf wetness 1"] = DFC_LeafWetness1;
        labelColumns["leaf wetness 2"] = DFC_LeafWetness2;
        labelColumns["leaf temperature 1"] = DFC_LeafTemperature1;
        labelColumns["leaf temperature 2"] = DFC_LeafTemperature2;
        labelColumns["extra humidity 1"] = DFC_ExtraHumidity1;
        labelColumns["extra humidity 2"] = DFC_ExtraHumidity2;
        labelColumns["extra temperature 1"] = DFC_ExtraTemperature1;
        labelColumns["extra temperature 2"] = DFC_ExtraTemperature2;
        labelColumns["extra temperature 3"] = DFC_ExtraTemperature3;

        // samples.dat uses slightly different column labels than samples_v2.dat (api >= 20220210)
        labelColumns["high_temp"] = DFC_HighTemp;
        labelColumns["low_temp"] = DFC_LowTemp;
        labelColumns["high_rain_rate"] = DFC_HighRainRate;
        labelColumns["gust_direction"] = DFC_GustDirection;
        labelColumns["high_solar_radiation"] = DFC_HighSolarRadiation;
        labelColumns["high_uv_index"] = DFC_HighUVIndex;
        labelColumns["forecast_rule_id"] = DFC_ForecastRuleID;
        labelColumns["soil_moisture_1"] = DFC_SoilMoisture1;
        labelColumns["soil_moisture_2"] = DFC_SoilMoisture2;
        labelColumns["soil_moisture_3"] = DFC_SoilMoisture3;
        labelColumns["soil_moisture_4"] = DFC_SoilMoisture4;
        labelColumns["soil_temperature_1"] = DFC_SoilTemperature1;
        labelColumns["soil_temperature_2"] = DFC_SoilTemperature2;
        labelColumns["soil_temperature_3"] = DFC_SoilTemperature3;
        labelColumns["soil_temperature_4"] = DFC_SoilTemperature4;
        labelColumns["leaf_wetness_1"] = DFC_LeafWetness1;
        labelColumns["leaf_wetness_2"] = DFC_LeafWetness2;
        labelColumns["leaf_temperature_1"] = DFC_LeafTemperature1;
        labelColumns["leaf_temperature_2"] = DFC_LeafTemperature2;
        labelColumns["extra_humidity_1"] = DFC_ExtraHumidity1;
        labelColumns["extra_humidity_2"] = DFC_ExtraHumidity2;
        labelColumns["extra_temperature_1"] = DFC_ExtraTemperature1;
        labelColumns["extra_temperature_2"] = DFC_ExtraTemperature2;
        labelColumns["extra_temperature_3"] = DFC_ExtraTemperature3;

    }
}

void DataFileWebTask::beginTask() {
    data_file_t cache_info =
            WebCacheDB::getInstance().getDataFileCacheInformation(_url);

    if (cache_info.isValid && cache_info.isComplete && !_forceDownload) {
        qDebug() << "Data file is marked COMPLETE in cache database - no server check required" << _url;
        emit finished();
        return;
    }

    if (_forceDownload || (!cache_info.isComplete && cache_info.isValid)) {

        if (!cache_info.isComplete) {
            qDebug() << "Skipping HEAD request - cached data file is incomplete.";
        }

        getDataset();
    } else {
        QNetworkRequest request(_url);
        emit httpHead(request);
    }
}

void DataFileWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        if (!_downloadingDataset) {
            cacheStatusRequestFinished(reply);
        } else {
            downloadRequestFinished(reply);
        }
    }
}

bool DataFileWebTask::UrlNeedsDownloading(QNetworkReply *reply) {
    QString url = reply->request().url().toString();
    data_file_t cache_info =
            WebCacheDB::getInstance().getDataFileCacheInformation(url);

    qDebug() << "Cache status request for url [" << url << "] finished.";

    if (!cache_info.isComplete) {
        qDebug() << "Cache is marked as incomplete. Possibly the server has more data";
        return true;
    }

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

        // Fire of a GET to GET the full dataset. Which we'll then process and
        // cache.
        return true;
    } else {
        // else the data file we have cached sounds the same as what is on the
        // server. We won't bother redownloading it.

        // We won't be downloading, processing or caching anything for this
        // file so we can skip forward a bit.
        qDebug() << "Cache copy seems ok. Skiping download.";
        return false;
    }
}


void DataFileWebTask::cacheStatusRequestFinished(QNetworkReply *reply) {
    if (DataFileWebTask::UrlNeedsDownloading(reply)) {
        getDataset();
    } else {
        // else the data file we have cached sounds the same as what is on the
        // server. We won't bother redownloading it.

        // We won't be downloading, processing or caching anything for this
        // file so we can skip forward a bit.
        qDebug() << "Cache copy seems ok. Skiping download.";
        emit finished();
    }
}

void DataFileWebTask::getDataset() {
    _downloadingDataset = true;
    emit subtaskChanged(QString(tr("Downloading data for %1")).arg(_name));

    QNetworkRequest request(_url);
    emit httpGet(request);
}

void DataFileWebTask::downloadRequestFinished(QNetworkReply *reply) {
    QStringList fileData;

    qDebug() << "Download completed for" << _name << "[" << _url << "]";

    QMap<enum DataFileWebTask::DataFileColumn, int> columnPositions;

    while (!reply->atEnd()) {
        QString line = reply->readLine().trimmed();
        if (!line.startsWith("#")) {
            fileData.append(line);
        } else if (columnPositions.isEmpty()) {
            // Build the column list. We skip over the first character
            // as its just a '#' and we don't care about that. We then
            // trim the string in case there is a space between the #
            // and the first column label.
            QStringList columnList = line.mid(1).trimmed().split('\t');
            for (int i = 0; i < columnList.length(); i++) {
                QString column = columnList[i];

                if (!labelColumns.contains(column)) {
                    qDebug() << "Unrecognised column" << column << " - ignoring";
                    continue;
                }

                enum DataFileColumn dfc = labelColumns[column];
                columnPositions[dfc] = i;
            }
        }
    }

    qDebug() << "File contains" << fileData.count() << "records.";

    QDateTime lastModified = reply->header(
                QNetworkRequest::LastModifiedHeader).toDateTime();
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();


    qDebug() << "Checking db cache status...";
    cache_stats_t cacheStats = WebCacheDB::getInstance().getCacheStats(_url);
    if (cacheStats.isValid) {
        qDebug() << "File exists in cache db and contains"
                 << cacheStats.count << "records between"
                 << cacheStats.start << "and" << cacheStats.end;
    }

    data_file_t dataFile = loadDataFile(fileData, lastModified, size,
                                        cacheStats, columnPositions);

    emit subtaskChanged(QString(tr("Caching data for %1")).arg(_name));

    if (dataFile.samples.timestamp.isEmpty()) {
        qDebug() << "Skip caching datafile - no rows to cache.";
    } else {
        WebCacheDB::getInstance().cacheDataFile(dataFile, _stationDataUrl);

    }
    emit finished();
}

double nullableDouble(QString v) {
    if (v == "None" || v == "?") {
        return qQNaN();
    }
    bool ok;
    double result = v.toDouble(&ok);
    if (ok) {
        return result;
    }
    return qQNaN();
}

data_file_t DataFileWebTask::loadDataFile(
        QStringList fileData, QDateTime lastModified, int fileSize,
        cache_stats_t cacheStats,
        QMap<enum DataFileWebTask::DataFileColumn, int> columnPositions) {
    emit subtaskChanged(QString(tr("Processing data for %1")).arg(_name));

    bool stationArchived = WebCacheDB::getInstance().stationIsArchived(_stationDataUrl);

    SampleSet samples;

    // Split & trim the data
    QList<QDateTime> timeStamps;
    QList<QStringList> sampleParts;

    QList<QDateTime> ignoreTimeStamps;
    QList<QStringList> ignoreSampleParts;

    /* How this should work:
     *   We're downloading data for an entire month. That means the data file
     *   should start within $archiveInterval minutes of 00:00 on the 1st.
     *   From there there should be a new sample every $archiveInterval minutes
     *   until we're within $archiveInterval minutes of the end of the month.
     */

    QDateTime startTime = QDateTime();
    QDateTime previousTime = QDateTime();
    QDateTime endTime = QDateTime();

    // We'll let the largest gap be slightly larger than the sample interval
    // to account for things like clocks being adjusted, etc.
    qDebug() << "Station sample interval is" << _sampleInterval;
    int archiveInterval = _sampleInterval + 0.5*_sampleInterval;
    qDebug() << "Using" << archiveInterval << "as gap threshold.";

    bool gapDetected = false;
    QDateTime startContiguousTo, endContiguousFrom;

    int lineNumber = 0;
    while (!fileData.isEmpty()) {
        lineNumber++;
        QString line = fileData.takeFirst();
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
        QStringList parts = line.split('\t');
#else
        QStringList parts = line.split('\t');
#endif

        if (parts.count() < columnPositions.count()) {
            qDebug() << "Data file" << _url
                     << "record" << lineNumber
                     << "is invalid - found " << parts.count()
                     << "columns when the colum list specifies"
                     << columnPositions.count()
                     << ". line will be ignored.";
            continue; // invalid record.
        }

        // Build timestamp
        QString tsString = parts.at(columnPositions[DFC_TimeStamp]);
        QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODate);

        if (startTime.isNull()) {
            startTime = QDateTime(
                        QDate(timestamp.date().year(),
                              timestamp.date().month(),
                              1),
                        QTime(0,0,0));;
            endTime = startTime.addMonths(1).addSecs(-1);

            if (stationArchived) {
                qDebug() << "Station is marked as archived - not detecting gaps. "
                            "Received data file assumed to be complete and will be "
                            "cached as-is permanently.";
                startContiguousTo = endTime;
                endContiguousFrom = startTime;
                gapDetected = false;
            }
        }

        // If a station is archived that means all data that will ever be available for it
        // *is* available right now and the stations entire data-set is now read-only. This
        // means that any gaps in the data set are permanent and will always be there if we
        // were to download the file again at some point in the future. Because of this we
        // can cache any files from archived stations permanently which makes searching them
        // for gaps unnecessary.
        if (!stationArchived) {
            // -----------/ The Gap Detection Zone /-----------
            // Here in The Gap Detection Zone our job is to figure out if the data
            // file contains absolutely every sample it could contain. This means
            // checking the gap between any two samples is no greater than the
            // stations sample interval.

            if (!previousTime.isValid()) {
                // First sample in the file. Initialise previousTime to be the very
                // start of the month and set the end time to be the very end of
                // the month
                startTime = QDateTime(
                            QDate(
                                timestamp.date().year(),
                                timestamp.date().month(),
                                1),
                            QTime(0,0,0));;
                endTime = startTime.addMonths(1).addSecs(-1);
                previousTime = startTime;
                startContiguousTo = endTime;
                endContiguousFrom = startTime;
                qDebug() << "Data file max range:" << startTime << "to" << endTime;

                if (timestamp != startTime) {
                    startContiguousTo = QDateTime(); // Start not contiguous
                }
            }

            qint64 previousSecs = TO_UNIX_TIME(previousTime);
            qint64 thisSecs = TO_UNIX_TIME(timestamp);

            if (thisSecs - previousSecs > archiveInterval) {
                // Detected gap is (previousTime, timestamp). If we've got a record of this
                // gap being marked as permanent we can ignore it.
                if (WebCacheDB::getInstance().sampleGapIsKnown(_stationDataUrl, previousTime, timestamp)) {
                    qDebug() << "Detected gap from " << previousTime << "to" << timestamp << "is known to be permanent. Ignoring.";
                } else {
                    qDebug() << "GAP: This timestamp is" << timestamp << "previous was" << previousTime << ". Gap duration is" << thisSecs - previousSecs << "seconds.";
                    gapDetected = true;
                    if (startContiguousTo.isValid() && previousTime < startContiguousTo) {
                        startContiguousTo = previousTime;
                        qDebug() << "Start contiguous to:" << startContiguousTo;
                    }
                    if (timestamp > endContiguousFrom) {
                        endContiguousFrom = timestamp;
                        qDebug() << "End contiguous to:" << endContiguousFrom;
                    }
                }
            }

            if (fileData.isEmpty()) {
                // Reached the end of the file. Current row is the last row.
                // Check the final timestamp in the file is within archiveInterval
                // seconds of the end of the month.
                qint64 endSecs = TO_UNIX_TIME(endTime);
                if (endSecs - thisSecs > archiveInterval) {
                    // Detected gap is (timestamp, endTime). Check with the DB to see if this gap is
                    // known to be permanent. If so we can safely ignore it and cache the gap.
                    if (WebCacheDB::getInstance().sampleGapIsKnown(_stationDataUrl, timestamp, endTime)) {
                        qDebug() << "Gap at end of file from" << timestamp << "to" << endTime << "is known to be permanent. Ignoring.";
                    } else {
                        qDebug() << "GAP (@end): The end is" << endTime << "last row was" << timestamp << ". Gap duration is" << endSecs - thisSecs << "seconds.";
                        gapDetected = true;
                        endContiguousFrom = QDateTime(); // End is not contiguous.
                        if (startContiguousTo.isValid() && timestamp < startContiguousTo) {
                            startContiguousTo = timestamp;
                            qDebug() << "Start contiguous to:" << startContiguousTo;
                        }
                    }
                }
            }

            previousTime = timestamp;

            // ------------------------------------------------
        }

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

    if (gapDetected) {
        qDebug() << "----> Data file is INCOMPLETE: it contains one or more gaps!";
        if (startContiguousTo.isValid()) {
            qDebug() << "Start of the file is contiguous to:" << startContiguousTo;
        } else {
            qDebug() << "Gap exists at start of file.";
        }
        if (endContiguousFrom.isValid()) {
            qDebug() << "End of the file is contiguous from:" << endContiguousFrom;
        } else {
            qDebug() << "Gap exists at end of file.";
        }

    } else {
        qDebug() << "----> Data file is COMPLETE: no gaps detected!";
        qDebug() << "Expiring local cache and replacing with received data.";
        // *this* data file is 100% complete. There should never be new rows
        // to appear in it so the only reason we'd ever re-download it is if
        // for some reason some values changed (data fixed some erroneous rain
        // tips?). So we'll replace whatever is in the cache database with this.

        // Add the samples the cache already has back into the set that will be
        // inserted as we're replacing whats currently cached.
        sampleParts.append(ignoreSampleParts);
        timeStamps.append(ignoreTimeStamps);
        expireCache = true;
    }

    if ((ignoreTimeStamps.count() == cacheStats.count)) {
        // There is the same number of records between those timestamps in
        // both the cache database and the data file. Probably safe to assume
        // none of them were changed so we'll just ignore them.
        qDebug() << "Sample count in cache matches sample count for matching timespan in data file.";
        ignoreTimeStamps.clear();
        ignoreSampleParts.clear();
    } else {
        // One or more samples were added or removed between the date ranges
        // available in the cache database. We'll take the downloaded data
        // file as authorative and dump what we currently have in the database
        // for this file.
        qDebug() << "Sample count in cache timespan differs between DB and data file - expiring cache.";
        sampleParts.append(ignoreSampleParts);
        timeStamps.append(ignoreTimeStamps);
        expireCache = true;
    }

    // Allocate memory for the sample set
    int size = timeStamps.count();
    SampleColumns cols;
    cols.standard = ALL_SAMPLE_COLUMNS;
    cols.extra = ALL_EXTRA_COLUMNS;
    ReserveSampleSetSpace(samples, size, cols);


    while (!timeStamps.isEmpty()) {

        auto timestamp = TO_UNIX_TIME(timeStamps.takeFirst());
        samples.timestamp.append(timestamp);
        samples.timestampUnix.append(timestamp);

        QStringList values = sampleParts.takeFirst();

        samples.temperature.append(nullableDouble(values.at(columnPositions[DFC_Temperature])));
        samples.dewPoint.append(nullableDouble(values.at(columnPositions[DFC_DewPoint])));
        samples.apparentTemperature.append(nullableDouble(values.at(columnPositions[DFC_ApparentTemperature])));
        samples.windChill.append(nullableDouble(values.at(columnPositions[DFC_WindChill])));
        samples.humidity.append(nullableDouble(values.at(columnPositions[DFC_RelHumidity])));
        samples.absolutePressure.append(nullableDouble(values.at(columnPositions[DFC_AbsolutePressure])));

        APPEND_OPT_NULLABLE_DOUBLE(DFC_MSLPressure, samples.meanSeaLevelPressure);

        samples.indoorTemperature.append(nullableDouble(values.at(columnPositions[DFC_IndoorTemperature])));
        samples.indoorHumidity.append(nullableDouble(values.at(columnPositions[DFC_IndoorRelHumidity])));
        samples.rainfall.append(nullableDouble(values.at(columnPositions[DFC_Rainfall])));
        samples.averageWindSpeed.append(nullableDouble(values.at(columnPositions[DFC_AvgWindSpeed])));
        samples.gustWindSpeed.append(nullableDouble(values.at(columnPositions[DFC_GustWindSpeed])));

        QVariant val = values.at(columnPositions[DFC_WindDirection]);
        if (val != "None")
            samples.windDirection[timestamp] = val.toDouble();

        if (_requestData.isSolarAvailable) {
            APPEND_OPT_NULLABLE_DOUBLE(DFC_UVIndex, samples.uvIndex);
            APPEND_OPT_NULLABLE_DOUBLE(DFC_SolarRadiation, samples.solarRadiation);
        }

        // Optional - potentially missing on cabled stations.
        APPEND_OPT_NULLABLE_DOUBLE(DFC_Reception, samples.reception);

        // These are specific to DAVIS stations so may not always be
        // present
        APPEND_OPT_NULLABLE_DOUBLE(DFC_HighTemp, samples.highTemperature);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_LowTemp, samples.lowTemperature);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_HighRainRate, samples.highRainRate);

        if (columnPositions.contains(DFC_GustDirection)) {
            QVariant gustDirection = values.at(columnPositions[DFC_GustDirection]);
            if (gustDirection != "None") {
                samples.gustWindDirection[timestamp] = gustDirection.toDouble();
            }
        }

        APPEND_OPT_NULLABLE_DOUBLE(DFC_Evapotranspiration, samples.evapotranspiration);
        if (_requestData.isSolarAvailable) {
            APPEND_OPT_NULLABLE_DOUBLE(DFC_HighSolarRadiation, samples.highSolarRadiation);
            APPEND_OPT_NULLABLE_DOUBLE(DFC_HighUVIndex, samples.highUVIndex);
        }

        if (columnPositions.contains(DFC_ForecastRuleID)) {
            samples.forecastRuleId.append(values.at(columnPositions[DFC_ForecastRuleID]).toInt());
        }

        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilMoisture1, samples.soilMoisture1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilMoisture2, samples.soilMoisture2);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilMoisture3, samples.soilMoisture3);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilMoisture4, samples.soilMoisture4);

        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilTemperature1, samples.soilTemperature1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilTemperature2, samples.soilTemperature2);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilTemperature3, samples.soilTemperature3);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_SoilTemperature4, samples.soilTemperature4);

        APPEND_OPT_NULLABLE_DOUBLE(DFC_LeafWetness1, samples.leafWetness1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_LeafWetness2, samples.leafWetness2);

        APPEND_OPT_NULLABLE_DOUBLE(DFC_LeafTemperature1, samples.leafTemperature1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_LeafTemperature2, samples.leafTemperature2);

        APPEND_OPT_NULLABLE_DOUBLE(DFC_ExtraHumidity1, samples.extraHumidity1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_ExtraHumidity2, samples.extraHumidity2);

        APPEND_OPT_NULLABLE_DOUBLE(DFC_ExtraTemperature1, samples.extraTemperature1);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_ExtraTemperature2, samples.extraTemperature2);
        APPEND_OPT_NULLABLE_DOUBLE(DFC_ExtraTemperature3, samples.extraTemperature3);
    }

    data_file_t dataFile;
    dataFile.filename = _url;
    dataFile.isValid = true;
    dataFile.last_modified = lastModified;
    dataFile.size = fileSize;
    dataFile.samples = samples;
    dataFile.expireExisting = expireCache;
    dataFile.hasSolarData = _requestData.isSolarAvailable;
    dataFile.isComplete = !gapDetected;
    dataFile.start_contiguous_to = startContiguousTo;
    dataFile.end_contiguous_from = endContiguousFrom;
    dataFile.start_time = startTime;
    dataFile.end_time = endTime;

    return dataFile;
}
