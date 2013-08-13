#ifndef SAMPLESET_H
#define SAMPLESET_H

#include <QVector>
#include <QMap>

typedef struct _SampleSet {
    unsigned long sampleCount;

    // Timestamp for each sample
    QVector<uint> timestampUnix;
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

    // Wind speed
    QVector<double> averageWindSpeed;
    QVector<double> gustWindSpeed;

    // Wind direction is a map as not all timestamps have a direction.
    // Wind only has a direction when its blowing - if averageWindSpeed for
    // a given timestamp is 0 then the direction will be undefined.
    QMap<uint,uint> windDirection;
} SampleSet;

void ReserveSampleSetSpace(SampleSet& samples, int size);

#endif // SAMPLESET_H
