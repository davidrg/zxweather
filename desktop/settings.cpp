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

#include "settings.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QDebug>

/** Settings keys.
 */
namespace SettingsKey {
    /** General settings (minimise to system tray, etc).
     */
    namespace General {
        const QString MINIMISE_TO_SYSTRAY = "General/minimise_to_systray";
        const QString CLOSE_TO_SYSTRAY = "General/close_to_systray";

        const QString IMPERIAL = "General/imperial";

        const QString MAIN_WINDOW_STATE ="General/mw_state";
        const QString MAIN_WINDOW_GEOMETRY = "General/mw_geom";

        namespace LiveMon {
            const QString ENABLED = "General/live_mon/enabled";
            const QString INTERVAL = "General/live_mon/interval";
        }

        namespace ImagesWindow {
            // This is the state of the two splitter layouts
            const QString HLAYOUT = "General/images_window/horizontal_layout";
            const QString VLAYOUT = "General/images_window/vertical_layout";
            const QString WLAYOUT = "General/images_window/window_layout";
            const QString TYPE_SORT = "General/images_window/image_type_sort_order";
        }
    }

    namespace LiveChart {
        const QString AGGERGATE  = "LiveChart/aggregate";
        const QString AGGREGATE_SECONDS = "LiveChart/aggregate_seconds";
        const QString MAX_RAIN_RATE = "LiveChart/max_rain_rate";
        const QString STORM_RAIN = "LiveChart/storm_rain";
        const QString TIMESPAN_MINUTES = "LiveChart/timespan_minutes";
        const QString LIVE_TAGS = "LiveChart/live_tags";
        const QString MULTI_RECT = "LiveChart/multiple_axis_rects";
    }

    namespace WeatherValueWidgets {
        const QString ROOT = "WeatherValueWidget";
    }

    /** Settings about where to get data from.
     */
    namespace DataSource {
        const QString LIVE_TYPE = "DataSource/type";
        const QString SAMPLE_TYPE = "DataSource/sample_type";
        const QString URL = "DataSource/url";
        const QString STATION_NAME = "DataSource/station_code";
        namespace Database {
            const QString NAME = "DataSource/Database/name";
            const QString HOST_NAME = "DataSource/Database/hostname";
            const QString PORT = "DataSource/Database/port";
            const QString USERNAME = "DataSource/Database/username";
            const QString PASSWORD = "DataSource/Database/password";

            /* This is where v0.2 stored it. As it is used for the web
             * interface and server data source types as well it was moved
             * directly under DataSource in zxweather v1.0.
             */
            const QString STATION_NAME_LEGACY = "DataSource/Database/station";
        }

        namespace Server {
            const QString HOST_NAME = "DataSource/Server/hostname";
            const QString PORT = "DataSource/Server/port";
        }
    }

    namespace Colours {
        namespace Charts {
            const QString TEMPERATURE = "Colours/Charts/temperature";
            const QString INDOOR_TEMPERATURE = "Colours/Charts/indoor_temperature";
            const QString APPARENT_TEMPERATURE = "Colours/Charts/apparent_temperature";
            const QString WIND_CHILL = "Colours/Charts/wind_chill";
            const QString DEW_POINT = "Colours/Charts/dew_point";
            const QString HUMIDITY = "Colours/Charts/humidity";
            const QString INDOOR_HUMIDITY = "Colours/Charts/indoor_humidity";
            const QString PRESSURE = "Colours/Charts/pressure";
            const QString RAINFALL = "Colours/Charts/rainfall";
            const QString RAINRATE = "Colours/charts/rainrate";
            const QString AVG_WIND_SPEED = "Colours/Charts/average_wind_speed";
            const QString GUST_WIND_SPEED = "Colours/Charts/gust_wind_speed";
            const QString WIND_DIRECTION = "Colours/Charts/wind_direction";
            const QString UV_INDEX = "Colours/Charts/uv_index";
            const QString SOLAR_RADIATION = "Colours/Charts/solar_radiation";
            const QString EVAPOTRANSPIRATION = "Colours/Charts/evapotranspiration";
            const QString RECEPTION = "Colours/Charts/reception";
            const QString CONSOLE_BATTERY_VOLTAGE = "Colours/Charts/console_battery_voltage";
            const QString TITLE = "Colours/Charts/title";
            const QString BACKGROUND = "Colours/Charts/background";
        }
    }

