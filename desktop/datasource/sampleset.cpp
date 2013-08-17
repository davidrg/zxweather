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
}
