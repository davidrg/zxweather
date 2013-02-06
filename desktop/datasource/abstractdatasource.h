#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QProgressDialog>
#include <QWidget>

typedef struct _SampleSet {
    unsigned long sampleCount;

    // Timestamp for each sample
    QVector<double> timestamp;
    double minTimeStamp, maxTimeStamp;

    // Temperature
    QVector<double> temperature;
    double minTemperature, maxTemperature;

    QVector<double> dewPoint;
    double minDewPoint, maxDewPoint;

    QVector<double> apparentTemperature;
    double minApparentTemperature, maxApparentTemperature;

    QVector<double> windChill;
    double minWindChill, maxWindChill;

    QVector<double> indoorTemperature;
    double minIndoorTemperature, maxIndoorTemperature;

    // Humidity
    QVector<double> humidity;
    double minHumidity, maxHumiditiy;

    QVector<double> indoorHumidity;
    double minIndoorHumidity, maxIndoorHumidity;

    // Pressure
    QVector<double> pressure;
    double minPressure, maxPressure;

    QVector<double> rainfall;
    double minRainfall, maxRainfall;

} SampleSet;

class AbstractDataSource : public QObject
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
