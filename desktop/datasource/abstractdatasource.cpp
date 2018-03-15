#include "abstractdatasource.h"

#include "nullprogresslistener.h"

AbstractDataSource::AbstractDataSource(AbstractProgressListener *progressListener, QObject *parent) :
    AbstractLiveDataSource(parent)
{
    //progressDialog.reset(new QProgressDialog(parentWidget));
    this->progressListener = progressListener;

    if (this->progressListener == NULL) {
       this->progressListener = new NullProgressListener(this);
    }
}

void AbstractDataSource::fetchSamples(DataSet dataSet) {
    fetchSamples(dataSet.columns,
                 dataSet.startTime,
                 dataSet.endTime,
                 dataSet.aggregateFunction,
                 dataSet.groupType,
                 dataSet.customGroupMinutes);
}
