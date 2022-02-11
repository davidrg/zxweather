#include "sampleset.h"
#include <QtDebug>
#include "compat.h"

#if (QT_VERSION < QT_VERSION_CHECK(5,2,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

void ReserveSampleSetSpace(SampleSet& samples, int size, SampleColumns columns)
{
    qDebug() << "Reserving space for" << size << "samples.";
    samples.timestampUnix.reserve(size);
    samples.sampleCount = size;
    samples.timestamp.reserve(size);

    if (columns.standard.testFlag(SC_Temperature))
        samples.temperature.reserve(size);

    if (columns.standard.testFlag(SC_DewPoint))
        samples.dewPoint.reserve(size);

    if (columns.standard.testFlag(SC_ApparentTemperature))
        samples.apparentTemperature.reserve(size);

    if (columns.standard.testFlag(SC_WindChill))
        samples.windChill.reserve(size);

    if (columns.standard.testFlag(SC_IndoorTemperature))
        samples.indoorTemperature.reserve(size);

    if (columns.standard.testFlag(SC_Humidity))
        samples.humidity.reserve(size);

    if (columns.standard.testFlag(SC_IndoorHumidity))
        samples.indoorHumidity.reserve(size);

    if (columns.standard.testFlag(SC_Pressure))
        samples.pressure.reserve(size);

    if (columns.standard.testFlag(SC_AbsolutePressure))
        samples.absolutePressure.reserve(size);

    if (columns.standard.testFlag(SC_MeanSeaLevelPressure))
        samples.meanSeaLevelPressure.reserve(size);

    if (columns.standard.testFlag(SC_Rainfall))
        samples.rainfall.reserve(size);

    if (columns.standard.testFlag(SC_AverageWindSpeed))
        samples.averageWindSpeed.reserve(size);

    if (columns.standard.testFlag(SC_GustWindSpeed))
        samples.gustWindSpeed.reserve(size);

    if (columns.standard.testFlag(SC_UV_Index))
        samples.uvIndex.reserve(size);

    if (columns.standard.testFlag(SC_SolarRadiation))
        samples.solarRadiation.reserve(size);

    if (columns.standard.testFlag(SC_Evapotranspiration))
        samples.evapotranspiration.reserve(size);

    if (columns.standard.testFlag(SC_HighTemperature))
        samples.highTemperature.reserve(size);

    if (columns.standard.testFlag(SC_LowTemperature))
        samples.lowTemperature.reserve(size);

    if (columns.standard.testFlag(SC_HighRainRate))
        samples.highRainRate.reserve(size);

    if (columns.standard.testFlag(SC_HighSolarRadiation))
        samples.highSolarRadiation.reserve(size);

    if (columns.standard.testFlag(SC_HighUVIndex))
        samples.highUVIndex.reserve(size);

    if (columns.standard.testFlag(SC_Reception))
        samples.reception.reserve(size);

    if (columns.standard.testFlag(SC_ForecastRuleId))
        samples.forecastRuleId.reserve(size);

    if (columns.extra.testFlag(EC_LeafWetness1))
        samples.leafWetness1.reserve(size);

    if (columns.extra.testFlag(EC_LeafWetness2))
        samples.leafWetness2.reserve(size);

    if (columns.extra.testFlag(EC_LeafTemperature1))
        samples.leafTemperature1.reserve(size);

    if (columns.extra.testFlag(EC_LeafTemperature2))
        samples.leafTemperature2.reserve(size);

    if (columns.extra.testFlag(EC_SoilMoisture1))
        samples.soilMoisture1.reserve(size);

    if (columns.extra.testFlag(EC_SoilMoisture2))
        samples.soilMoisture2.reserve(size);

    if (columns.extra.testFlag(EC_SoilMoisture3))
        samples.soilMoisture3.reserve(size);

    if (columns.extra.testFlag(EC_SoilMoisture4))
        samples.soilMoisture4.reserve(size);

    if (columns.extra.testFlag(EC_SoilTemperature1))
        samples.soilTemperature1.reserve(size);

    if (columns.extra.testFlag(EC_SoilTemperature2))
        samples.soilTemperature2.reserve(size);

    if (columns.extra.testFlag(EC_SoilTemperature3))
        samples.soilTemperature3.reserve(size);

    if (columns.extra.testFlag(EC_SoilTemperature4))
        samples.soilTemperature4.reserve(size);

    if (columns.extra.testFlag(EC_ExtraHumidity1))
        samples.extraHumidity1.reserve(size);

    if (columns.extra.testFlag(EC_ExtraHumidity2))
        samples.extraHumidity2.reserve(size);

    if (columns.extra.testFlag(EC_ExtraTemperature1))
        samples.extraTemperature1.reserve(size);

    if (columns.extra.testFlag(EC_ExtraTemperature2))
        samples.extraTemperature2.reserve(size);

    if (columns.extra.testFlag(EC_ExtraTemperature3))
        samples.extraTemperature3.reserve(size);
}

