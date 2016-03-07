#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QDateTime>
#include <QProgressDialog>
#include <QWidget>

#include "abstractlivedatasource.h"
#include "sampleset.h"

class AbstractDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit AbstractDataSource(QWidget* parentWidget = 0, QObject *parent = 0);

    /** Gets all samples for the supplied time range. When the samples have
     * finished downloading the samplesReady signal will be emitted with the
     * requested sample set.
     *
     * @param columns The columns to include in the dataset.
     * @param startTime Minimum timestamp.
     * @param endTime Maximum timestamp. Defaults to now.
     * @return A set of all samples between the two timestamps (inclusive)
     */
    virtual void fetchSamples(
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0) = 0;

    virtual void fetchSamples(DataSet dataSet);

    virtual hardware_type_t getHardwareType() = 0;

signals:
    /** Emitted when samples have been retrieved and are ready for processing.
     *
     * @param samples Requested samples.
     */
    void samplesReady(SampleSet samples);

    /** Emitted when an error occurs during sample retrieval which forced the
     * request to be aborted.
     *
     * @param message Message describing the error.
     */
    void sampleRetrievalError(QString message);

protected:
    QScopedPointer<QProgressDialog> progressDialog;
};



#endif // ABSTRACTDATASOURCE_H
