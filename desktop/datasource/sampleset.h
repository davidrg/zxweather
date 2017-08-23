#ifndef SAMPLESET_H
#define SAMPLESET_H

#include <QVector>
#include <QMap>

#include "samplecolumns.h"

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

    QVector<double> solarRadiation;
    QVector<double> uvIndex;
} SampleSet;

/** Reserve space in the sample sets data structures for the specified number
 * of columns.
 *
 * @param samples Sample set to reserve space in
 * @param size Number of expected records
 * @param columns Columns that will be populated.
 */
void ReserveSampleSetSpace(SampleSet& samples,
                           int size,
                           SampleColumns columns);

typedef struct _Sample {
    // Timestamp
    QDateTime timestamp;

    // Temperature
    double temperature;
    double dewPoint;
    double apparentTemperature;
    double windChill;
    double indoorTemperature;

    // Humidity
    double humidity;
    double indoorHumidity;

    // Pressure / rain
    double pressure;
    double rainfall;

    // Wind
    double averageWindSpeed;
    double gustWindSpeed;
    bool windDirectionValid;
    uint windDirection;

    // Sun
    bool solarRadiationValid;
    double solarRadiation;
    bool uvIndexValid;
    double uvIndex;
} Sample;

#endif // SAMPLESET_H
