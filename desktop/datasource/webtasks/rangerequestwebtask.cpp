
#include "rangerequestwebtask.h"
#include "datafilewebtask.h"
#include "selectsampleswebtask.h"
#include "cachingfinishedwebtask.h"
#include "datasource/webcachedb.h"
#include "compat.h"
#include "json/json.h"

#include <QtDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>

#define DATASET_RANGE "samplerange.json"


// Undefine this to use the tab delimited data files under /data
// (eg /data/sb/2016/2/samples.txt) instead of the files weather_plot
// generates for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat).
// This allows the use of the desktop client remotely without weather_plot
// running. Its incompatible with versions of zxweather < 1.0

// Uncomment this to use the tab delimited data files generated by
// weather_plot for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat)
// instead of the data files generated by zxw_web under /data
// (eg /data/sb/2016/2/samples.txt)
//#define USE_GNUPLOT_DATA
// Currently we use GNUPLOT data because the Web UI takes too long to prepare
// the required cache control headers. Some time this needs to be turned into a
// UI option or the Web UI needs to come up with the headers without getting the
// database involved.

RangeRequestWebTask::RangeRequestWebTask(QString baseUrl,
                                         QString stationCode,
                                         request_data_t requestData,
                                         bool select,
                                         WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {
    _requestData = requestData;
    _select = select;
    _requestingRange = true;
    _awaitingUrls = 0;
}

void RangeRequestWebTask::beginTask() {

    // For gap detection
    _sampleInterval = WebCacheDB::getInstance().getSampleInterval(_stationDataUrl);

    // Before we go bothering the server asking it what timespan its got, lets
    // see what timespan *we've* got.
    sample_range_t cacheRange = WebCacheDB::getInstance().getSampleRange(_stationDataUrl);

    qDebug() << "Request range:" << _requestData.startTime << _requestData.endTime;
    qDebug() << "  Cache range:" << cacheRange.start << cacheRange.end;

    if (cacheRange.isValid &&
            _requestData.startTime >= cacheRange.start &&
            _requestData.endTime <= cacheRange.end) {
        // Requested timespan is covered by the cache database. This means we
        // know the server is able to supply data for this entire range (its
        // supplied it in the past). All we need to do now is fill in any
        // gaps in the cache database (if any).
        _requestingRange = false;

        // We might be able to cover this entire data request without
        // bothering the server once!

        qDebug() << "Requested range is covered by the cache DB! Skipping server check.";

        bool ok = buildUrlListAndQueue();

        if (ok) {
            emit finished();
        }
        return;
    }

    // If the requested timespan fits within the cache databases range then
    // there is no reason to bother the server

    QString url = _stationBaseUrl + DATASET_RANGE;

    emit subtaskChanged(tr("Validating data range..."));

    QNetworkRequest request(url);

    emit httpGet(request);
}

void RangeRequestWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString url = reply->request().url().toString();
        if (reply->error() == QNetworkReply::ContentNotFoundError
                && _urlMonths.contains(url)) {
            qDebug() << "Requested URL" << url << "was not found on the server!";

            QDate startDate = _urlMonths[url];

            data_file_t dataFile;
            dataFile.filename = url;
            dataFile.isValid = true;
            dataFile.last_modified = QDateTime::currentDateTime();
            dataFile.size = 0;
            //dataFile.samples;
            dataFile.expireExisting = false;
            dataFile.hasSolarData = false; // This is just used for caching the data (which we don't have)
            dataFile.start_time = QDateTime(startDate, QTime(0,0));
            dataFile.end_time = dataFile.start_time.addMonths(1).addSecs(-1);

            if (WebCacheDB::getInstance().stationIsArchived(_stationDataUrl)) {
                qDebug() << "Station is archived - treating 404 as permanent gap of 1 month.";
                dataFile.isComplete = true;
                dataFile.start_contiguous_to = dataFile.end_time;
                dataFile.end_contiguous_from = dataFile.start_time;
            } else {
                dataFile.isComplete = false;
                //dataFile.start_contiguous_to = QDateTime();
                //dataFile.end_contiguous_from = endContiguousFrom;
            }

            WebCacheDB::getInstance().cacheDataFile(dataFile, _stationDataUrl);
            _awaitingUrls--;


            if (_awaitingUrls == 0) {
                completeWork();
                emit finished();
            }

        } else {
            emit failed(reply->errorString());
        }
    } else {
        QByteArray replyData = reply->readAll();

#ifdef PARALLEL_HEAD
        bool ok;
        if (_requestingRange) {
            ok = processRangeResponse(replyData);
        } else {
            ok = processHeadResponse(reply);
        }
#else
        bool ok = processRangeResponse(replyData);
#endif

        if (ok) {
            emit finished();
        }
    }
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

