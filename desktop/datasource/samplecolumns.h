#ifndef SAMPLECOLUMNS_H
#define SAMPLECOLUMNS_H

#include <QFlags>
#include <QDateTime>

enum SampleColumn {
    SC_NoColumns           = 0x0000,
    SC_Timestamp           = 0x0001,
    SC_Temperature         = 0x0002,
    SC_IndoorTemperature   = 0x0004,
    SC_ApparentTemperature = 0x0008,
    SC_WindChill           = 0x0010,
    SC_DewPoint            = 0x0020,
    SC_Humidity            = 0x0040,
    SC_IndoorHumidity      = 0x0080,
    SC_Pressure            = 0x0100,
    SC_Rainfall            = 0x0200,
    SC_AverageWindSpeed    = 0x0400,
    SC_GustWindSpeed       = 0x0800,
    SC_WindDirection       = 0x1000
};
Q_DECLARE_FLAGS(SampleColumns, SampleColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(SampleColumns)

#define ALL_SAMPLE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection | SC_Timestamp)

#define TEMPERATURE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint)

#define HUMIDITY_COLUMNS (SC_Humidity | SC_IndoorHumidity)

#define WIND_COLUMNS (SC_WindDirection | SC_AverageWindSpeed | \
    SC_GustWindSpeed)

#define OTHER_COLUMNS (SC_Pressure | SC_Rainfall)

typedef uint16_t dataset_id_t;

/** Describes a set of columns to be plotted in a chart along with the
 * timespan they should be plotted over. It also includes an ID will be
 * set by the ChartWindow to a unique value for use as a key in hashtables,
 * etc.
 */
typedef struct {
    dataset_id_t id;
    SampleColumns columns;
    QDateTime startTime;
    QDateTime endTime;
    QString axisLabel;
} DataSet;

/** Compares two DataSets to see if they're equal for data caching purposes.
 * Note that this only includes start time, end time, column set and id.
 *
 * @param lhs First dataset
 * @param rhs Second dataset
 * @return true if the two datasets are equal.
 */
bool operator==(const DataSet& lhs, const DataSet& rhs);

#endif // SAMPLECOLUMNS_H
