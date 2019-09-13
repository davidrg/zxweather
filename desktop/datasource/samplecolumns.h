#ifndef SAMPLECOLUMNS_H
#define SAMPLECOLUMNS_H

#include <QFlags>
#include <QDateTime>
#include <QMap>

// TODO: Remove this when C++11 is the minimum supported standard
#define __STDC_LIMIT_MACROS
#include <stdint.h>

#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#include "aggregate.h"

enum StandardColumn {
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
    SC_SolarRadiation      = 0x00002000,    // Vantage Pro2+
    SC_UV_Index            = 0x00004000,    // Vantage Pro2+
    SC_Reception           = 0x00008000,    // Wireless davis
    SC_HighTemperature     = 0x00010000,    // Davis
    SC_LowTemperature      = 0x00020000,    // Davis
    SC_HighRainRate        = 0x00040000,    // Davis
    SC_GustWindDirection   = 0x00080000,    // Davis
    SC_Evapotranspiration  = 0x00100000,    // Vantage Pro2+
    SC_HighSolarRadiation  = 0x00200000,    // Vantage Pro2+
    SC_HighUVIndex         = 0x00400000,    // Vantage Pro2+
    SC_ForecastRuleId      = 0x00800000     // Davis
    // Space for 8 more columns
};
Q_DECLARE_FLAGS(StandardColumns, StandardColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(StandardColumns)

enum ExtraColumn {  // Columns for VP2 Expansion stations
    EC_NoColumns           = 0x00000000,
    EC_LeafWetness1        = 0x00000001,
    EC_LeafWetness2        = 0x00000002,
    EC_LeafTemperature1    = 0x00000004,
    EC_LeafTemperature2    = 0x00000008,
    EC_SoilMoisture1       = 0x00000010,
    EC_SoilMoisture2       = 0x00000020,
    EC_SoilMoisture3       = 0x00000040,
    EC_SoilMoisture4       = 0x00000080,
    EC_SoilTemperature1    = 0x00000100,
    EC_SoilTemperature2    = 0x00000200,
    EC_SoilTemperature3    = 0x00000400,
    EC_SoilTemperature4    = 0x00000800,
    EC_ExtraHumidity1      = 0x00001000,
    EC_ExtraHumidity2      = 0x00002000,
    EC_ExtraTemperature1   = 0x00004000,
    EC_ExtraTemperature2   = 0x00008000,
    EC_ExtraTemperature3   = 0x00010000
    // Space for 15 more columns
};
Q_DECLARE_FLAGS(ExtraColumns, ExtraColumn)

Q_DECLARE_OPERATORS_FOR_FLAGS(ExtraColumns)

typedef struct _sample_column {
    StandardColumns standard;
    ExtraColumns extra;
} SampleColumns;

#define ALL_SAMPLE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_Humidity | \
    SC_IndoorHumidity | SC_Pressure | SC_Rainfall | SC_AverageWindSpeed | \
    SC_GustWindSpeed | SC_WindDirection | SC_Timestamp | \
    SC_SolarRadiation | SC_UV_Index | SC_Reception | SC_HighTemperature | \
    SC_LowTemperature | SC_HighRainRate | SC_GustWindDirection | \
    SC_Evapotranspiration | SC_HighSolarRadiation | SC_HighUVIndex | \
    SC_ForecastRuleId )

#define ALL_EXTRA_COLUMNS (EC_LeafWetness1 | EC_LeafWetness2 | EC_LeafTemperature1 | \
    EC_LeafTemperature2 | EC_SoilMoisture1 | EC_SoilMoisture2 | EC_SoilMoisture3 | \
    EC_SoilMoisture4 | EC_SoilTemperature1 | EC_SoilTemperature2 | \
    EC_SoilTemperature3 | EC_SoilTemperature4 | EC_ExtraHumidity1 | EC_ExtraHumidity2 | \
    EC_ExtraTemperature1 | EC_ExtraTemperature2 | EC_ExtraTemperature3 )

#define TEMPERATURE_COLUMNS (SC_Temperature | SC_IndoorTemperature | \
    SC_ApparentTemperature | SC_WindChill | SC_DewPoint | SC_HighTemperature | \
    SC_LowTemperature )

#define EXTRA_TEMPERATURE_COLUMNS (EC_ExtraTemperature1 | EC_ExtraTemperature2 | EC_ExtraTemperature3)

#define HUMIDITY_COLUMNS (SC_Humidity | SC_IndoorHumidity)

#define EXTRA_HUMIDITY_COLUMNS (EC_ExtraHumidity1 | EC_ExtraHumidity2)

#define WIND_COLUMNS (SC_WindDirection | SC_AverageWindSpeed | \
    SC_GustWindSpeed)

// These columns are only available on the Vantage Pro 2 Plus
#define SOLAR_COLUMNS (SC_SolarRadiation | SC_UV_Index | SC_Evapotranspiration \
    | SC_HighSolarRadiation | SC_HighUVIndex)

#define OTHER_COLUMNS (SC_Pressure | SC_Rainfall | SC_HighRainRate | \
    SC_Reception)

#define SOIL_COLUMNS (EC_SoilMoisture1 | EC_SoilMoisture2 | \
    EC_SoilMoisture3 | EC_SoilMoisture4 | EC_SoilTemperature1 | \
    EC_SoilTemperature2 | EC_SoilTemperature3 | EC_SoilTemperature4)

#define LEAF_COLUMNS (EC_LeafWetness1 | EC_LeafWetness2 | \
    EC_LeafTemperature1 | EC_LeafTemperature2)

// These are only available on Davis hardware. Theyre the high and low
// values during the archive period (highest temperature reported during the
// 5 minutes rather than the average, etc)
#define RECORD_COLUMNS (SC_HighTemperature | SC_LowTemperature | \
    SC_HighRainRate | SC_HighSolarRadiation | SC_HighUVIndex )

// These columns are only available on davis hardware
#define DAVIS_COLUMNS ( SC_Reception | RECORD_COLUMNS | SC_GustWindDirection | \
    SOLAR_COLUMNS | SC_ForecastRuleId )
#define DAVIS_EXTRA_COLUMNS ALL_EXTRA_COLUMNS

#define SUMMABLE_COLUMNS (SC_Rainfall | SC_Evapotranspiration)

#define EXTRA_SUMMABLE_COLUMNS (EC_NoColumns)

typedef uint16_t dataset_id_t;

#define INVALID_DATASET_ID UINT16_MAX

/** Describes a set of columns to be plotted in a chart along with the
 * timespan they should be plotted over. It also includes an ID will be
 * set by the ChartWindow to a unique value for use as a key in hashtables,
 * etc.
 */
struct DataSet {
    dataset_id_t id;    /*!< Unique identifier for the dataset */
    SampleColumns columns;  /*!< Columns that should be displayed for the dataset */
    QMap<ExtraColumn, QString> extraColumnNames;

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
