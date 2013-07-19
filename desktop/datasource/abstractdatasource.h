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
     * @param startTime Minimum timestamp
     * @param endTime Maximum timestamp. Defaults to now.
     * @return A set of all samples between the two timestamps (inclusive)
     */
    virtual void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime()) = 0;

    virtual hardware_type_t getHardwareType() = 0;

signals:
    void samplesReady(SampleSet samples);

protected:
    QScopedPointer<QProgressDialog> progressDialog;
};

#endif // ABSTRACTDATASOURCE_H
