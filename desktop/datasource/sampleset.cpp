#include "sampleset.h"
#include <QtDebug>

void ReserveSampleSetSpace(SampleSet& samples, int size)
{
    qDebug() << "Reserving space for" << size << "samples.";
    samples.timestampUnix.reserve(size);
    samples.sampleCount = size;
    samples.timestamp.reserve(size);
    samples.temperature.reserve(size);
    samples.dewPoint.reserve(size);
    samples.apparentTemperature.reserve(size);
    samples.windChill.reserve(size);
    samples.indoorTemperature.reserve(size);
    samples.humidity.reserve(size);
    samples.indoorHumidity.reserve(size);
    samples.pressure.reserve(size);
    samples.rainfall.reserve(size);
}
