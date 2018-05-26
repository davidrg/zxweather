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
