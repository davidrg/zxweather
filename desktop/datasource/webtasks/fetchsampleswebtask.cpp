#include "fetchsampleswebtask.h"
#include "datafilewebtask.h"
#include "rangerequestwebtask.h"
#include "datasource/webcachedb.h"
#include "fetchstationinfo.h"

#include "json/json.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <cfloat>

#define DATASET_SYSCONFIG "sysconfig.json"


FetchSamplesWebTask::FetchSamplesWebTask(QString baseUrl,
                                         QString stationCode,
                                         SampleColumns columns,
                                         QDateTime startTime,
                                         QDateTime endTime,
                                         AggregateFunction aggregateFunction,
                                         AggregateGroupType groupType,
                                         uint32_t groupMinutes,
                                         bool select,
                                         WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _columns = columns;
    _startTime = startTime;
    _endTime = endTime;
    _aggregateFunction = aggregateFunction;
    _groupType = groupType;
    _groupMinutes = groupMinutes;
    _select = select;
}

QString FetchSamplesWebTask::lastSysConfig;

void FetchSamplesWebTask::beginTask() {
    QString url = _dataRootUrl + DATASET_SYSCONFIG;

    if (!lastSysConfig.isNull() && lastSysConfig == url) {
        // We've already loaded sysconfig from the server once this session.
        // Everything we need should be in the cache database. Just fetch it from there
        // instead of bothering the server again.

        qDebug() << "Already fetched SYSCONFIG recently. Loading from database instead.";

        station_info_t stationInfo = WebCacheDB::getInstance().getStationInfo(_stationDataUrl);
        _hwType = stationInfo.hardwareType;
        _isSolarAvailable = stationInfo.hasSolarAndUV;
        _isWireless = stationInfo.isWireless;
        _stationName = stationInfo.title;

        finishWork();
    } else {
        QNetworkRequest request(url);

        emit httpGet(request);
    }
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
            lastSysConfig = reply->url().toString();
            finishWork();
        }
    }
}

void FetchSamplesWebTask::finishWork() {

    // Firstly, filter out any columns that aren't valid:
    if (_hwType != HW_DAVIS) {
        _columns.standard = _columns.standard & ~DAVIS_COLUMNS;
        _columns.extra = _columns.extra & ~DAVIS_EXTRA_COLUMNS;
    } else {
        // davis hardware. Turn off any columns not applicable for the
        // model of hardware in use.
        if (!_isSolarAvailable) {
            _columns.standard = _columns.standard & ~SOLAR_COLUMNS;
        }
        if (!_isWireless) {
            _columns.standard = _columns.standard & ~SC_Reception;
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
                _baseUrl, _stationCode, request, _select, _dataSource);
    emit queueTask(task);
    emit finished();
}

bool FetchSamplesWebTask::processResponse(QByteArray responseData) {
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(responseData, ok).toMap();

    if (!ok) {
        qDebug() << "sysconfig parse error. Data:" << responseData;
        emit failed(tr("JSON parsing failed while loading system configuration."));
        return false;
    }

    qDebug() << "Parsing SYSCONFIG data";

    QVariantList stations = result["stations"].toList();
    foreach (QVariant station, stations) {
        QVariantMap stationData = station.toMap();

        qDebug() << "SYSCONFIG: Station:" << stationData["code"].toString();

        if (stationData["code"].toString().toLower() == _stationCode) {
            _stationName = stationData["name"].toString();
            _isSolarAvailable = false;
            _isWireless = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString().toUpper();

            int davis_broadcast_id = -1;

            if (hw == "DAVIS") {
                _isSolarAvailable = stationData["hw_config"]
                        .toMap()["has_solar_and_uv"].toBool();
                _isWireless = stationData["hw_config"]
                        .toMap()["is_wireless"].toBool();
                _hwType = HW_DAVIS;

                if (_isWireless) {
                    bool ok;
                    davis_broadcast_id = stationData["hw_config"].toMap()["broadcast_id"].toInt(&ok);
                    if (!ok) {
                        davis_broadcast_id = -1;
                    }
                }
            } else if (hw == "FOWH1080") {
                _hwType = HW_FINE_OFFSET;
            } else {
                _hwType = HW_GENERIC;
            }

            float latitude = FLT_MAX, longitude = FLT_MAX;
            QVariantMap coordinates = stationData["coordinates"].toMap();
            if (!coordinates["latitude"].isNull()) {
                latitude = coordinates["latitude"].toFloat();
            }
            if (!coordinates["longitude"].isNull()) {
                longitude = coordinates["longitude"].toFloat();
            }

            int sample_interval = 5;
            if (stationData.contains("interval")) {
                sample_interval = stationData["interval"].toInt() / 60;
            }


            QMap<ExtraColumn, QString> extraColumnNames;
            ExtraColumns extraColumns;

            FetchStationInfoWebTask::parseSensorConfig(
                        stationData, &extraColumnNames, &extraColumns);

            _dataSource->updateStation(
                    _stationName,
                    stationData["desc"].toString(),
                    stationData["hw_type"].toMap()["code"].toString().toLower(),
                    sample_interval,
                    latitude,
                    longitude,
                    stationData["coordinates"].toMap()["altitude"].toFloat(),
                    _isSolarAvailable,
                    davis_broadcast_id,
                    extraColumns,
                    extraColumnNames
            );

            return true;
        }
    }

    return false;
}

