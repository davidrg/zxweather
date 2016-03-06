#ifndef SAMPLECOLUMNS_H
#define SAMPLECOLUMNS_H

#include <QFlags>
#include <QDateTime>

#include "aggregate.h"

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
    SC_WindDirection       = 0x1000,
    SC_SolarRadiation      = 0x2000,
    SC_UV_Index            = 0x4000
};
Q_DECLARE_FLAGS(SampleColumns, SampleColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(SampleColumns)

#define ALL_SAMPLE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection | SC_Timestamp | \
    SC_SolarRadiation | SC_UV_Index)

#define TEMPERATURE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint)

#define HUMIDITY_COLUMNS (SC_Humidity | SC_IndoorHumidity)

#define WIND_COLUMNS (SC_WindDirection | SC_AverageWindSpeed | \
    SC_GustWindSpeed)

#define SOLAR_COLUMNS (SC_SolarRadiation | SC_UV_Index)

#define OTHER_COLUMNS (SC_Pressure | SC_Rainfall)

typedef uint16_t dataset_id_t;

/** Describes a set of columns to be plotted in a chart along with the
 * timespan they should be plotted over. It also includes an ID will be
 * set by the ChartWindow to a unique value for use as a key in hashtables,
 * etc.
 */
struct DataSet {
    dataset_id_t id;    /*!< Unique identifier for the dataset */
    SampleColumns columns;  /*!< Columns that should be displayed for the dataset */
    QDateTime startTime;    /*!< Start of the timespan */
    QDateTime endTime;      /*!< End of the timespan */

    AggregateFunction aggregateFunction;    /*!< Function to be used for grouping (if any) */
    AggregateGroupType groupType; /*!< Grouping type to use (if any) */
    uint32_t customGroupMinutes;  /*!< Number of minutes to group by if group type is AGT_Custom */

    /** Compares two DataSets to see if they're equal for data caching purposes.
     *
     * @param other The other dataset
     * @return true if the two datasets are equal.
     */
    bool operator==(const DataSet& other);
};




#endif // SAMPLECOLUMNS_H
