#ifndef ABSTRACTLIVEDATASOURCE_H
#define ABSTRACTLIVEDATASOURCE_H

#include <QObject>
#include <QDateTime>
#include <QDate>

enum hardware_type_t {
    HW_GENERIC,
    HW_FINE_OFFSET,
    HW_DAVIS
};

struct _DavisLive {
    float stormRain;
    float rainRate;
    QDate stormStartDate;
    bool stormDateValid;
    int barometerTrend;
    int forecastIcon;
    int forecastRule;
    int txBatteryStatus;
    float consoleBatteryVoltage;
};

typedef struct _liveData {
    float temperature;
    float indoorTemperature;
    float apparentTemperature;
    float windChill;
    float dewPoint;

    int humidity;
    int indoorHumidity;

    float pressure;

    float windSpeed;
    float gustWindSpeed; // TODO: Remove me
    int windDirection;

    QDateTime timestamp;

    bool indoorDataAvailable;

    hardware_type_t hw_type;

    struct _DavisLive davisHw;
} LiveDataSet;

/** This is an interface for all data sources that can provide live data.
 * Data sources that can also provide samples should inherit from
 * AbstractDataSource instead.
 */
class AbstractLiveDataSource : public QObject
{
    Q_OBJECT

public:
    explicit AbstractLiveDataSource(QObject* parent=0): QObject(parent) {}

    /** Enables the live data feed. The data source will not supply any live
     * data until this is called.
     */
    virtual void enableLiveData() = 0;


    /** Gets the type of weather station hardware currently in use.
     *
     * @return Station type.
     */
    virtual hardware_type_t getHardwareType() = 0;
signals:
    /** Emitted when ever live data is available.
     *
     * @param data The live data.
     */
    void liveData(LiveDataSet data);

    /** Emitted when ever an error occurs.
     *
     * @param errMsg The error message.
     */
    void error(QString errMsg);
};

#endif // ABSTRACTLIVEDATASOURCE_H
