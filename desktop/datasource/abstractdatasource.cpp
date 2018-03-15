#include "abstractdatasource.h"

AbstractDataSource::AbstractDataSource(AbstractProgressListener *progressListener, QObject *parent) :
    AbstractLiveDataSource(parent)
{
    //progressDialog.reset(new QProgressDialog(parentWidget));
    progressDialog = progressListener;
}

void AbstractDataSource::fetchSamples(DataSet dataSet) {
    fetchSamples(dataSet.columns,
                 dataSet.startTime,
                 dataSet.endTime,
                 dataSet.aggregateFunction,
                 dataSet.groupType,
                 dataSet.customGroupMinutes);
}
