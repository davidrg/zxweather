/*****************************************************************************
 *            Created: 23/09/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QColor>
#include <QStringList>
#include <QVariantMap>
#include <QFont>

class QSettings;

typedef struct _chart_colours {
    QColor temperature;
    QColor highTemperature;
    QColor lowTemperature;
    QColor indoorTemperature;
    QColor apparentTemperature;
    QColor windChill;
    QColor dewPoint;

    QColor humidity;
    QColor indoorHumidity;

    QColor pressure;

    QColor rainfall;
    QColor rainRate;

    QColor averageWindSpeed;
    QColor gustWindSpeed;
    QColor windDirection;
    QColor gustWindDirection;

    QColor uvIndex;
    QColor highUVIndex;
    QColor solarRadiation;
    QColor highSolarRadiation;

    QColor evapotranspiration;
    QColor reception;
    QColor consoleBatteryVoltage; // live only

    QColor leafWetness1;
    QColor leafWetness2;
    QColor leafTemperature1;
    QColor leafTemperature2;

    QColor soilMoisture1;
    QColor soilMoisture2;
    QColor soilMoisture3;
    QColor soilMoisture4;
    QColor soilTemperature1;
    QColor soilTemperature2;
    QColor soilTemperature3;
    QColor soilTemperature4;

    QColor extraHumidity1;
    QColor extraHumidity2;

    QColor extraTemperature1;
    QColor extraTemperature2;
    QColor extraTemperature3;


    QColor title;
    QColor background;
} ChartColours;

/** Provides access to application settings. This is a singleton. Call
 * getInstance() to get a reference to the single instance.
 */
class Settings : public QObject
{
    Q_OBJECT
public:
    static Settings& getInstance() {
        static Settings instance;
        return instance;
    }

    void setConfigFile(const QString filename);

    enum data_source_type_t {DS_TYPE_DATABASE, DS_TYPE_WEB_INTERFACE, DS_TYPE_SERVER};
    
    /* General Settings */
    void setMinimiseToSysTray(bool enabled);
    bool miniseToSysTray();

    void setCloseToSysTray(bool enabled);
    bool closeToSysTray();

    /* Data Source */
    void setLiveDataSourceType(Settings::data_source_type_t type);
    Settings::data_source_type_t liveDataSourceType();

    void setSampleDataSourceType(Settings::data_source_type_t type);
    Settings::data_source_type_t sampleDataSourceType();

    void setWebInterfaceUrl(QString webInterfaceUrl);
    QString webInterfaceUrl();

    void setDatabaseName(QString dbName);
    QString databaseName();

    void setDatabaseHostname(QString hostName);
    QString databaseHostName();

    void setDatabasePort(int port);
    int databasePort();

    void setDatabaseUsername(QString username);
    QString databaseUsername();

    void setDatabasePassword(QString password);
    QString databasePassword();

    void setServerHostname(QString hostname);
    QString serverHostname();

    void setServerPort(int port);
    int serverPort();

    void setStationCode(QString name);
    QString stationCode();

    void setChartColours(ChartColours colours);
    ChartColours getChartColours();

    /* Single shot stuff */
    void setSingleShotMinimiseToSysTray();
    bool singleShotMinimiseToSysTray();

    void setSingleShotCloseToSysTray();
    bool singleShotCloseToSysTray();

    void setSingleShotFirstRun();
    bool singleShotFirstRun();

    /* Live timeout stuff */
    void setLiveTimeoutEnabled(bool enabled);
    bool liveTimeoutEnabled();

    void setLiveTimeoutInterval(uint interval);
    uint liveTimeoutInterval();

    /* Images window */
    void setImagesWindowHSplitterLayout(QByteArray data);
    QByteArray getImagesWindowHSplitterLayout();

    void setImagesWindowVSplitterLayout(QByteArray data);
    QByteArray getImagesWindowVSplitterLayout();

