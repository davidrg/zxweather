#include "averagedliveaggregator.h"

#include <QtDebug>

AveragedLiveAggregator::AveragedLiveAggregator(int timespan, bool maxRainRate,
                                               bool runningTotalRain,
                                               QObject *parent)
    :AbstractLiveAggregator(parent)
{
    this->maxRainRate = maxRainRate;
    this->runningTotalRain = runningTotalRain;
    this->timespan = timespan;

    reset();
}

void AveragedLiveAggregator::reset() {
    currentTs = 0;
    clear();
    samples = -1;
}

void AveragedLiveAggregator::clear() {
    samples = 0;
    temperature = 0;
    indoorTemperature = 0;
    apparentTemperature = 0;
    windChill = 0;
    dewPoint = 0;
    humidity = 0;
    indoorHumidity = 0;
    pressure = 0;
    windSpeed = 0;
    windDirection = 0;
    consoleBatteryVoltage = 0;
    uvIndex = 0;
    solarRadiation = 0;
    rainRate = 0;
    stormRain = 0;
    lastStormRain = 0;
}

qint64 AveragedLiveAggregator::nextTs() {
    return currentTs + (timespan * 1000);
}

LiveDataSet AveragedLiveAggregator::makeLiveData(bool indoorDataAvailable, hardware_type_t hw_type) {
    LiveDataSet lds;
    lds.temperature = temperature / samples;
    lds.indoorTemperature = indoorTemperature / samples;
    lds.apparentTemperature = apparentTemperature / samples;
    lds.windChill = windChill / samples;
    lds.dewPoint = dewPoint / samples;
    lds.humidity = humidity / samples;
    lds.indoorHumidity = indoorHumidity / samples;
    lds.pressure = pressure / samples;
    lds.windSpeed = windSpeed / samples;
    lds.windDirection = windDirection / samples;
    lds.timestamp = QDateTime::fromMSecsSinceEpoch(currentTs);
    lds.indoorDataAvailable = indoorDataAvailable;
    lds.hw_type = hw_type;

    if (hw_type == HW_DAVIS) {
        lds.davisHw.stormRain = stormRain;

        if (maxRainRate) {
            lds.davisHw.rainRate = rainRate;
        } else {
            lds.davisHw.rainRate = rainRate / samples;
        }

        lds.davisHw.consoleBatteryVoltage = consoleBatteryVoltage / samples;
        lds.davisHw.uvIndex = uvIndex / samples;
        lds.davisHw.solarRadiation = solarRadiation / samples;
    }

    return lds;
}

void AveragedLiveAggregator::incomingLiveData(LiveDataSet data) {
    qDebug() << "Adding live data to the collective" << data.timestamp;
    if (currentTs == 0) {
        qDebug() << "adopting current sample TS at" << data.timestamp;
        currentTs = data.timestamp.toMSecsSinceEpoch();
    }

    if (samples == -1) {
        // Put the first point in the plot
        emit liveData(data);
        samples = 0;
    }

    qint64 ts = nextTs();

    qDebug() << "This TS" << data.timestamp.toMSecsSinceEpoch()
             << "Next TS" << ts;

    if (ts < data.timestamp.toMSecsSinceEpoch()) {
        qDebug() << "Next TS is in the past!";

        // Time to average it.
        emit liveData(makeLiveData(data.indoorDataAvailable, data.hw_type));

        clear();

        // Then reset everything for the next time period.
        currentTs = ts;
        qDebug() << "New TS:" << QDateTime::fromMSecsSinceEpoch(ts);
    }

    samples++;

    temperature += data.temperature;
    indoorTemperature += data.indoorTemperature;
    apparentTemperature += data.apparentTemperature;
    windChill += data.windChill;
    dewPoint += data.dewPoint;
    humidity += data.humidity;
    indoorHumidity += data.indoorHumidity;
    pressure += data.pressure;
    windSpeed += data.windSpeed;
    windDirection += data.windDirection;

    if (data.hw_type == HW_DAVIS) {
        consoleBatteryVoltage += data.davisHw.consoleBatteryVoltage;
        uvIndex += data.davisHw.uvIndex;
        solarRadiation += data.davisHw.solarRadiation;

        if (maxRainRate) {
            if(rainRate < data.davisHw.rainRate) {
                rainRate = data.davisHw.rainRate;
            } else {
                rainRate += data.davisHw.rainRate;
            }
        }

        if (runningTotalRain) {
            stormRain = data.davisHw.stormRain;
        } else {
            stormRain = data.davisHw.stormRain - lastStormRain;
        }
    }

}
