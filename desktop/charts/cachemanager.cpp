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
        qDebug() << "Queueing dataset" << ds.id
                 << "for fetch with timespan" << ds.startTime
                 << "-" << ds.endTime;

        requestedDataSets.append(ds.id);
    }

    getNextDataSet();
}

void CacheManager::getNextDataSet() {
    qDebug() << "Fetching next dataset...";
    Q_ASSERT_X(!dataSetsToFetch.isEmpty(), "getNextDataSet", "No more data sets to fetch");

    DataSet ds = dataSetsToFetch.first();
    DataSet cachedDataSet = datasetCache[ds.id];

    if (cachedDataSet == ds) {
        // We already have this dataset in cache. Skip it.

        qDebug() << "Skipping dataset" << ds.id
                 << "(start" << ds.startTime << ", end"
                 << ds.endTime << ", columns" << ds.columns << ", function"
                 << ds.aggregateFunction << ", grouping" << ds.groupType
                 << ", minutes" << ds.customGroupMinutes
                 << ") - cached data set is identical";

        Q_ASSERT_X(ds.startTime == cachedDataSet.startTime,
                   "getNextDataSet", "cached start time matches");
        Q_ASSERT_X(ds.endTime == cachedDataSet.endTime,
                   "getNextDataSet", "cached endsdffdf time matches");

        // Pull the data from cache
        samplesReady(sampleCache[ds.id]);

    }
    else if (ds.id == cachedDataSet.id &&
             ds.startTime == cachedDataSet.startTime &&
             ds.endTime == cachedDataSet.endTime &&
             ds.aggregateFunction == cachedDataSet.aggregateFunction &&
             ds.groupType == cachedDataSet.groupType &&
             ds.customGroupMinutes == cachedDataSet.customGroupMinutes) {
        // Only the columns have changed. We should be able to pull some or all data
        // from cache.
        if ((ds.columns & cachedDataSet.columns) == ds.columns) {
            // Some columns have been removed from the dataset but no new ones were added.
            // This means that we can just return data from cache. The WeatherPlotter will
            // simply ignore any columns in the SampleSet it didn't request.

            qDebug() << "Skipping dataset" << ds.id
                     << "(start" << ds.startTime << ", end"
                     << ds.endTime << ", columns" << ds.columns << ", function"
                     << ds.aggregateFunction << ", grouping" << ds.groupType
                     << ", minutes" << ds.customGroupMinutes
                     << ") - column superset already cached";

            samplesReady(sampleCache[ds.id]);
        } else {
            // These bits are set both in the cache and request.
            SampleColumns commonColumns = ds.columns & cachedDataSet.columns;

            // Find columns that are in ds.columns but not in cachedDataSet.columns
            SampleColumns newColumns = ds.columns & (~commonColumns);

            // There are some new flags.
            qDebug() << "Requested dataset" << ds.id
                     << "(start" << ds.startTime << ", end"
                     << ds.endTime << ", columns" << ds.columns << ", function"
                     << ds.aggregateFunction << ", grouping" << ds.groupType
                     << ", minutes" << ds.customGroupMinutes
                     << ") is a superset of the cached dataset. "
                        "Fetching new columns (" << newColumns << ") only.";

            // Make a note of the new columns we're fetching. We'll have to merge these
            // into the cache later.
            dataSetsToFetch[0].columns = newColumns;

            // Fetch the data.
            dataSource->fetchSamples(newColumns, ds.startTime, ds.endTime,
                                     ds.aggregateFunction, ds.groupType, ds.customGroupMinutes);
        }
    } else {
        // We either don't have the dataset cached or the datasets timespan has changed.
        // Its too hard trying to merge the timespan so we'll just request the entire
        // dataset again.

        qDebug() << "Fetching columns" << ds.columns << "between" << ds.startTime
                 << "and" << ds.endTime << "for data set" << ds.id;



        dataSource->fetchSamples(ds.columns, ds.startTime, ds.endTime,
                                 ds.aggregateFunction, ds.groupType, ds.customGroupMinutes);
    }
}

