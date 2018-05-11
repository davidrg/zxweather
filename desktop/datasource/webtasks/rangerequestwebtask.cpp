
#include "rangerequestwebtask.h"
#include "datafilewebtask.h"
#include "selectsampleswebtask.h"
#include "cachingfinishedwebtask.h"

#include "json/json.h"

#include <QtDebug>
#include <QNetworkRequest>
#include <QNetworkReply>

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
}

void RangeRequestWebTask::beginTask() {
    QString url = _stationBaseUrl + DATASET_RANGE;

    QNetworkRequest request(url);

    emit httpGet(request);
}

void RangeRequestWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        QByteArray replyData = reply->readAll();

        bool ok = processResponse(replyData);

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

void getURLList(QString baseURL, QDateTime startTime, QDateTime endTime,
                QStringList& urlList, QStringList& nameList) {
    QDate startDate = startTime.date();
    QDate endDate = endTime.date();

    int startYear = startDate.year();
    int startMonth = startDate.month();

    int endYear = endDate.year();
    int endMonth = endDate.month();

    //TODO: consider trying to make use of day-level data sources if it makes
    // sense.

    for(int year = startYear; year <= endYear; year++) {
        qDebug() << "Year:" << year;
        int month = year == startYear ? startMonth : 1;
        for(; month <= 12; month++) {

            QString monthName = monthToName(month);

#ifdef USE_GNUPLOT_DATA
            QString url = baseURL + QString::number(year) + "/" +
                    monthName + "/gnuplot_data.dat";
#else
            QString url = baseURL + QString::number(year) + "/" +
                    QString::number(month) + "/samples.dat";
#endif

            urlList.append(url);
            nameList.append(monthName + " " + QString::number(year));

            // We're finished.
            if (year == endYear && month == endMonth)
                break;
        }
    }
}

bool RangeRequestWebTask::processResponse(QString data) {
    using namespace QtJson;

    bool ok;

    qDebug() << "Range request completed.";

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        emit failed("JSON parsing failed for timestamp range "
                                  "request. Download aborted.");
        qWarning() << "Failed parsing JSON response from timestamp range request";

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

    QStringList urlList;
    QStringList nameList;
    getURLList(_stationDataUrl,
               dlStartTime,
               dlEndTime,
               urlList /*OUT*/,
               nameList /*OUT*/
               );

    qDebug() << "URLs:" << urlList;
    qDebug() << "Names:" << nameList;

    // Chuck the names in a hashtable for use later.
    QHash<QString, QString> urlNames;
    for (int i = 0; i < urlList.count(); i++) {
        QString url = urlList[i];
        QString name = nameList[i];
        urlNames[url] = name;
    }

    // Queue up all data files for processing
    foreach (QString url, urlList) {
        QString name = urlNames[url];
        qDebug() << "URL: " << name << url;
        DataFileWebTask *task = new DataFileWebTask(_baseUrl, _stationCode,
                                                    _requestData, name, url,
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

    return true;
}