    /** For tracking what single-shot events have happened (for example,
     * showing a message to the user the first time the program minimises
     * to the system tray.
     */
    namespace SingleShot {
        const QString MINIMISE_TO_SYS_TRAY_INFO = "SingleShot/minimise_to_systray_info";
        const QString CLOSE_TO_SYS_TRAY_INFO = "SingleShot/close_to_systray_info";
        const QString FIRST_RUN = "SingleShot/first_run";
    }

    /** Where zxweather desktop v0.1 stored database settings. Versions 0.2
     * and up delete the key in here and replace it with one of the new
     * /DataSource/Database/ ones when ever that setting is written.
     */
    namespace V1_0_Database {
        const QString NAME = "Database/name";
        const QString HOST_NAME = "Database/hostname";
        const QString PORT = "Database/port";
        const QString USERNAME = "Database/username";
        const QString PASSWORD = "Database/password";
    }

    namespace ReportCustomCriteria {
        const QString CUSTOM_CRITERIA = "ReportCriteria/";
    }
}

Settings::Settings() {
    QString settingsFile;
    settingsFile = QCoreApplication::applicationName() + ".ini";

    if (QFile::exists(settingsFile)) {
        // Load settings from there
        qDebug() << "Loading settings from file" << settingsFile;
        settings = new QSettings(settingsFile, QSettings::IniFormat, this);
    } else {
        qDebug() << "Loading settings from platform default location";
        settings = new QSettings("zxnet", "zxweather", this);
    }
}

Settings::~Settings() {
    delete settings;
    settings = NULL;
}


/* General Settings */
void Settings::setMinimiseToSysTray(bool enabled) {
    settings->setValue(SettingsKey::General::MINIMISE_TO_SYSTRAY,
                       enabled);
}

bool Settings::miniseToSysTray() {
    return settings->value(SettingsKey::General::MINIMISE_TO_SYSTRAY,
                           true).toBool();
}

void Settings::setCloseToSysTray(bool enabled) {
    settings->setValue(SettingsKey::General::CLOSE_TO_SYSTRAY,
                       enabled);
}

bool Settings::closeToSysTray() {
    return settings->value(SettingsKey::General::CLOSE_TO_SYSTRAY,
                           true).toBool();
}

/* Data Source */
void Settings::setLiveDataSourceType(Settings::data_source_type_t type) {
    int val = 0;

    switch(type) {
    case DS_TYPE_SERVER:
        val = 2;
        break;
    case DS_TYPE_WEB_INTERFACE:
        val = 1;
        break;
    case DS_TYPE_DATABASE:
    default:
        val = 0;
    }

    settings->setValue(SettingsKey::DataSource::LIVE_TYPE, val);
}

Settings::data_source_type_t Settings::liveDataSourceType() {

    int val = settings->value(SettingsKey::DataSource::LIVE_TYPE,
                              Settings::DS_TYPE_DATABASE).toInt();

    switch (val) {
    case 2:
        return Settings::DS_TYPE_SERVER;
        break;
    case 1:
        return Settings::DS_TYPE_WEB_INTERFACE;
        break;
    case 0:
    default:
        return Settings::DS_TYPE_DATABASE;
    }
}

void Settings::setSampleDataSourceType(Settings::data_source_type_t type) {
    int val = 0;

    switch(type) {
    case DS_TYPE_WEB_INTERFACE:
        val = 1;
        break;
    case DS_TYPE_DATABASE:
    default:
        val = 0;
    }

    settings->setValue(SettingsKey::DataSource::SAMPLE_TYPE, val);
}

Settings::data_source_type_t Settings::sampleDataSourceType() {

    int val = settings->value(SettingsKey::DataSource::SAMPLE_TYPE,
                              Settings::DS_TYPE_DATABASE).toInt();

    switch (val) {
    case 1:
        return Settings::DS_TYPE_WEB_INTERFACE;
        break;
    case 0:
    default:
        return Settings::DS_TYPE_DATABASE;
    }
}

void Settings::setWebInterfaceUrl(QString url) {
    settings->setValue(SettingsKey::DataSource::URL, url);
}

QString Settings::webInterfaceUrl() {
    QString result = settings->value(SettingsKey::DataSource::URL).toString();
    if (!result.endsWith("/"))
        result.append("/");
    return result;
}