QMap<QString, QDateTime> RangeRequestWebTask::lastQuery;

void RangeRequestWebTask::ClearURLCache() {
    RangeRequestWebTask::lastQuery.clear();
}

void RangeRequestWebTask::getURLList(QString baseURL, QDateTime startTime, QDateTime endTime,
                QStringList& urlList, QStringList& nameList, QList<QDate>& months) {


    QDate startDate = startTime.date();
    QDate endDate = endTime.date();

    qDebug() << "Building URLlist from" << startTime << "to" << endTime;

    int startYear = startDate.year();
    int startMonth = startDate.month();

    int endYear = endDate.year();
    int endMonth = endDate.month();

    //TODO: consider trying to make use of day-level data sources if it makes
    // sense.

    QDate today = QDate::currentDate();

    for(int year = startYear; year <= endYear; year++) {
        qDebug() << "Year:" << year;

        int month = 1;
        if (year == startYear) {
            month = startMonth;
        }

        for(; month <= 12; month++) {
            if (QDate(year, month, 1) > endDate) {
                break; // we're finished
            }

            QString monthName = monthToName(month);

            qDebug() << "Build URL for" << year << monthName;

#ifdef USE_GNUPLOT_DATA
            QString url = baseURL + QString::number(year) + "/" +
                    monthName + "/gnuplot_data.dat";
#else
            QString url = baseURL + QString::number(year) + "/" +
                    QString::number(month) + "/samples.dat";
#endif

            data_file_t cache_info =
                    WebCacheDB::getInstance().getDataFileCacheInformation(url);

            if (cache_info.isValid && cache_info.isComplete) {
                qDebug() << "Data file is marked COMPLETE in cache database - no server check required" << url;
                continue;
            }

            if (RangeRequestWebTask::lastQuery.contains(url)) {
                if (TO_UNIX_TIME(QDateTime::currentDateTime()) -
                        TO_UNIX_TIME(RangeRequestWebTask::lastQuery[url]) < 86400) {
                    // URL was queried less than 24 hours ago. Skip.
                    if (year == endYear && month == endMonth)
                        break;
                    continue;
                }
            } else if (!(today.month() == month && today.year() == year)) {
                RangeRequestWebTask::lastQuery[url] = QDateTime::currentDateTime();
            }

            urlList.append(url);
            nameList.append(monthName + " " + QString::number(year));
            months.append(QDate(year, month, 1));

            // We're finished.
//            if (year == endYear && month == endMonth)
//                break;
        }
    }
}

bool RangeRequestWebTask::processRangeResponse(QString data) {
    _requestingRange = false;
    using namespace QtJson;

    bool ok;

    qDebug() << "Range request completed.";

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        emit failed(tr("JSON parsing failed for timestamp range "
                                  "request. Download aborted."));
        qWarning() << "Failed parsing JSON response from timestamp range request";
        qDebug() << "Received document:" << data;

        return false;
    }

    QDateTime minTimestamp = QDateTime::fromString(
                result["oldest"].toString(), Qt::ISODate);
    QDateTime maxTimestamp = QDateTime::fromString(
                result["latest"].toString(), Qt::ISODate);

    qDebug() << "Valid time range on remote server is"
             << minTimestamp << "to" << maxTimestamp;

    QDateTime dlStartTime = _requestData.startTime;
    QDateTime dlEndTime = _requestData.endTime;

    // If the requested range is greater than what the server can provide then
    // clip it to what the server has available.
    if (dlStartTime < minTimestamp) dlStartTime = minTimestamp;
    if (dlEndTime > maxTimestamp) dlEndTime = maxTimestamp;

    _requestData.startTime = dlStartTime;
    _requestData.endTime = dlEndTime;

    return buildUrlListAndQueue();
}

