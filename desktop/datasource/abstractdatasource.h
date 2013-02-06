#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QProgressDialog>
#include <QWidget>

#include "abstractlivedatasource.h"

typedef struct _SampleSet {
    unsigned long sampleCount;

    // Timestamp for each sample
    QVector<double> timestamp;

    // Temperature
    QVector<double> temperature;
    QVector<double> dewPoint;
    QVector<double> apparentTemperature;
    QVector<double> windChill;
    QVector<double> indoorTemperature;

    // Humidity
    QVector<double> humidity;
    QVector<double> indoorHumidity;

    // Pressure
    QVector<double> pressure;
    QVector<double> rainfall;
} SampleSet;

class AbstractDataSource : public AbstractLiveDataSource2
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

signals:
    void samplesReady(SampleSet samples);

protected:
    QScopedPointer<QProgressDialog> progressDialog;
};

#endif // ABSTRACTDATASOURCE_H
