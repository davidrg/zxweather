#ifndef ABSTRACTLIVEDATASOURCE_H
#define ABSTRACTLIVEDATASOURCE_H

#include <QObject>
#include <QDateTime>
#include <QDate>

#include "sampleset.h"

enum hardware_type_t {
    HW_GENERIC = 0,
    HW_FINE_OFFSET = 1,
    HW_DAVIS = 2
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
    float uvIndex;
    float solarRadiation;
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
    int windDirection;

    QDateTime timestamp;

    bool indoorDataAvailable;

    hardware_type_t hw_type;

    struct _DavisLive davisHw;
} LiveDataSet;

typedef struct _newImageInfo {
    QString imageSourceCode;
    QString stationCode;
    QDateTime timestamp;
    int imageId;
} NewImageInfo;

enum LiveValue {
    LV_NoColumns                = 0x00000000,
    LV_Temperature              = 0x00000001,
    LV_IndoorTemperature        = 0x00000002,
    LV_ApparentTemperature      = 0x00000004,
    LV_WindChill                = 0x00000008,
    LV_DewPoint                 = 0x00000010,
    LV_Humidity                 = 0x00000020,
    LV_IndoorHumidity           = 0x00000040,
    LV_Pressure                 = 0x00000080,
    LV_WindSpeed                = 0x00000100,
    LV_WindDirection            = 0x00000200,
    LV_StormRain                = 0x00000400,
    LV_RainRate                 = 0x00000800,
    LV_BatteryVoltage           = 0x00001000,
    LV_UVIndex                  = 0x00002000,
    LV_SolarRadiation           = 0x00004000

};
Q_DECLARE_FLAGS(LiveValues, LiveValue)

#define ALL_LIVE_COLUMNS (LV_Temperature | LV_IndoorTemperature | \
    LV_ApparentTemperature | LV_WindChill | LV_DewPoint | LV_Humidity | \
    LV_IndoorHumidity | LV_Pressure | LV_WindSpeed | LV_WindDirection | \
    LV_StormRain | LV_RainRate | LV_BatteryVoltage | LV_UVIndex | \
    LV_SolarRadiation)

#define LIVE_TEMPERATURE_COLUMNS (LV_Temperature | LV_IndoorTemperature | \
    LV_ApparentTemperature | LV_WindChill | LV_DewPoint)

#define LIVE_HUMIDITY_COLUMNS (LV_Humidity | LV_IndoorHumidity)

#define LIVE_WIND_COLUMNS (LV_WindDirection | LV_WindSpeed)

#define LIVE_SOLAR_COLUMNS (LV_SolarRadiation | LV_UVIndex)

#define LIVE_RAIN_COLUMNS (LV_StormRain | LV_RainRate)

#define LIVE_OTHER_COLUNS (LV_BatteryVoltage | LV_Pressure)

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

    /** Emitted before the first live data record to indicate if Solar
     * and UV data are available.
     *
     * @param enabled If UV and Solar Radiation are enabled for this station
     */
    void isSolarDataEnabled(bool enabled);

    /** Emitted when the data source obtains the stations actual name.
     *
     * @param name The name of the station
     */
    void stationName(QString name);

    /** Emitted when a new image is available
     */
    void newImage(NewImageInfo imageInfo);

    /** Emitted when a new sample is available
     */
    void newSample(Sample sample);
};

#endif // ABSTRACTLIVEDATASOURCE_H
