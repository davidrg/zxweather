#include "sampleset.h"
#include <QtDebug>

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

void ReserveSampleSetSpace(SampleSet& samples, int size, SampleColumns columns)
{
    qDebug() << "Reserving space for" << size << "samples.";
    samples.timestampUnix.reserve(size);
    samples.sampleCount = size;
    samples.timestamp.reserve(size);

    if (columns.testFlag(SC_Temperature))
        samples.temperature.reserve(size);

    if (columns.testFlag(SC_DewPoint))
        samples.dewPoint.reserve(size);

    if (columns.testFlag(SC_ApparentTemperature))
        samples.apparentTemperature.reserve(size);

    if (columns.testFlag(SC_WindChill))
        samples.windChill.reserve(size);

    if (columns.testFlag(SC_IndoorTemperature))
        samples.indoorTemperature.reserve(size);

    if (columns.testFlag(SC_Humidity))
        samples.humidity.reserve(size);

    if (columns.testFlag(SC_IndoorHumidity))
        samples.indoorHumidity.reserve(size);

    if (columns.testFlag(SC_Pressure))
        samples.pressure.reserve(size);

    if (columns.testFlag(SC_Rainfall))
        samples.rainfall.reserve(size);

    if (columns.testFlag(SC_AverageWindSpeed))
        samples.averageWindSpeed.reserve(size);

    if (columns.testFlag(SC_GustWindSpeed))
        samples.gustWindSpeed.reserve(size);

    if (columns.testFlag(SC_UV_Index))
        samples.uvIndex.reserve(size);

    if (columns.testFlag(SC_SolarRadiation))
        samples.solarRadiation.reserve(size);

    if (columns.testFlag(SC_Evapotranspiration))
        samples.evapotranspiration.reserve(size);

    if (columns.testFlag(SC_HighTemperature))
        samples.highTemperature.reserve(size);

    if (columns.testFlag(SC_LowTemperature))
        samples.lowTemperature.reserve(size);

    if (columns.testFlag(SC_HighRainRate))
        samples.highRainRate.reserve(size);

    if (columns.testFlag(SC_HighSolarRadiation))
        samples.highSolarRadiation.reserve(size);

    if (columns.testFlag(SC_HighUVIndex))
        samples.highUVIndex.reserve(size);

    if (columns.testFlag(SC_Reception))
        samples.reception.reserve(size);

    if (columns.testFlag(SC_ForecastRuleId))
        samples.forecastRuleId.reserve(size);
}

void AppendNullSample(SampleSet &samples, SampleColumns columns, QDateTime time) {
    samples.timestampUnix.append(time.toTime_t());
    samples.sampleCount++;
    samples.timestamp.append(time.toTime_t());

    if (columns.testFlag(SC_Temperature))
        samples.temperature.append(qQNaN());

    if (columns.testFlag(SC_DewPoint))
        samples.dewPoint.append(qQNaN());

    if (columns.testFlag(SC_ApparentTemperature))
        samples.apparentTemperature.append(qQNaN());

    if (columns.testFlag(SC_WindChill))
        samples.windChill.append(qQNaN());

    if (columns.testFlag(SC_IndoorTemperature))
        samples.indoorTemperature.append(qQNaN());

    if (columns.testFlag(SC_Humidity))
        samples.humidity.append(qQNaN());

    if (columns.testFlag(SC_IndoorHumidity))
        samples.indoorHumidity.append(qQNaN());

    if (columns.testFlag(SC_Pressure))
        samples.pressure.append(qQNaN());

    if (columns.testFlag(SC_Rainfall))
        samples.rainfall.append(qQNaN());

    if (columns.testFlag(SC_AverageWindSpeed))
        samples.averageWindSpeed.append(qQNaN());

    if (columns.testFlag(SC_GustWindSpeed))
        samples.gustWindSpeed.append(qQNaN());

    if (columns.testFlag(SC_UV_Index))
        samples.uvIndex.append(qQNaN());

    if (columns.testFlag(SC_SolarRadiation))
        samples.solarRadiation.append(qQNaN());

    if (columns.testFlag(SC_Evapotranspiration))
        samples.evapotranspiration.append(qQNaN());

    if (columns.testFlag(SC_HighTemperature))
        samples.highTemperature.append(qQNaN());

    if (columns.testFlag(SC_LowTemperature))
        samples.lowTemperature.append(qQNaN());

    if (columns.testFlag(SC_HighRainRate))
        samples.highRainRate.append(qQNaN());

    if (columns.testFlag(SC_HighSolarRadiation))
        samples.highSolarRadiation.append(qQNaN());

    if (columns.testFlag(SC_HighUVIndex))
        samples.highUVIndex.append(qQNaN());

    if (columns.testFlag(SC_Reception))
        samples.reception.append(qQNaN());

    if (columns.testFlag(SC_ForecastRuleId))
        samples.forecastRuleId.append(0);
}

void AppendNullSamples(SampleSet &samples, SampleColumns columns, QDateTime startTime,
                       QDateTime endTime, int intervalSeconds) {
    QDateTime ts = startTime;

    while (ts <= endTime) {
        AppendNullSample(samples, columns, ts);
        ts = ts.addSecs(intervalSeconds);
    }
}

UnitConversions::unit_t SampleColumnUnits(SampleColumn column) {
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

QVector<double> SampleColumnInUnits(SampleSet samples, SampleColumn column, UnitConversions::unit_t units) {
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