void Settings::setDatabaseName(QString dbName) {
    settings->setValue(SettingsKey::DataSource::Database::NAME,
                       dbName);
    settings->remove(SettingsKey::V1_0_Database::NAME);
}

QString Settings::databaseName() {
    QVariant val = settings->value(SettingsKey::DataSource::Database::NAME);

    if (val.isNull())
        val = settings->value(SettingsKey::V1_0_Database::NAME, "");

    return val.toString();
}

void Settings::setDatabaseHostname(QString hostName) {
    settings->setValue(SettingsKey::DataSource::Database::HOST_NAME,
                       hostName);
    settings->remove(SettingsKey::V1_0_Database::HOST_NAME);
}

QString Settings::databaseHostName() {
    QVariant val = settings->value(SettingsKey::DataSource::Database::HOST_NAME);

    if (val.isNull())
        val = settings->value(SettingsKey::V1_0_Database::HOST_NAME, "");

    return val.toString();
}

void Settings::setDatabasePort(int port) {
    settings->setValue(SettingsKey::DataSource::Database::PORT,
                       port);
    settings->remove(SettingsKey::V1_0_Database::PORT);
}

int Settings::databasePort() {
    QVariant val = settings->value(SettingsKey::DataSource::Database::PORT);

    if (val.isNull())
        val = settings->value(SettingsKey::V1_0_Database::PORT, 5432);

    return val.toInt();
}

void Settings::setDatabaseUsername(QString username) {
    settings->setValue(SettingsKey::DataSource::Database::USERNAME,
                       username);
    settings->remove(SettingsKey::V1_0_Database::USERNAME);
}

QString Settings::databaseUsername() {
    QVariant val = settings->value(SettingsKey::DataSource::Database::USERNAME);

    if (val.isNull())
        val = settings->value(SettingsKey::V1_0_Database::USERNAME, "");

    return val.toString();
}

void Settings::setDatabasePassword(QString password) {
    settings->setValue(SettingsKey::DataSource::Database::PASSWORD,
                       password);
    settings->remove(SettingsKey::V1_0_Database::PASSWORD);
}

QString Settings::databasePassword() {
    QVariant val = settings->value(SettingsKey::DataSource::Database::PASSWORD);

    if (val.isNull())
        val = settings->value(SettingsKey::V1_0_Database::PASSWORD, "");

    return val.toString();
}

void Settings::setServerHostname(QString hostname) {
    settings->setValue(SettingsKey::DataSource::Server::HOST_NAME, hostname);
}

QString Settings::serverHostname() {
    return settings->value(SettingsKey::DataSource::Server::HOST_NAME, "").toString();
}

void Settings::setServerPort(int port) {
    settings->setValue(SettingsKey::DataSource::Server::PORT, port);
}

int Settings::serverPort() {
    return settings->value(SettingsKey::DataSource::Server::PORT, 0).toInt();
}

void Settings::setStationCode(QString name) {
    settings->setValue(SettingsKey::DataSource::STATION_NAME, name);
}

void Settings::setChartColours(ChartColours colours) {
    settings->setValue(SettingsKey::Colours::Charts::APPARENT_TEMPERATURE,
                       colours.apparentTemperature);
    settings->setValue(SettingsKey::Colours::Charts::DEW_POINT,
                       colours.dewPoint);
    settings->setValue(SettingsKey::Colours::Charts::HUMIDITY,
                       colours.humidity);
    settings->setValue(SettingsKey::Colours::Charts::INDOOR_HUMIDITY,
                       colours.indoorHumidity);
    settings->setValue(SettingsKey::Colours::Charts::INDOOR_TEMPERATURE,
                       colours.indoorTemperature);
    settings->setValue(SettingsKey::Colours::Charts::PRESSURE,
                       colours.pressure);
    settings->setValue(SettingsKey::Colours::Charts::TEMPERATURE,
                       colours.temperature);
    settings->setValue(SettingsKey::Colours::Charts::WIND_CHILL,
                       colours.windChill);
    settings->setValue(SettingsKey::Colours::Charts::RAINFALL,
                       colours.rainfall);
    settings->setValue(SettingsKey::Colours::Charts::RAINRATE,
                       colours.rainRate);
    settings->setValue(SettingsKey::Colours::Charts::AVG_WIND_SPEED,
                       colours.averageWindSpeed);
    settings->setValue(SettingsKey::Colours::Charts::GUST_WIND_SPEED,
                       colours.gustWindSpeed);
    settings->setValue(SettingsKey::Colours::Charts::WIND_DIRECTION,
                       colours.windDirection);
    settings->setValue(SettingsKey::Colours::Charts::UV_INDEX,
                       colours.uvIndex);
    settings->setValue(SettingsKey::Colours::Charts::SOLAR_RADIATION,
                       colours.solarRadiation);
    settings->setValue(SettingsKey::Colours::Charts::TITLE,
                       colours.title);
    settings->setValue(SettingsKey::Colours::Charts::BACKGROUND,
                       colours.background);
    settings->setValue(SettingsKey::Colours::Charts::EVAPOTRANSPIRATION,
                       colours.evapotranspiration);
    settings->setValue(SettingsKey::Colours::Charts::RECEPTION,
                       colours.reception);
    settings->setValue(SettingsKey::Colours::Charts::CONSOLE_BATTERY_VOLTAGE,
                       colours.consoleBatteryVoltage);
}