    void setImagesWindowLayout(QByteArray data);
    QByteArray getImagesWindowLayout();

    void saveImagesWindowGeometry(QByteArray geom);
    QByteArray imagesWindowGeometry() const;

    void setImagesWindowViewMode(int viewMode);
    int imagesWindowViewMode();

    void setImagesWindowNavigationPaneVisible(bool visible);
    bool imagesWindowNavigationPaneVisible();

    void setImagesWindowPreviewPaneVisible(bool visible);
    bool imagesWindowPreviewPaneVisible();

    QStringList imageTypeSortOrder();

    /** Sets the units used for display through-out the application.
     *
     * @param imperial If Imperial units should be used instead of metric
     * @param kmh When using metric, if kilometers per hour should be used for
     *            wind speed instead of meters per second.
     */
    void setUnits(bool imperial, bool kmh);

    /** If units should be imperial / US Customary intead of metric
     */
    bool imperial() const;

    /** When using Metric units, if wind speed should be displayed
     * in kilometers per hour (km/h) by default instead of meters
     * per sceond (m/s)
     */
    bool kmh() const;

    QVariant weatherValueWidgetSetting(QString name, QString setting, QVariant defaultValue);
    void setWeatherValueWidgetSetting(QString name, QString setting, QVariant value);

    // These are the last-used settings for the live chart window.
    int liveAggregateSeconds() const;
    int liveTimespanMinutes() const;
    bool liveAggregate() const;
    bool liveMaxRainRate() const;
    bool liveStormRain() const;
    bool liveTagsEnabled() const;
    bool liveMultipleAxisRectsEnabled() const;

    void setLiveAggregateSeconds(int value);
    void setLiveTimespanMinutes(int value);
    void setLiveAggregate(bool value);
    void setLiveMaxRainRate(bool value);
    void setLiveStormRain(bool value);
    void setLiveTagsEnabled(bool value);
    void setLiveMultipleAxisRectsEnabled(bool value);

    void saveMainWindowState(QByteArray state);
    QByteArray mainWindowState() const;

    void saveMainWindowGeometry(QByteArray geom);
    QByteArray mainWindowGeometry() const;

    void saveChartWindowState(QByteArray state);
    QByteArray chartWindowState() const;

    void saveChartWindowGeometry(QByteArray geom);
    QByteArray chartWindowGeometry() const;

    void saveReportCriteria(QString report, QVariantMap criteria);
    QVariantMap getReportCriteria(QString report);

    bool chartCursorEnabled();
    void setChartCursorEnabled(bool enabled);

    bool showCurrentDayInImageWindow();
    bool selectCurrentDayInImageWindow();

    QStringList reportSearchPath() const;

    int liveBufferHours() const;

    void setDefaultChartTitleFont(QFont font);
    QFont defaultChartTitleFont() const;

    void setDefaultChartAxisTickLabelFont(QFont font);
    QFont defaultChartAxisTickLabelFont() const;

    void setDefaultChartAxisLabelFont(QFont font);
    QFont defaultChartAxisLabelFont() const;

    void setDefaultChartLegendFont(QFont font);
    QFont defaultChartLegendFont() const;

    void resetFontsToDefaults() const;

    void overrideStationCode(const QString &stationCode);
    bool isStationCodeOverridden() const;

    void temporarilyAddReportSearchPath(const QString path);
    void removeReportSearchPath(const QString path);

signals:
    void unitsChanged(bool imperial, bool kmh);

private:
    Settings();
    ~Settings();

    Settings(Settings const&); /* Not implemented. Don't use it. */
    void operator=(Settings const&); /* Not implemented. Don't use it. */

    QSettings *settings;

    QStringList imageTypePriority;

    QString stationCodeOverride;

    QStringList extraReportSearchPaths;
    QStringList blacklistReportSearchPaths;

    ChartColours defaultChartColours;
};

#endif // SETTINGS_H
