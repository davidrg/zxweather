#include "selectsampleswebtask.h"

#include "datasource/webcachedb.h"

SelectSamplesWebTask::SelectSamplesWebTask(QString baseUrl,
                                           QString stationCode,
                                           request_data_t requestData,
                                           WebDataSource* ds)
      : AbstractWebTask(baseUrl, stationCode, ds) {

    _requestData = requestData;
}

// This task doesn't issue any HTTP requests so we're not expecting to
// receive any data.
void SelectSamplesWebTask::networkReplyReceived(QNetworkReply *reply) {
    Q_UNUSED(reply)
}

void SelectSamplesWebTask::beginTask() {
    SampleSet samples = WebCacheDB::getInstance().retrieveDataSet(
                _stationDataUrl,
                _requestData.startTime,
                _requestData.endTime,
                _requestData.columns,
                _requestData.aggregateFunction,
                _requestData.groupType,
                _requestData.groupMinutes,
                _dataSource->progressListener);

    _dataSource->fireSamplesReady(samples);

    emit finished();
}