void AppendNullSample(SampleSet &samples, SampleColumns columns, QDateTime time) {
    samples.timestampUnix.append(TO_UNIX_TIME(time));
    samples.sampleCount++;
    samples.timestamp.append(TO_UNIX_TIME(time));

    if (columns.standard.testFlag(SC_Temperature))
        samples.temperature.append(qQNaN());

    if (columns.standard.testFlag(SC_DewPoint))
        samples.dewPoint.append(qQNaN());

    if (columns.standard.testFlag(SC_ApparentTemperature))
        samples.apparentTemperature.append(qQNaN());

    if (columns.standard.testFlag(SC_WindChill))
        samples.windChill.append(qQNaN());

    if (columns.standard.testFlag(SC_IndoorTemperature))
        samples.indoorTemperature.append(qQNaN());

    if (columns.standard.testFlag(SC_Humidity))
        samples.humidity.append(qQNaN());

    if (columns.standard.testFlag(SC_IndoorHumidity))
        samples.indoorHumidity.append(qQNaN());

    if (columns.standard.testFlag(SC_Pressure))
        samples.pressure.append(qQNaN());

    if (columns.standard.testFlag(SC_MeanSeaLevelPressure))
        samples.pressure.append(qQNaN());

    if (columns.standard.testFlag(SC_Rainfall))
        samples.rainfall.append(qQNaN());

    if (columns.standard.testFlag(SC_AverageWindSpeed))
        samples.averageWindSpeed.append(qQNaN());

    if (columns.standard.testFlag(SC_GustWindSpeed))
        samples.gustWindSpeed.append(qQNaN());

    if (columns.standard.testFlag(SC_UV_Index))
        samples.uvIndex.append(qQNaN());

    if (columns.standard.testFlag(SC_SolarRadiation))
        samples.solarRadiation.append(qQNaN());

    if (columns.standard.testFlag(SC_Evapotranspiration))
        samples.evapotranspiration.append(qQNaN());

    if (columns.standard.testFlag(SC_HighTemperature))
        samples.highTemperature.append(qQNaN());

    if (columns.standard.testFlag(SC_LowTemperature))
        samples.lowTemperature.append(qQNaN());

    if (columns.standard.testFlag(SC_HighRainRate))
        samples.highRainRate.append(qQNaN());

    if (columns.standard.testFlag(SC_HighSolarRadiation))
        samples.highSolarRadiation.append(qQNaN());

    if (columns.standard.testFlag(SC_HighUVIndex))
        samples.highUVIndex.append(qQNaN());

    if (columns.standard.testFlag(SC_Reception))
        samples.reception.append(qQNaN());

    if (columns.standard.testFlag(SC_ForecastRuleId))
        samples.forecastRuleId.append(0);

    if (columns.extra.testFlag(EC_LeafWetness1))
        samples.leafWetness1.append(qQNaN());

    if (columns.extra.testFlag(EC_LeafWetness2))
        samples.leafWetness2.append(qQNaN());

    if (columns.extra.testFlag(EC_LeafTemperature1))
        samples.leafTemperature1.append(qQNaN());

    if (columns.extra.testFlag(EC_LeafTemperature2))
        samples.leafTemperature2.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilMoisture1))
        samples.soilMoisture1.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilMoisture2))
        samples.soilMoisture2.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilMoisture3))
        samples.soilMoisture3.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilMoisture4))
        samples.soilMoisture4.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilTemperature1))
        samples.soilTemperature1.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilTemperature2))
        samples.soilTemperature2.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilTemperature3))
        samples.soilTemperature3.append(qQNaN());

    if (columns.extra.testFlag(EC_SoilTemperature4))
        samples.soilTemperature4.append(qQNaN());

    if (columns.extra.testFlag(EC_ExtraHumidity1))
        samples.extraHumidity1.append(qQNaN());

    if (columns.extra.testFlag(EC_ExtraHumidity2))
        samples.extraHumidity2.append(qQNaN());

    if (columns.extra.testFlag(EC_ExtraTemperature1))
        samples.extraTemperature1.append(qQNaN());

    if (columns.extra.testFlag(EC_ExtraTemperature2))
        samples.extraTemperature2.append(qQNaN());

    if (columns.extra.testFlag(EC_ExtraTemperature3))
        samples.extraTemperature3.append(qQNaN());
}

