#include "abstractdatasource.h"

AbstractDataSource::AbstractDataSource(QWidget *parentWidget, QObject *parent) :
    AbstractLiveDataSource(parent)
{
    progressDialog.reset(new QProgressDialog(parentWidget));
}

void AbstractDataSource::fetchSamples(DataSet dataSet) {
    fetchSamples(dataSet.columns,
                 dataSet.startTime,
                 dataSet.endTime,
                 dataSet.aggregateFunction,
                 dataSet.groupType,
                 dataSet.customGroupMinutes);
}
