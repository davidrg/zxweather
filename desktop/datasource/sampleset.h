#ifndef SAMPLESET_H
#define SAMPLESET_H

#include <QVector>
#include <QMap>

#include "samplecolumns.h"
#include "unit_conversions.h"

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
    QMap<uint,uint> gustWindDirection; // Davis only

    // Solar columns (Vantage Pro2 Plus only)
    QVector<double> solarRadiation;
    QVector<double> uvIndex;
    QVector<double> evapotranspiration;

    // Records columns (Davis only)
    QVector<double> highTemperature;
    QVector<double> lowTemperature;
    QVector<double> highRainRate;
    QVector<double> highSolarRadiation; // Pro2 plus only
    QVector<double> highUVIndex; // Pro2 plus only

    // Misc
    QVector<double> reception; // wireless davis only
    QVector<int> forecastRuleId;

    // Leaf columns
    QVector<double> leafWetness1;
    QVector<double> leafWetness2;
    QVector<double> leafTemperature1;
    QVector<double> leafTemperature2;

    // Soil columns
    QVector<double> soilMoisture1;
    QVector<double> soilMoisture2;
    QVector<double> soilMoisture3;
    QVector<double> soilMoisture4;
    QVector<double> soilTemperature1;
    QVector<double> soilTemperature2;
    QVector<double> soilTemperature3;
    QVector<double> soilTemperature4;

    // Temp+Hum statoins
    QVector<double> extraHumidity1;
    QVector<double> extraHumidity2;
    QVector<double> extraTemperature1;
    QVector<double> extraTemperature2;
    QVector<double> extraTemperature3;

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

void AppendNullSamples(SampleSet &samples, SampleColumns columns, QDateTime startTime, QDateTime endTime, int intervalSeconds);

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

    // Leaf
    double leafWetness1;
    double leafWetness2;
    double leafTemperature1;
    double leafTemperature2;

    // Soil
    double soilMoisture1;
    double soilMoisture2;
    double soilMoisture3;
    double soilMoisture4;
    double soilTemperature1;
    double soilTemperature2;
    double soilTemperature3;
    double soilTemperature4;

    // Temp+Humidity stations
    double extraHumidity1;
    double extraHumidity2;
    double extraTemperature1;
    double extraTemperature2;
    double extraTemperature3;
} Sample;

inline UnitConversions::unit_t SampleColumnUnits(StandardColumn column) {
    using namespace UnitConversions;

    switch (column) {
    case SC_Temperature:
    case SC_IndoorTemperature:
    case SC_ApparentTemperature:
    case SC_WindChill:
    case SC_DewPoint:
    case SC_HighTemperature:
    case SC_LowTemperature:
        return U_CELSIUS;
    case SC_Humidity:
    case SC_IndoorHumidity:
        return U_HUMIDITY;
    case SC_Pressure:
        return U_HECTOPASCALS;
    case SC_Rainfall:
    case SC_Evapotranspiration:
        return U_MILLIMETERS;
    case SC_AverageWindSpeed:
    case SC_GustWindSpeed:
        return U_METERS_PER_SECOND;
    case SC_WindDirection:
    case SC_GustWindDirection:
        return U_DEGREES;
    case SC_SolarRadiation:
    case SC_HighSolarRadiation:
        return U_WATTS_PER_SQUARE_METER;
    case SC_UV_Index:
    case SC_HighUVIndex:
        return U_UV_INDEX;
    case SC_HighRainRate:
        return U_MILLIMETERS_PER_HOUR;

    case SC_NoColumns:
    case SC_Timestamp:
    case SC_Reception:
    case SC_ForecastRuleId:
        return U_UNKNOWN;
    }

    return U_UNKNOWN;
}

QVector<double> SampleColumnInUnits(SampleSet samples, StandardColumn column, UnitConversions::unit_t units);

inline UnitConversions::unit_t SampleColumnUnits(ExtraColumn column) {
    using namespace UnitConversions;

    switch (column) {
    case EC_LeafTemperature1:
    case EC_LeafTemperature2:
    case EC_SoilTemperature1:
    case EC_SoilTemperature2:
    case EC_SoilTemperature3:
    case EC_SoilTemperature4:
    case EC_ExtraTemperature1:
    case EC_ExtraTemperature2:
    case EC_ExtraTemperature3:
        return U_CELSIUS;
    case EC_ExtraHumidity1:
    case EC_ExtraHumidity2:
        return U_HUMIDITY;
    case EC_LeafWetness1:
    case EC_LeafWetness2:
        return U_LEAF_WETNESS;
    case EC_SoilMoisture1:
    case EC_SoilMoisture2:
    case EC_SoilMoisture3:
    case EC_SoilMoisture4:
        return U_CENTIBAR;
    default:
        return U_UNKNOWN;
    }

    return U_UNKNOWN;
}

QVector<double> SampleColumnInUnits(SampleSet samples, ExtraColumn column, UnitConversions::unit_t units);

#endif // SAMPLESET_H
