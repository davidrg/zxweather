#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QObject>
#include <QList>

#include "datasource/abstractdatasource.h"


/** The CacheManager handles fetching multiple DataSets from a datasource
 * and returning them all at once. If any of the DataSets have been requested
 * in the past data for them will be returned without a trip to the datasource.
 */
class CacheManager : public QObject
{
    Q_OBJECT
public:
    explicit CacheManager(QObject *parent = 0);

    /** Sets the datasource to get data from.
     *
     * @param dataSource Datasource to use for fetching samples.
     */
    void setDataSource(AbstractDataSource *dataSource);

    /** Gets samples for the specified datasets asynchronously.
     * Data will be returned via the dataSetsReady() signal.
     *
     * @param dataSets Datasets to fetch.
     */
    void getDataSets(QList<DataSet> dataSets);

    /** Flushes all cached samples.
     */
    void flushCache();

    /** Refreshes the specified dataset from the datasource.
     *
     * @param dataSet Dataset ID to refresh.
     */
    void refreshDataSet(dataset_id_t dataSetId);

signals:
    /** Datasets fetched in response to a getDataSets() call
     *
     * @param samples Samples retrieived either from cache or the datasource.
     */
    void dataSetsReady(QMap<dataset_id_t, SampleSet> samples);

    /** Emitted when the datasource reports an error.
     *
     * @param message Error message.
     */
    void retreivalError(QString message);

private slots:
    /** Called by the data source when it has finished retrieving samples.
     *
     * @param samples Samples retrieved
     */
    void samplesReady(SampleSet samples);

    /** Called by the datasource when it encounters an error
     *
     * @param message Error message
     */
    void sampleRetrievalError(QString message);

private:
    /** Asks the datasource for data from the next dataset.
     */
    void getNextDataSet();

    /** Merges a new sampleset into an existing cached dataset. This
     * is used when new columns are requested a dataset already in cache.
     *
     * @param dataSetId Dataset to merge the sampleset into
     * @param samples Samples to merge
     * @param columns Which columns in the sampleset to merge
     */
    void mergeSampleSet(dataset_id_t dataSetId, SampleSet samples, SampleColumns columns);

    /** Datasource that we're caching data from
     */
    QScopedPointer<AbstractDataSource> dataSource;

    /** Cache of data for different datasets.
     */
    QMap<dataset_id_t,SampleSet> sampleCache;

    /** These are the datasets we have data cached for.
     */
    QMap<dataset_id_t,DataSet> datasetCache;

    /** List of data sets we still need to request from the datasource.
     */
    QList<DataSet> dataSetsToFetch;

    /** List of data sets that were requested by the user. This
     * is what will be returned via the dataSetsReady() signal.
     */
    QList<dataset_id_t> requestedDataSets;

    /** This is the list of datasets the user asked us to refresh.
     */
    QList<dataset_id_t> refreshingDataSets;
};

#endif // CACHEMANAGER_H
