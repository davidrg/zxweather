#include "cachemanager.h"

#include <QtDebug>

CacheManager::CacheManager(QObject *parent) :
    QObject(parent)
{
}

void CacheManager::setDataSource(AbstractDataSource *dataSource) {
    connect(dataSource, SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(dataSource, SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(sampleRetrievalError(QString)));

    this->dataSource.reset(dataSource);
}

void CacheManager::flushCache() {
    sampleCache.clear();
    datasetCache.clear();
}

// TODO: if all that has changed is the set of columns, only fetch data for those new columns.
void CacheManager::getDataSets(QList<DataSet> dataSets) {
    Q_ASSERT_X(!dataSource.isNull(), "getDataSets", "DataSource is null");

    if (!dataSetsToFetch.isEmpty()) {
        qWarning() << "Already fetching datasets. Can't do two at once. Ignoring request.";
        return;
    }

    dataSetsToFetch = dataSets;
    foreach(DataSet ds, dataSets) {
        requestedDataSets.append(ds.id);
    }


    getNextDataSet();
}

void CacheManager::getNextDataSet() {
    qDebug() << "Fetching next dataset...";
    DataSet ds = dataSetsToFetch.first();
    DataSet cachedDataSet = datasetCache[ds.id];

    if (cachedDataSet == ds) {
        // We already have this dataset in cache. Skip it.

        qDebug() << "Skipping dataset" << ds.id
                 << "(start" << ds.startTime << ", end"
                 << ds.endTime << ", columns" << ds.columns
                 << ") - already cached";
        dataSetsToFetch.removeFirst();
        getNextDataSet();

    } else {
        // We either don't have the dataset cached or the dataset has changed
        // and so needs to be fetched again.

        qDebug() << "Fetching columns" << ds.columns << "between" << ds.startTime
                 << "and" << ds.endTime << "for data set" << ds.id;

        dataSource->fetchSamples(ds.columns, ds.startTime, ds.endTime);
    }
}

void CacheManager::samplesReady(SampleSet samples) {
    qDebug() << "Samples ready";

    DataSet ds = dataSetsToFetch.takeFirst();

    // Cache future samples for fast refreshing.
    sampleCache[ds.id] = samples;
    datasetCache[ds.id] = ds;

    if (dataSetsToFetch.isEmpty()) {
        qDebug() << "Finished fetching data.";
        // No more datasets to fetch. Send all the requested ones back.
        QMap<dataset_id_t, SampleSet> data;
        foreach (dataset_id_t id, requestedDataSets) {
            qDebug() <<"Dataset" << id << "...";
            data[id] = sampleCache[ds.id];
        }
        emit dataSetsReady(data);

        dataSetsToFetch.clear();
        requestedDataSets.clear();
    } else {
        // Still more work to do.
        getNextDataSet();
    }
}

void CacheManager::sampleRetrievalError(QString message) {
    dataSetsToFetch.clear();
    requestedDataSets.clear();
    flushCache();
    emit retreivalError(message);
}
