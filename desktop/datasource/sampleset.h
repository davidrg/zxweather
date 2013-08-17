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

enum SampleColumn {
    SC_NoColumns           = 0x000,
    SC_Temperature         = 0x001,
    SC_IndoorTemperature   = 0x002,
    SC_ApparentTemperature = 0x004,
    SC_WindChill           = 0x008,
    SC_DewPoint            = 0x010,
    SC_Humidity            = 0x020,
    SC_IndoorHumidity      = 0x040,
    SC_Pressure            = 0x080,
    SC_Rainfall            = 0x100,
    SC_AverageWindSpeed    = 0x200,
    SC_GustWindSpeed       = 0x400,
    SC_WindDirection       = 0x800
};
Q_DECLARE_FLAGS(SampleColumns, SampleColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(SampleColumns)

#define ALL_SAMPLE_COLUMNS SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection

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

#endif // SAMPLESET_H