void AppendNullSamples(SampleSet &samples, SampleColumns columns, QDateTime startTime,
                       QDateTime endTime, int intervalSeconds) {
    QDateTime ts = startTime;

    while (ts <= endTime) {
        AppendNullSample(samples, columns, ts);
        ts = ts.addSecs(intervalSeconds);
    }
}

QVector<double> SampleColumnInUnits(SampleSet samples, StandardColumn column, UnitConversions::unit_t units) {
    QVector<double> result;

    using namespace UnitConversions;

    unit_t columnUnits = SampleColumnUnits(column);

    if (columnUnits == U_CELSIUS) {
        if (column == SC_Temperature) result = samples.temperature;
        else if (column == SC_DewPoint) result = samples.dewPoint;
        else if (column == SC_ApparentTemperature) result = samples.apparentTemperature;
        else if (column == SC_WindChill) result = samples.windChill;
        else if (column == SC_IndoorTemperature) result = samples.indoorTemperature;
        else if (column == SC_HighTemperature) result = samples.highTemperature;
        else if (column == SC_LowTemperature) result = samples.lowTemperature;

        if (units == U_FAHRENHEIT) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = celsiusToFahrenheit(result[i]);
            }
        }
        // Else U_CELSIUS - nothing to do
    } else if (columnUnits == U_METERS_PER_SECOND) {
        if (column == SC_AverageWindSpeed) result = samples.averageWindSpeed;
        else if (column == SC_GustWindSpeed) result = samples.gustWindSpeed;

        if (units == U_KILOMETERS_PER_HOUR) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = metersPerSecondToKilometersPerHour(result[i]);
            }
        } else if (units == U_MILES_PER_HOUR) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = metersPerSecondToMilesPerHour(result[i]);
            }
        }
        // Else: U_BFT - not supported here
        // Else: U_METERS_PER_SECOND - nothing to do
    } else if (columnUnits == U_HECTOPASCALS) {
        result = samples.pressure;

        if (units == U_INCHES_OF_MERCURY) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = hectopascalsToInchesOfMercury(result[i]);
            }
        }
        // Else: U_HECTOPASCALS - nothing to do
    } else if (columnUnits == U_MILLIMETERS || columnUnits == U_MILLIMETERS_PER_HOUR) {
        if (column == SC_Rainfall) result = samples.rainfall;
        else if (column == SC_HighRainRate) result = samples.highRainRate;
        else if (column == SC_Evapotranspiration) result = samples.evapotranspiration;

        if (units == U_CENTIMETERS || units == U_CENTIMETERS_PER_HOUR) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = result[i] * 0.1;
            }
        } else if (units == U_INCHES || units == U_INCHES_PER_HOUR) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = millimetersToInches(result[i]);
            }
        }
        // Else: U_MILLIMETERS - nothing to do
    }

    return result;
}

QVector<double> SampleColumnInUnits(SampleSet samples, ExtraColumn column, UnitConversions::unit_t units) {
    QVector<double> result;

    using namespace UnitConversions;

    unit_t columnUnits = SampleColumnUnits(column);

    if (columnUnits == U_CELSIUS) {
        if (column == EC_LeafTemperature1) result = samples.leafTemperature1;
        else if (column == EC_LeafTemperature2) result = samples.leafTemperature2;
        else if (column == EC_SoilTemperature1) result = samples.soilTemperature1;
        else if (column == EC_SoilTemperature2) result = samples.soilTemperature2;
        else if (column == EC_SoilTemperature3) result = samples.soilTemperature3;
        else if (column == EC_SoilTemperature4) result = samples.soilTemperature4;
        else if (column == EC_ExtraTemperature1) result = samples.extraTemperature1;
        else if (column == EC_ExtraTemperature2) result = samples.extraTemperature2;
        else if (column == EC_ExtraTemperature3) result = samples.extraTemperature3;

        if (units == U_FAHRENHEIT) {
            for (int i = 0; i < result.count(); i++) {
                result[i] = celsiusToFahrenheit(result[i]);
            }
        }
        // Else U_CELSIUS - nothing to do
    } // else U_LEAF_WETNESS or U_CENTIBAR - nothing to do

    return result;
}
