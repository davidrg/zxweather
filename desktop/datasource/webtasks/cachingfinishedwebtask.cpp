#include "cachingfinishedwebtask.h"



CachingFinishedWebTask::CachingFinishedWebTask(
        QString baseUrl, QString stationCode, WebDataSource *ds):
    AbstractWebTask(baseUrl, stationCode, ds)
{

}


void CachingFinishedWebTask::beginTask() {
    _dataSource->finishedCaching();
    emit finished();
}
