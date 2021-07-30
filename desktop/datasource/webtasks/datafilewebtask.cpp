#include "datasource/webtasks/datafilewebtask.h"
#include "datasource/webtasks/request_data.h"
#include "datasource/webcachedb.h"

#include <QtDebug>
#include <QTimer>

#if (QT_VERSION < QT_VERSION_CHECK(5,2,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

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
}

void DataFileWebTask::beginTask() {
    data_file_t cache_info =
            WebCacheDB::getInstance().getDataFileCacheInformation(_url);

    if (cache_info.isValid && cache_info.isComplete && !_forceDownload) {
        qDebug() << "Data file is marked COMPLETE in cache database - no server check required" << _url;
        emit finished();
        return;
    }

    if (_forceDownload) {
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

bool DataFileWebTask::UrlNeedsDownlodaing(QNetworkReply *reply) {
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
    if (DataFileWebTask::UrlNeedsDownlodaing(reply)) {
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
    cache_stats_t cacheStats = WebCacheDB::getInstance().getCacheStats(_url);
    if (cacheStats.isValid) {
        qDebug() << "File exists in cache db and contains"
                 << cacheStats.count << "records between"
                 << cacheStats.start << "and" << cacheStats.end;
    }

    data_file_t dataFile = loadDataFile(fileData, lastModified, size,
                                        cacheStats);

    emit subtaskChanged(QString(tr("Caching data for %1")).arg(_name));
    WebCacheDB::getInstance().cacheDataFile(dataFile, _stationDataUrl);
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

data_file_t DataFileWebTask::loadDataFile(QStringList fileData,
                                        QDateTime lastModified, int fileSize,
                                        cache_stats_t cacheStats) {
    emit subtaskChanged(QString(tr("Processing data for %1")).arg(_name));

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


    while (!fileData.isEmpty()) {
        QString line = fileData.takeFirst();
        QStringList parts = line.split(QRegExp("\\s+"));

        if (parts.count() < 11) continue; // invalid record.

        // Build timestamp
        QString tsString = parts.takeFirst();
        tsString += " " + parts.takeFirst();
        QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODate);

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

        qint64 previousSecs = previousTime.toSecsSinceEpoch();
        qint64 thisSecs = timestamp.toSecsSinceEpoch();
        if (thisSecs - previousSecs > archiveInterval) {
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

        if (fileData.isEmpty()) {
            // Reached the end of the file. Current row is the last row.
            // Check the final timestamp in the file is within archiveInterval
            // seconds of the end of the month.
            qint64 endSecs = endTime.toSecsSinceEpoch();
            if (endSecs - thisSecs > archiveInterval) {
                qDebug() << "GAP (@end): The end is" << endTime << "last row was" << timestamp << ". Gap duration is" << endSecs - thisSecs << "seconds.";
                gapDetected = true;
                endContiguousFrom = QDateTime(); // End is not contiguous.
                if (startContiguousTo.isValid() && timestamp < startContiguousTo) {
                    startContiguousTo = timestamp;
                    qDebug() << "Start contiguous to:" << startContiguousTo;
                }
            }
        }

        previousTime = timestamp;

        // ------------------------------------------------

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
        // *this* data file is 100% complete. There should never be new rows
        // to appear in it so the only reason we'd ever re-download it is if
        // for some reason some values changed (data fixed some erroneous rain
        // tips?). So we'll replace whatever is in the cache database with this.
        expireCache = true;
    }

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
    SampleColumns cols;
    cols.standard = ALL_SAMPLE_COLUMNS;
    cols.extra = ALL_EXTRA_COLUMNS;
    ReserveSampleSetSpace(samples, size, cols);

    while (!timeStamps.isEmpty()) {

        uint timestamp = timeStamps.takeFirst().toTime_t();
        samples.timestamp.append(timestamp);
        samples.timestampUnix.append(timestamp);

        QStringList values = sampleParts.takeFirst();

        samples.temperature.append(nullableDouble(values.takeFirst()));
        samples.dewPoint.append(nullableDouble(values.takeFirst()));
        samples.apparentTemperature.append(nullableDouble(values.takeFirst()));
        samples.windChill.append(nullableDouble(values.takeFirst()));
        samples.humidity.append(nullableDouble(values.takeFirst()));
        samples.pressure.append(nullableDouble(values.takeFirst()));
        samples.indoorTemperature.append(nullableDouble(values.takeFirst()));
        samples.indoorHumidity.append(nullableDouble(values.takeFirst()));
        samples.rainfall.append(nullableDouble(values.takeFirst()));
        samples.averageWindSpeed.append(nullableDouble(values.takeFirst()));
        samples.gustWindSpeed.append(nullableDouble(values.takeFirst()));

        QVariant val = values.takeFirst();
        if (val != "None")
            samples.windDirection[timestamp] = val.toDouble();

        if (_requestData.isSolarAvailable) {
            samples.uvIndex.append(nullableDouble(values.takeFirst()));
            samples.solarRadiation.append(nullableDouble(values.takeFirst()));
        } else {
            // Throw away solar values
            values.takeFirst();
            values.takeFirst();
        }

        samples.reception.append(nullableDouble(values.takeFirst()));
        samples.highTemperature.append(nullableDouble(values.takeFirst()));
        samples.lowTemperature.append(nullableDouble(values.takeFirst()));
        samples.highRainRate.append(nullableDouble(values.takeFirst()));

        QVariant gustDirection = values.takeFirst();
        if (gustDirection != "None") {
            samples.gustWindDirection[timestamp] = gustDirection.toDouble();
        }

        samples.evapotranspiration.append(nullableDouble(values.takeFirst()));
        if (_requestData.isSolarAvailable) {
            samples.highSolarRadiation.append(nullableDouble(values.takeFirst()));
            samples.highUVIndex.append(nullableDouble(values.takeFirst()));
        } else {
            values.takeFirst();
            values.takeFirst();
        }
        samples.forecastRuleId.append(values.takeFirst().toInt());

        if (values.length() >= 17) {
            // Extra sensors are present.
            samples.soilMoisture1.append(nullableDouble(values.takeFirst()));
            samples.soilMoisture2.append(nullableDouble(values.takeFirst()));
            samples.soilMoisture3.append(nullableDouble(values.takeFirst()));
            samples.soilMoisture4.append(nullableDouble(values.takeFirst()));

            samples.soilTemperature1.append(nullableDouble(values.takeFirst()));
            samples.soilTemperature2.append(nullableDouble(values.takeFirst()));
            samples.soilTemperature3.append(nullableDouble(values.takeFirst()));
            samples.soilTemperature4.append(nullableDouble(values.takeFirst()));

            samples.leafWetness1.append(nullableDouble(values.takeFirst()));
            samples.leafWetness2.append(nullableDouble(values.takeFirst()));

            samples.leafTemperature1.append(nullableDouble(values.takeFirst()));
            samples.leafTemperature2.append(nullableDouble(values.takeFirst()));

            samples.extraHumidity1.append(nullableDouble(values.takeFirst()));
            samples.extraHumidity2.append(nullableDouble(values.takeFirst()));

            samples.extraTemperature1.append(nullableDouble(values.takeFirst()));
            samples.extraTemperature2.append(nullableDouble(values.takeFirst()));
            samples.extraTemperature3.append(nullableDouble(values.takeFirst()));
        }
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
