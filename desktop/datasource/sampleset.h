#ifndef SAMPLESET_H
#define SAMPLESET_H

#include <QVector>

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
} SampleSet;

void ReserveSampleSetSpace(SampleSet& samples, int size);

#endif // SAMPLESET_H