ChartColours Settings::getChartColours() {
    ChartColours colours;
    colours.apparentTemperature = settings->value(
                SettingsKey::Colours::Charts::APPARENT_TEMPERATURE,
                QColor(Qt::darkRed)).value<QColor>();
    colours.dewPoint = settings->value(
                SettingsKey::Colours::Charts::DEW_POINT,
                QColor(Qt::darkCyan)).value<QColor>();
    colours.humidity = settings->value(
                SettingsKey::Colours::Charts::HUMIDITY,
                QColor(Qt::darkMagenta)).value<QColor>();
    colours.indoorHumidity = settings->value(
                SettingsKey::Colours::Charts::INDOOR_HUMIDITY,
                QColor(Qt::darkYellow)).value<QColor>();
    colours.indoorTemperature = settings->value(
                SettingsKey::Colours::Charts::INDOOR_TEMPERATURE,
                QColor(Qt::darkGreen)).value<QColor>();
    colours.pressure = settings->value(
                SettingsKey::Colours::Charts::PRESSURE,
                QColor(Qt::black)).value<QColor>();
    colours.temperature = settings->value(
                SettingsKey::Colours::Charts::TEMPERATURE,
                QColor(Qt::darkBlue)).value<QColor>();
    colours.windChill = settings->value(
                SettingsKey::Colours::Charts::WIND_CHILL,
                QColor(Qt::darkGray)).value<QColor>();
    colours.rainfall = settings->value(
                SettingsKey::Colours::Charts::RAINFALL,
                QColor(Qt::blue)).value<QColor>();
    colours.rainRate = settings->value(
                SettingsKey::Colours::Charts::RAINRATE,
                QColor(Qt::red)).value<QColor>();
    colours.averageWindSpeed = settings->value(
                SettingsKey::Colours::Charts::AVG_WIND_SPEED,
                QColor(Qt::cyan)).value<QColor>();
    colours.gustWindSpeed = settings->value(
                SettingsKey::Colours::Charts::GUST_WIND_SPEED,
                QColor(Qt::red)).value<QColor>();
    colours.windDirection = settings->value(
                SettingsKey::Colours::Charts::WIND_DIRECTION,
                QColor(Qt::green)).value<QColor>();
    colours.uvIndex = settings->value(
                SettingsKey::Colours::Charts::UV_INDEX,
                QColor(Qt::magenta)).value<QColor>();
    colours.solarRadiation = settings->value(
                SettingsKey::Colours::Charts::SOLAR_RADIATION,
                QColor(Qt::yellow)).value<QColor>();
    colours.evapotranspiration = settings->value(
                SettingsKey::Colours::Charts::EVAPOTRANSPIRATION,
                QColor(Qt::gray)).value<QColor>();
    colours.reception = settings->value(
                SettingsKey::Colours::Charts::RECEPTION,
                QColor(Qt::lightGray)).value<QColor>();
    colours.consoleBatteryVoltage = settings->value(
                SettingsKey::Colours::Charts::CONSOLE_BATTERY_VOLTAGE,
                QColor(Qt::gray)).value<QColor>();

    /* Available default colours:
     *   Qt::white
     * white and light gray may not be real options.
     *
     * rain rate and gust wind speed share default colours
     * evapotranspiration and console battery voltage share default colours
     */

    colours.title = settings->value(
                SettingsKey::Colours::Charts::TITLE,
                QColor(Qt::black)).value<QColor>();
    colours.background = settings->value(
                SettingsKey::Colours::Charts::BACKGROUND,
                QColor(Qt::white)).value<QColor>();

    return colours;
}