void CacheManager::samplesReady(SampleSet samples) {
    qDebug() << "Samples ready";

    DataSet ds = dataSetsToFetch.takeFirst();
    DataSet cachedDataSet = datasetCache[ds.id];

    if (ds.startTime != cachedDataSet.startTime ||
        ds.endTime != cachedDataSet.endTime ||
        ds.id != cachedDataSet.id ||
        ds.aggregateFunction != cachedDataSet.aggregateFunction ||
        ds.groupType != cachedDataSet.groupType ||
        ds.customGroupMinutes != cachedDataSet.customGroupMinutes) {
        // Timespan or aggregate has changed. Reset the cached dataset to what
        //we just received.

        sampleCache[ds.id] = samples;
        datasetCache[ds.id] = ds;
    } else if (ds.id == cachedDataSet.id) {
        // We have the dataset cached with matching timespan and id but the columns
        // are different. If any of the columns in the SampleSet are missing from
        // the cache then merge them in.
        if ((ds.columns & cachedDataSet.columns) != ds.columns)
            mergeSampleSet(ds.id, samples, ds.columns);
        else
            qDebug() << "Requested samples for data set" << ds.id
                     << "already in cache - no merge necessary";
    }

    if (dataSetsToFetch.isEmpty()) {
        qDebug() << "Finished fetching data.";
        // No more datasets to fetch. Send all the requested ones back.
        QMap<dataset_id_t, SampleSet> data;
        foreach (dataset_id_t id, requestedDataSets) {
            qDebug() <<"Dataset" << id << "span" << ds.startTime
                    << "-" << ds.endTime << "...";
            data[id] = sampleCache[id];
        }
        emit dataSetsReady(data);

        dataSetsToFetch.clear();
        requestedDataSets.clear();
    } else {
        qDebug() << "Datasets remaining to fetch:" << dataSetsToFetch.count();
        // Still more work to do.
        getNextDataSet();
    }
}

void CacheManager::mergeSampleSet(dataset_id_t dataSetId, SampleSet samples, SampleColumns columns)
{
    qDebug() << "Merging in columns" << columns << "for dataset" << dataSetId;
    if (columns.testFlag(SC_Temperature))
        sampleCache[dataSetId].temperature = samples.temperature;

    if (columns.testFlag(SC_IndoorTemperature))
        sampleCache[dataSetId].indoorTemperature = samples.indoorTemperature;

    if (columns.testFlag(SC_ApparentTemperature))
        sampleCache[dataSetId].apparentTemperature = samples.apparentTemperature;

    if (columns.testFlag(SC_DewPoint))
        sampleCache[dataSetId].dewPoint = samples.dewPoint;

    if (columns.testFlag(SC_WindChill))
        sampleCache[dataSetId].windChill = samples.windChill;

    if (columns.testFlag(SC_Humidity))
        sampleCache[dataSetId].humidity = samples.humidity;

    if (columns.testFlag(SC_IndoorHumidity))
        sampleCache[dataSetId].indoorHumidity = samples.indoorHumidity;

    if (columns.testFlag(SC_Pressure))
        sampleCache[dataSetId].pressure = samples.pressure;

    if (columns.testFlag(SC_Rainfall))
        sampleCache[dataSetId].rainfall = samples.rainfall;

    if (columns.testFlag(SC_AverageWindSpeed))
        sampleCache[dataSetId].averageWindSpeed = samples.averageWindSpeed;

    if (columns.testFlag(SC_GustWindSpeed))
        sampleCache[dataSetId].gustWindSpeed = samples.gustWindSpeed;

    if (columns.testFlag(SC_WindDirection))
        sampleCache[dataSetId].windDirection = samples.windDirection;

    if (columns.testFlag(SC_UV_Index))
        sampleCache[dataSetId].uvIndex = samples.uvIndex;

    if (columns.testFlag(SC_SolarRadiation))
        sampleCache[dataSetId].solarRadiation = samples.solarRadiation;

    if (columns.testFlag(SC_HighTemperature))
        sampleCache[dataSetId].highTemperature = samples.highTemperature;

    if (columns.testFlag(SC_LowTemperature))
        sampleCache[dataSetId].lowTemperature = samples.lowTemperature;

    if (columns.testFlag(SC_HighSolarRadiation))
        sampleCache[dataSetId].highSolarRadiation = samples.highSolarRadiation;

    if (columns.testFlag(SC_HighUVIndex))
        sampleCache[dataSetId].highUVIndex = samples.highUVIndex;

    if (columns.testFlag(SC_GustWindDirection))
        sampleCache[dataSetId].gustWindDirection = samples.gustWindDirection;

    if (columns.testFlag(SC_HighRainRate))
        sampleCache[dataSetId].highRainRate = samples.highRainRate;

    if (columns.testFlag(SC_Reception))
        sampleCache[dataSetId].reception = samples.reception;

    if (columns.testFlag(SC_Evapotranspiration))
        sampleCache[dataSetId].evapotranspiration = samples.evapotranspiration;

    // Not supported: SC_ForecastRuleId


    datasetCache[dataSetId].columns |= columns;
}

void CacheManager::sampleRetrievalError(QString message) {
    dataSetsToFetch.clear();
    requestedDataSets.clear();
    flushCache();
    emit retreivalError(message);
}
