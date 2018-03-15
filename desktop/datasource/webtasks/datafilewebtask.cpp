#include "datasource/webtasks/datafilewebtask.h"
#include "datasource/webtasks/request_data.h"
#include "datasource/webcachedb.h"

#include <QtDebug>
#include <QTimer>

DataFileWebTask::DataFileWebTask(QString baseUrl, QString stationCode,
                                 request_data_t requestData, QString name,
                                 QString url, WebDataSource *ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {
    _requestData = requestData;
    _name = name;
    _url = url;
    _downloadingDataset = false; // We check the cache first.
}

void DataFileWebTask::beginTask() {
    QNetworkRequest request(_url);
    emit httpHead(request);
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

void DataFileWebTask::cacheStatusRequestFinished(QNetworkReply *reply) {
    data_file_t cache_info =
            WebCacheDB::getInstance().getDataFileCacheInformation(_url);

    qDebug() << "Cache status request for url [" << _url << "] finished.";

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
    emit subtaskChanged("Downloading data for " + _name);

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

    emit subtaskChanged("Caching data for " + _name);
    WebCacheDB::getInstance().cacheDataFile(dataFile, _stationDataUrl);
    emit finished();
}


data_file_t DataFileWebTask::loadDataFile(QStringList fileData,
                                        QDateTime lastModified, int fileSize,
                                        cache_stats_t cacheStats) {
    emit subtaskChanged("Processing data for " + _name);

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
            samples.windDirection[timestamp] = val.toDouble();

        if (_requestData.isSolarAvailable) {
            samples.uvIndex.append(values.takeFirst().toDouble());
            samples.solarRadiation.append(values.takeFirst().toDouble());
        } else {
            // Throw away solar values
            values.takeFirst();
            values.takeFirst();
        }

        samples.reception.append(values.takeFirst().toDouble());
        samples.highTemperature.append(values.takeFirst().toDouble());
        samples.lowTemperature.append(values.takeFirst().toDouble());
        samples.highRainRate.append(values.takeFirst().toDouble());

        QVariant gustDirection = values.takeFirst();
        if (gustDirection != "None") {
            samples.gustWindDirection[timestamp] = gustDirection.toDouble();
        }

        samples.evapotranspiration.append(values.takeFirst().toDouble());
        if (_requestData.isSolarAvailable) {
            samples.highSolarRadiation.append(values.takeFirst().toDouble());
            samples.highUVIndex.append(values.takeFirst().toDouble());
        } else {
            values.takeFirst();
            values.takeFirst();
        }
        samples.forecastRuleId.append(values.takeFirst().toInt());
    }

    data_file_t dataFile;
    dataFile.filename = _url;
    dataFile.isValid = true;
    dataFile.last_modified = lastModified;
    dataFile.size = fileSize;
    dataFile.samples = samples;
    dataFile.expireExisting = expireCache;
    dataFile.hasSolarData = _requestData.isSolarAvailable;

    return dataFile;
}
