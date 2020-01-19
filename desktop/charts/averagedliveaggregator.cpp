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
    leafWetness1 = 0;
    leafWetness2 = 0;
    leafTemperature1 = 0;
    leafTemperature2 = 0;
    soilMoisture1 = 0;
    soilMoisture2 = 0;
    soilMoisture3 = 0;
    soilMoisture4 = 0;
    soilTemperature1 = 0;
    soilTemperature2 = 0;
    soilTemperature3 = 0;
    soilTemperature4 = 0;
    extraHumidity1 = 0;
    extraHumidity2 = 0;
    extraTemperature1 = 0;
    extraTemperature2 = 0;
    extraTemperature3 = 0;
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
        lds.davisHw.leafWetness1 = leafWetness1 / samples;
        lds.davisHw.leafWetness2 = leafWetness2 / samples;
        lds.davisHw.leafTemperature1 = leafTemperature1 / samples;
        lds.davisHw.leafTemperature2 = leafTemperature2 / samples;
        lds.davisHw.soilMoisture1 = soilMoisture1 / samples;
        lds.davisHw.soilMoisture2 = soilMoisture2 / samples;
        lds.davisHw.soilMoisture3 = soilMoisture3 / samples;
        lds.davisHw.soilMoisture4 = soilMoisture4 / samples;
        lds.davisHw.soilTemperature1 = soilTemperature1 / samples;
        lds.davisHw.soilTemperature2 = soilTemperature2 / samples;
        lds.davisHw.soilTemperature3 = soilTemperature3 / samples;
        lds.davisHw.soilTemperature4 = soilTemperature4 / samples;
        lds.davisHw.extraHumidity1 = extraHumidity1 / samples;
        lds.davisHw.extraHumidity2 = extraHumidity2 / samples;
        lds.davisHw.extraTemperature1 = extraTemperature1 / samples;
        lds.davisHw.extraTemperature2 = extraTemperature2 / samples;
        lds.davisHw.extraTemperature3 = extraTemperature3 / samples;
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
        leafWetness1 += data.davisHw.leafWetness1;
        leafWetness2 += data.davisHw.leafWetness2;
        leafTemperature1 += data.davisHw.leafTemperature1;
        leafTemperature2 += data.davisHw.leafTemperature2;
        soilMoisture1 += data.davisHw.soilMoisture1;
        soilMoisture2 += data.davisHw.soilMoisture2;
        soilMoisture3 += data.davisHw.soilMoisture3;
        soilMoisture4 += data.davisHw.soilMoisture4;
        soilTemperature1 += data.davisHw.soilTemperature1;
        soilTemperature2 += data.davisHw.soilTemperature2;
        soilTemperature3 += data.davisHw.soilTemperature3;
        soilTemperature4 += data.davisHw.soilTemperature4;
        extraHumidity1 += data.davisHw.extraHumidity1;
        extraHumidity2 += data.davisHw.extraHumidity2;
        extraTemperature1 += data.davisHw.extraTemperature1;
        extraTemperature2 += data.davisHw.extraTemperature2;
        extraTemperature3 += data.davisHw.extraTemperature3;

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
