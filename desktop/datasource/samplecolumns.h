#ifndef SAMPLECOLUMNS_H
#define SAMPLECOLUMNS_H

#include <QFlags>
#include <QDateTime>
#include <stdint.h>

#include "aggregate.h"

/*
        #   reception (wireless stations only)
        #   high_temperature
        #   low_temperature
        #   high_rain_rate
        #   gust_wind_direction
        #   evapotranspiration (Vantage Pro2 Plus only)
        #   high_solar_radiation (Vantage Pro2 Plus only)
        #   high_uv_index (Vantage Pro2 Plus only)
    #   forecast_rule_id
*/

enum SampleColumn {
    SC_NoColumns           = 0x00000000,
    SC_Timestamp           = 0x00000001,
    SC_Temperature         = 0x00000002,
    SC_IndoorTemperature   = 0x00000004,
    SC_ApparentTemperature = 0x00000008,
    SC_WindChill           = 0x00000010,
    SC_DewPoint            = 0x00000020,
    SC_Humidity            = 0x00000040,
    SC_IndoorHumidity      = 0x00000080,
    SC_Pressure            = 0x00000100,
    SC_Rainfall            = 0x00000200,
    SC_AverageWindSpeed    = 0x00000400,
    SC_GustWindSpeed       = 0x00000800,
    SC_WindDirection       = 0x00001000,
    SC_SolarRadiation      = 0x00002000,
    SC_UV_Index            = 0x00004000,
    SC_Reception           = 0x00008000,
    SC_HighTemperature     = 0x00010000,
    SC_LowTemperature      = 0x00020000,
    SC_HighRainRate        = 0x00040000,
    SC_GustWindDirection   = 0x00080000,
    SC_Evapotranspiration  = 0x00100000,
    SC_HighSolarRadiation  = 0x00200000,
    SC_HighUVIndex         = 0x00400000,
    SC_ForecastRuleId      = 0x00800000
    // Space for 8 more columns
};
Q_DECLARE_FLAGS(SampleColumns, SampleColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(SampleColumns)

#define ALL_SAMPLE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection | SC_Timestamp | \
    SC_SolarRadiation | SC_UV_Index | SC_Reception | SC_HighTemperature | \
    SC_LowTemperature | SC_HighRainRate | SC_GustWindDirection | \
    SC_Evapotranspiration | SC_HighSolarRadiation | SC_HighUVIndex | \
    SC_ForecastRuleId )

#define TEMPERATURE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_HighTemperature | \
    SC_LowTemperature )

#define HUMIDITY_COLUMNS (SC_Humidity | SC_IndoorHumidity)

#define WIND_COLUMNS (SC_WindDirection | SC_AverageWindSpeed | \
    SC_GustWindSpeed)

// These columns are only available on the Vantage Pro 2 Plus
#define SOLAR_COLUMNS (SC_SolarRadiation | SC_UV_Index | SC_Evapotranspiration \
    | SC_HighSolarRadiation | SC_HighUVIndex)

#define OTHER_COLUMNS (SC_Pressure | SC_Rainfall | SC_HighRainRate | \
    SC_Reception)

// These are only available on Davis hardware. Theyre the high and low
// values during the archive period (highest temperature reported during the
// 5 minutes rather than the average, etc)
#define RECORD_COLUMNS (SC_HighTemperature | SC_LowTemperature | \
    SC_HighRainRate | SC_HighSolarRadiation | SC_HighUVIndex )

// These columns are only available on davis hardware
#define DAVIS_COLUMNS ( SC_Reception | RECORD_COLUMNS | SC_GustWindDirection | \
    SOLAR_COLUMNS | SC_ForecastRuleId )

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

    QString title;

    /** Compares two DataSets to see if they're equal for data caching purposes.
     *
     * @param other The other dataset
     * @return true if the two datasets are equal.
     */
    bool operator==(const DataSet& other);
};




#endif // SAMPLECOLUMNS_H
