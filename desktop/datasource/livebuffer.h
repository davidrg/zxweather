#ifndef LIVEBUFFER_H
#define LIVEBUFFER_H

#include <QList>
#include <QObject>
#include "abstractlivedatasource.h"

/*
 * Would be nice to do this but given we'll be processing this every
 * 2.5 seconds we need a quick way of finding the next most recordy record
 * when current records are expired from the buffer.
 *
typedef struct _live_records {
    hardware_type_t hwType;
    bool indoorDataAvailable;

    float temperature;
    QDateTime temperatureTime;

    float indoorTemperature;
    QDateTime indoorTemperatureTime;

    float apparentTemperature;
    QDateTime apparentTemperatureTime;

    float windChill;
    QDateTime windChillTime;

    float dewPoint;
    QDateTime dewPointTime;

    int humidity;
    QDateTime humidityTime;

    int indoorHumidity;
    QDateTime indoorHumidityTime;

    float pressure;
    QDateTime pressureTime;

    float windSpeed;
    QDateTime windSpeedTime;

    int windDirection;
    QDateTime windDirectionTime;

    // Davis specific fields
    float rainRate;
    QDateTime rainRateTime;

    float uvIndex;
    QDateTime uvIndexTime;

    float solarRadiation;
    QDateTime solarRadiationTime;
} LiveRecords;
*/

class LiveBuffer: public QObject
{
    Q_OBJECT

public:
    static LiveBuffer& getInstance() {
        static LiveBuffer instance;
        return instance;
    }

    QList<LiveDataSet> getData();

public slots:
    void liveData(LiveDataSet data);
    void connectStation(QString station);

private:
    LiveBuffer();
    ~LiveBuffer();

    LiveBuffer(LiveBuffer const&); /* Not implemented. Don't use it. */
    void operator=(LiveBuffer const&); /* Not implemented. Don't use it. */

    QString stationCode;

    QList<LiveDataSet> buffer;

    int maxHours;

    void trimBuffer();

    QDateTime lastFileWriteTime;
    QDateTime lastSaveTime;
    QDateTime lastFileOverwriteTime;
    QString encodeLiveDataSet(LiveDataSet lds);
    LiveDataSet decodeLiveDataSet(QString row);
    void save(bool appendOnly=false);
    void load();
    QString bufferFilename();
};

#endif // LIVEBUFFER_H
