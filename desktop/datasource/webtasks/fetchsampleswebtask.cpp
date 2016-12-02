#include "fetchsampleswebtask.h"
#include "datafilewebtask.h"
#include "rangerequestwebtask.h"
#include "datasource/webcachedb.h"

#include "json/json.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

#define DATASET_SYSCONFIG "sysconfig.json"


// Undefine this to use the tab delimited data files under /data
// (eg /data/sb/2016/2/samples.txt) instead of the files weather_plot
// generates for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat).
// This allows the use of the desktop client remotely without weather_plot
// running. Its incompatible with versions of zxweather < 1.0

// Uncomment this to use the tab delimited data files generated by
// weather_plot for gnuplots use (eg /b/sb/2016/february/gnuplot_data.dat)
// instead of the data files generated by zxw_web under /data
// (eg /data/sb/2016/2/samples.txt)
#define USE_GNUPLOT_DATA
// Currently we use GNUPLOT data because the Web UI takes too long to prepare
// the required cache control headers. Some time this needs to be turned into a
// UI option or the Web UI needs to come up with the headers without getting the
// database involved.


FetchSamplesWebTask::FetchSamplesWebTask(QString baseUrl,
                                         QString stationCode,
                                         SampleColumns columns,
                                         QDateTime startTime,
                                         QDateTime endTime,
                                         AggregateFunction aggregateFunction,
                                         AggregateGroupType groupType,
                                         uint32_t groupMinutes,
                                         WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _columns = columns;
    _startTime = startTime;
    _endTime = endTime;
    _aggregateFunction = aggregateFunction;
    _groupType = groupType;
    _groupMinutes = groupMinutes;
}

void FetchSamplesWebTask::beginTask() {
    QString url = _dataRootUrl + DATASET_SYSCONFIG;

    QNetworkRequest request(url);

    emit httpGet(request);
}

void FetchSamplesWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        QByteArray replyData = reply->readAll();

        bool ok = processResponse(replyData);

        if (ok) {
            // Sysconfig data loaded! Queue up the next task!
            request_data_t request;
            request.columns = _columns;
            request.startTime = _startTime;
            request.endTime = _endTime;
            request.aggregateFunction = _aggregateFunction;
            request.groupType = _groupType;
            request.groupMinutes = _groupMinutes;
            request.stationName = _stationName;
            request.isSolarAvailable = _isSolarAvailable;
            request.hwType = _hwType;

            RangeRequestWebTask* task = new RangeRequestWebTask(
                        _baseUrl, _stationCode, request, _dataSource);
            emit queueTask(task);
            emit finished();
        }
    }
}

bool FetchSamplesWebTask::processResponse(QByteArray responseData) {
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(responseData, ok).toMap();

    if (!ok) {
        qDebug() << "sysconfig parse error. Data:" << responseData;
        emit failed("JSON parsing failed while loading system configuration.");
        return false;
    }

    QVariantList stations = result["stations"].toList();
    foreach (QVariant station, stations) {
        QVariantMap stationData = station.toMap();

        if (stationData["code"].toString() == _stationCode) {
            _stationName = stationData["name"].toString();
            _isSolarAvailable = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString();

            if (hw == "DAVIS") {
                _isSolarAvailable = stationData["hw_config"]
                        .toMap()["has_solar_and_uv"].toBool();
                _hwType = HW_DAVIS;
            } else if (hw == "FOWH1080") {
                _hwType = HW_FINE_OFFSET;
            } else {
                _hwType = HW_GENERIC;
            }
            return true;
        }
    }

    return false;
}

