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
        _apiLevel = stationInfo.apiLevel;

        finishWork();
    } else {
        qDebug() << "Haven't fetched SYSCONFIG recently - queueing request and requeueing self.";
        lastSysConfig = url;

        FetchStationInfoWebTask* task = new FetchStationInfoWebTask(
                    _baseUrl, _stationCode, _dataSource);

        emit queueTask(task);

        FetchSamplesWebTask *task2 = new FetchSamplesWebTask(
                    _baseUrl, _stationCode, _columns, _startTime, _endTime,
                    _aggregateFunction, _groupType, _groupMinutes, _select,
                    _dataSource);
        emit queueTask(task2);
        emit finished();
    }
}


void FetchSamplesWebTask::networkReplyReceived(QNetworkReply *reply) {
    // This task doesn't make any network requests.
    reply->deleteLater();
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
                _baseUrl, _apiLevel, _stationCode, request, _select, _dataSource);
    emit queueTask(task);
    emit finished();
}

