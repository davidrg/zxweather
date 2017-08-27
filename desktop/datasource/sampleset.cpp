#include "sampleset.h"
#include <QtDebug>

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