QString Settings::stationCode() {
    QString result = settings->value(
                SettingsKey::DataSource::STATION_NAME,"").toString();

    // If it can't be found in the normal place try the old v0.2 location
    if (result.isEmpty()) {
        result = settings->value(
                    SettingsKey::DataSource::Database::STATION_NAME_LEGACY,
                    "").toString();

        if (!result.isEmpty()) {
            // Move it to the new location
            setStationCode(result);
            settings->remove(SettingsKey::DataSource::Database::STATION_NAME_LEGACY);
        }
    }

    return result;
}

void Settings::setSingleShotMinimiseToSysTray() {
    settings->setValue(SettingsKey::SingleShot::MINIMISE_TO_SYS_TRAY_INFO, true);
}

bool Settings::singleShotMinimiseToSysTray() {
    return settings->value(SettingsKey::SingleShot::MINIMISE_TO_SYS_TRAY_INFO,
                           false).toBool();
}

void Settings::setSingleShotCloseToSysTray() {
    settings->setValue(SettingsKey::SingleShot::CLOSE_TO_SYS_TRAY_INFO, true);
}

bool Settings::singleShotCloseToSysTray() {
    return settings->value(SettingsKey::SingleShot::CLOSE_TO_SYS_TRAY_INFO,
                           false).toBool();
}

void Settings::setSingleShotFirstRun() {
    settings->setValue(SettingsKey::SingleShot::FIRST_RUN, true);
}

bool Settings::singleShotFirstRun() {
    return settings->value(SettingsKey::SingleShot::FIRST_RUN, false).toBool();
}

void Settings::setLiveTimeoutEnabled(bool enabled) {
    settings->setValue(SettingsKey::General::LiveMon::ENABLED, enabled);
}

bool Settings::liveTimeoutEnabled() {
    return settings->value(SettingsKey::General::LiveMon::ENABLED,
                           true).toBool();
}

void Settings::setLiveTimeoutInterval(uint interval) {
    settings->setValue(SettingsKey::General::LiveMon::INTERVAL, interval);
}

uint Settings::liveTimeoutInterval() {
    return settings->value(SettingsKey::General::LiveMon::INTERVAL,
                           60000).toUInt();
}

void Settings::setImagesWindowHSplitterLayout(QByteArray data) {
    settings->setValue(SettingsKey::General::ImagesWindow::HLAYOUT,
                       data);
}

QByteArray Settings::getImagesWindowHSplitterLayout() {
    return settings->value(SettingsKey::General::ImagesWindow::HLAYOUT,
                           QByteArray()).toByteArray();

}

void Settings::setImagesWindowVSplitterLayout(QByteArray data) {
    settings->setValue(SettingsKey::General::ImagesWindow::VLAYOUT,
                       data);
}

QByteArray Settings::getImagesWindowVSplitterLayout() {
    return settings->value(SettingsKey::General::ImagesWindow::VLAYOUT,
                           QByteArray()).toByteArray();

}

void Settings::setImagesWindowLayout(QByteArray data) {
    settings->setValue(SettingsKey::General::ImagesWindow::WLAYOUT,
                       data);
}

QByteArray Settings::getImagesWindowLayout() {
    return settings->value(SettingsKey::General::ImagesWindow::WLAYOUT,
                           QByteArray()).toByteArray();

}

QStringList Settings::imageTypeSortOrder() {
    // This setting is cached as this function is called repeatedly by some
    // sort comparison functions

    if (!imageTypePriority.isEmpty()) {
        return imageTypePriority;
    }

    QString def = "TLVID,CAM,AEMSA,AEHVT,AEMCI,AEZA,AEMSP,EHVP,AEHCP,AESEA,AETHE,AENO,AEHVC,APTD,SPEC";
    imageTypePriority = settings->value(SettingsKey::General::ImagesWindow::TYPE_SORT,
                           def).toString().split(",");
    return imageTypePriority;
}