bool RangeRequestWebTask::buildUrlListAndQueue() {

    QStringList urlList;
    QStringList nameList;
    QList<QDate> monthList;
    RangeRequestWebTask::getURLList(
                _stationDataUrl,
               _requestData.startTime,
               _requestData.endTime,
               urlList /*OUT*/,
               nameList /*OUT*/,
               monthList /*OUT*/
               );

    qDebug() << "URLs:" << urlList;
    qDebug() << "Names:" << nameList;

    if (urlList.count() == 0) {
        // No URLs in need of fetching. Job done.

        completeWork();
        return true;
    }

    // Chuck the names in a hashtable for use later.
    for (int i = 0; i < urlList.count(); i++) {
        QString url = urlList[i];
        QString name = nameList[i];
        QDate month = monthList[i];
        _urlNames[url] = name;
        _urlMonths[url] = month;
    }

#ifdef PARALLEL_HEAD
    headUrls();
    return false;
#else
    queueDownloadTasks(false);
    return true;
#endif
}

void RangeRequestWebTask::queueDownloadTasks(bool forceDownload) {
    // Queue up all data files for processing
    foreach (QString url, _urlNames.keys()) {
        QString name = _urlNames[url];
        qDebug() << "URL: " << name << url;
        DataFileWebTask *task = new DataFileWebTask(_baseUrl, _stationCode,
                                                    _requestData, name, url,
                                                    forceDownload,
                                                    _sampleInterval,
                                                    _dataSource);
        emit queueTask(task);
    }

    if (_select) {
        // Put a task onto the end of the queue to grab the dataset from the cache
        // database and hand it to the datasource.
        SelectSamplesWebTask *selectTask = new SelectSamplesWebTask(_baseUrl,
                                                                    _stationCode,
                                                                    _requestData,
                                                                    _dataSource);
        emit queueTask(selectTask);
    } else {
        CachingFinishedWebTask *finishedTask = new CachingFinishedWebTask(_baseUrl,
                                                                          _stationCode,
                                                                          _dataSource);
        emit queueTask(finishedTask);
    }
}

#ifdef PARALLEL_HEAD
void RangeRequestWebTask::headUrls() {
    emit subtaskChanged(tr("Checking Cache Status..."));
    foreach (QString url, _urlNames.keys()) {
        QNetworkRequest request(url);
        _awaitingUrls++;
        emit httpHead(request);
    }
}

bool RangeRequestWebTask::processHeadResponse(QNetworkReply *reply) {
    QString url = reply->request().url().toString();
    _awaitingUrls--;

    if (DataFileWebTask::UrlNeedsDownloading(reply)) {
        QString name = _urlNames[url];
        qDebug() << "URL: " << name << url;
        DataFileWebTask *task = new DataFileWebTask(_baseUrl, _stationCode,
                                                    _requestData, name, url,
                                                    true, // don't issue a HEAD, force download
                                                    _sampleInterval,
                                                    _dataSource);
        emit queueTask(task);
    }

    bool finished = _awaitingUrls == 0;

    if (finished) {
        completeWork();
    }

    return finished;
}
#endif

void RangeRequestWebTask::completeWork() {
    if (_select) {
        // Put a task onto the end of the queue to grab the dataset from the cache
        // database and hand it to the datasource.
        SelectSamplesWebTask *selectTask = new SelectSamplesWebTask(_baseUrl,
                                                                    _stationCode,
                                                                    _requestData,
                                                                    _dataSource);
        emit queueTask(selectTask);
    } else {
        CachingFinishedWebTask *finishedTask = new CachingFinishedWebTask(_baseUrl,
                                                                          _stationCode,
                                                                          _dataSource);
        emit queueTask(finishedTask);
    }
}

#if QT_VERSION < 0x050600
void RangeRequestWebTask::requestRedirected(QString oldUrl, QString newUrl) {
    _urlNames[newUrl] = _urlNames[oldUrl];
}
#endif
