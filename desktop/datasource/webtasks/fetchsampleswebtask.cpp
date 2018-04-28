#include "fetchsampleswebtask.h"
#include "datafilewebtask.h"
#include "rangerequestwebtask.h"
#include "datasource/webcachedb.h"

#include "json/json.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

#define DATASET_SYSCONFIG "sysconfig.json"


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

            // Firstly, filter out any columns that aren't valid:
            if (_hwType != HW_DAVIS) {
                _columns = _columns & ~DAVIS_COLUMNS;
            } else {
                // davis hardware. Turn off any columns not applicable for the
                // model of hardware in use.
                if (!_isSolarAvailable) {
                    _columns = _columns & ~SOLAR_COLUMNS;
                }
                if (!_isWireless) {
                    _columns = _columns & ~SC_Reception;
                }
            }

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

        if (stationData["code"].toString().toLower() == _stationCode) {
            _stationName = stationData["name"].toString();
            _isSolarAvailable = false;
            _isWireless = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString().toUpper();

            if (hw == "DAVIS") {
                _isSolarAvailable = stationData["hw_config"]
                        .toMap()["has_solar_and_uv"].toBool();
                _isWireless = stationData["hw_config"]
                        .toMap()["is_wireless"].toBool();
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

