#include "datasource/webtasks/datafilewebtask.h"
#include "datasource/webtasks/request_data.h"
#include "datasource/webcachedb.h"

#include <QtDebug>
#include <QTimer>

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

DataFileWebTask::DataFileWebTask(QString baseUrl, QString stationCode,
                                 request_data_t requestData, QString name,
                                 QString url, bool forceDownload, WebDataSource *ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {
    _requestData = requestData;
    _name = name;
    _url = url;
    _downloadingDataset = false; // We check the cache first.
    _forceDownload = forceDownload;
}

void DataFileWebTask::beginTask() {
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
