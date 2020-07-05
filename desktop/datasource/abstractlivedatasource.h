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

    // Optional sensor transmitters:
    double leafWetness1;
    double leafWetness2;
    double leafTemperature1;
    double leafTemperature2;
    double soilMoisture1;
    double soilMoisture2;
    double soilMoisture3;
    double soilMoisture4;
    double soilTemperature1;
    double soilTemperature2;
    double soilTemperature3;
    double soilTemperature4;
    double extraTemperature1;
    double extraTemperature2;
    double extraTemperature3;
    double extraHumidity1;
    double extraHumidity2;
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
    LV_StormRain                = 0x00000400,   // Vantage Vue/Pro2
    LV_RainRate                 = 0x00000800,   // Vantage Vue/Pro2
    LV_BatteryVoltage           = 0x00001000,   // Vantage Vue/Pro2
    LV_UVIndex                  = 0x00002000,   // Vantage Pro2+
    LV_SolarRadiation           = 0x00004000,   // Vantage Pro2+

    // Vantage Pro2 extra sensor stations
    LV_LeafWetness1             = 0x00008000,  // Leaf+Soil or Leaf Station
    LV_LeafWetness2             = 0x00010000,  // Leaf+Soil or Leaf Station
    LV_LeafTemperature1         = 0x00020000,  // Leaf+Soil or Leaf Station
    LV_LeafTemperature2         = 0x00040000,   // Leaf+Soil or Leaf Station
    LV_SoilMoisture1            = 0x00080000,   // Leaf+Soil or Soil Station
    LV_SoilMoisture2            = 0x00100000,   // Leaf+Soil or Soil Station
    LV_SoilMoisture3            = 0x00200000,   // Leaf+Soil or Soil Station
    LV_SoilMoisture4            = 0x00400000,   // Leaf+Soil or Soil Station
    LV_SoilTemperature1         = 0x00800000,   // Leaf+Soil or Soil Station
    LV_SoilTemperature2         = 0x01000000,   // Leaf+Soil or Soil Station
    LV_SoilTemperature3         = 0x02000000,   // Leaf+Soil or Soil Station
    LV_SoilTemperature4         = 0x04000000,   // Leaf+Soil or Soil Station
    LV_ExtraTemperature1        = 0x08000000,   // Temperature-Humidity Station
    LV_ExtraTemperature2        = 0x10000000,   // Temperature-Humidity Station
    LV_ExtraTemperature3        = 0x20000000,   // Temperature Station
    LV_ExtraHumidity1           = 0x40000000,   // Temperature-Humidity Station
    LV_ExtraHumidity2           = 0x80000000    // Temperature-Humidity Station

};
Q_DECLARE_FLAGS(LiveValues, LiveValue)

#define LIVE_LEAF_COLUMNS (LV_LeafWetness1 | LV_LeafWetness2 | LV_LeafTemperature1 | LV_LeafTemperature2)
#define LIVE_SOIL_COLUMNS (LV_SoilMoisture1 | LV_SoilMoisture2 | LV_SoilMoisture3 | LV_SoilMoisture4 | \
    LV_SoilTemperature1 | LV_SoilTemperature2 | LV_SoilTemperature3 | LV_SoilTemperature4 )
#define LIVE_EXTRA_TEMP_HUM_COLUMNS (LV_ExtraTemperature1 | LV_ExtraTemperature2 | LV_ExtraTemperature3 | \
    LV_ExtraHumidity1 | LV_ExtraHumidity2)
#define LIVE_TEMPERATURE_COLUMNS (LV_Temperature | LV_IndoorTemperature | \
    LV_ApparentTemperature | LV_WindChill | LV_DewPoint)
#define LIVE_HUMIDITY_COLUMNS (LV_Humidity | LV_IndoorHumidity)
#define LIVE_WIND_COLUMNS (LV_WindDirection | LV_WindSpeed)
#define LIVE_SOLAR_COLUMNS (LV_SolarRadiation | LV_UVIndex)
#define LIVE_RAIN_COLUMNS (LV_StormRain | LV_RainRate)
#define LIVE_OTHER_COLUNS (LV_BatteryVoltage | LV_Pressure)

#define ALL_LIVE_COLUMNS (LIVE_TEMPERATURE_COLUMNS | LIVE_HUMIDITY_COLUMNS | LIVE_WIND_COLUMNS | \
    LIVE_RAIN_COLUMNS | LIVE_OTHER_COLUNS | LIVE_SOLAR_COLUMNS | LIVE_LEAF_COLUMNS | \
    LIVE_SOIL_COLUMNS | LIVE_EXTRA_TEMP_HUM_COLUMNS)

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
    virtual void disableLiveData() = 0;


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

    void liveConnectFailed(QString errMsg);
    void liveConnected();

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
