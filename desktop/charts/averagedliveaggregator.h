#ifndef AVERAGEDLIVEAGGREGATOR_H
#define AVERAGEDLIVEAGGREGATOR_H

#include "abstractliveaggregator.h"

#include <QDateTime>
#include <QTimer>

class AveragedLiveAggregator : public AbstractLiveAggregator
{
    Q_OBJECT

public:
    AveragedLiveAggregator(int timespan, bool maxRainRate, bool runningTotalRain, QObject *parent=0);

public slots:
    void incomingLiveData(LiveDataSet data);
    void reset();
    void clear();

private:
    qint64 nextTs();
    qint64 currentTs;
    int timespan;

    QTimer timer;

    LiveDataSet makeLiveData(bool indoorDataAvailable, hardware_type_t hw_type);

    bool maxRainRate;
    bool runningTotalRain;

    int samples;

    float temperature;
    float indoorTemperature;
    float apparentTemperature;
    float windChill;
    float dewPoint;
    int humidity;
    int indoorHumidity;
    float pressure;
    float windSpeed;
    int windDirection;

    float consoleBatteryVoltage;
    float uvIndex;
    float solarRadiation;
    float rainRate;

    float leafWetness1;
    float leafWetness2;
    float leafTemperature1;
    float leafTemperature2;

    float soilMoisture1;
    float soilMoisture2;
    float soilMoisture3;
    float soilMoisture4;
    float soilTemperature1;
    float soilTemperature2;
    float soilTemperature3;
    float soilTemperature4;

    float extraHumidity1;
    float extraHumidity2;

    float extraTemperature1;
    float extraTemperature2;
    float extraTemperature3;


    // Storm rain is never averaged
    float stormRain;
    float lastStormRain;
};

#endif // AVERAGEDLIVEAGGREGATOR_H