QVariant Settings::weatherValueWidgetSetting(QString name, QString setting, QVariant defaultValue) {
    QString s = SettingsKey::WeatherValueWidgets::ROOT + "/" + name + "/" + setting;
    return settings->value(s, defaultValue);
}

void Settings::setWeatherValueWidgetSetting(QString name, QString setting, QVariant value) {
    QString s = SettingsKey::WeatherValueWidgets::ROOT + "/" + name + "/" + setting;
    settings->setValue(s, value);
}


bool Settings::imperial() const {
    return settings->value(SettingsKey::General::IMPERIAL, false).toBool();
}

void Settings::setImperial(bool enabled) {
    settings->setValue(SettingsKey::General::IMPERIAL, enabled);
}


int Settings::liveAggregateSeconds() const {
    return settings->value(SettingsKey::LiveChart::AGGREGATE_SECONDS, 60).toInt();
}

int Settings::liveTimespanMinutes() const {
    return settings->value(SettingsKey::LiveChart::TIMESPAN_MINUTES, 2).toInt();
}

bool Settings::liveAggregate() const {
    return settings->value(SettingsKey::LiveChart::AGGERGATE, false).toBool();
}

bool Settings::liveMaxRainRate() const {
    return settings->value(SettingsKey::LiveChart::MAX_RAIN_RATE, true).toBool();
}

bool Settings::liveStormRain() const {
    return settings->value(SettingsKey::LiveChart::STORM_RAIN, true).toBool();
}

bool Settings::liveTagsEnabled() const {
    return settings->value(SettingsKey::LiveChart::LIVE_TAGS, false).toBool();
}

bool Settings::liveMultipleAxisRectsEnabled() const {
    return settings->value(SettingsKey::LiveChart::MULTI_RECT, false).toBool();
}

void Settings::setLiveAggregateSeconds(int value) {
    settings->setValue(SettingsKey::LiveChart::AGGREGATE_SECONDS, value);
}

void Settings::setLiveTimespanMinutes(int value){
    settings->setValue(SettingsKey::LiveChart::TIMESPAN_MINUTES, value);
}

void Settings::setLiveAggregate(bool value) {
    settings->setValue(SettingsKey::LiveChart::AGGERGATE, value);
}

void Settings::setLiveMaxRainRate(bool value) {
    settings->setValue(SettingsKey::LiveChart::MAX_RAIN_RATE, value);
}

void Settings::setLiveStormRain(bool value) {
    settings->setValue(SettingsKey::LiveChart::STORM_RAIN, value);
}

void Settings::setLiveTagsEnabled(bool value) {
    settings->setValue(SettingsKey::LiveChart::LIVE_TAGS, value);
}

void Settings::setLiveMultipleAxisRectsEnabled(bool value) {
    settings->setValue(SettingsKey::LiveChart::MULTI_RECT, value);
}

void Settings::saveMainWindowState(QByteArray state) {
    settings->setValue(SettingsKey::General::MAIN_WINDOW_STATE, state);
}

QByteArray Settings::mainWindowState() const {
    return settings->value(SettingsKey::General::MAIN_WINDOW_STATE, QByteArray()).toByteArray();
}

void Settings::saveMainWindowGeometry(QByteArray geom) {
    settings->setValue(SettingsKey::General::MAIN_WINDOW_GEOMETRY, geom);
}

QByteArray Settings::mainWindowGeometry() const {
    return settings->value(SettingsKey::General::MAIN_WINDOW_GEOMETRY, QByteArray()).toByteArray();
}

void Settings::saveReportCriteria(QString report, QVariantMap criteria) {
    QString key = SettingsKey::ReportCustomCriteria::CUSTOM_CRITERIA;
    if (sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE) {
        key += webInterfaceUrl().replace("/", "_") + stationCode() + "/" + report;
    } else {
        key += stationCode() + "/" + report;
    }

    settings->setValue(key, criteria);
}

QVariantMap Settings::getReportCriteria(QString report) {
    QString key = SettingsKey::ReportCustomCriteria::CUSTOM_CRITERIA;
    if (sampleDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE) {
        key += webInterfaceUrl().replace("/", "_") + stationCode() + "/" + report;
    } else {
        key += stationCode() + "/" + report;
    }

    QVariantMap map;
    return settings->value(key, map).toMap();
}
