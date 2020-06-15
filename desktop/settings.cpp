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
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QDebug>

#include "constants.h"

/** Settings keys.
 */
namespace SettingsKey {
    /** General settings (minimise to system tray, etc).
     */
    namespace General {
        const QString MINIMISE_TO_SYSTRAY = "General/minimise_to_systray";
        const QString CLOSE_TO_SYSTRAY = "General/close_to_systray";

        const QString IMPERIAL = "General/imperial";
        const QString METRIC_KMH = "General/metric_kmh";

        const QString MAIN_WINDOW_STATE ="General/mw_state";
        const QString MAIN_WINDOW_GEOMETRY = "General/mw_geom";

        // Number of hours of live data to keep in memory. This is currently used
        // only to initialise live plots.
        const QString LIVE_BUFFER_HOURS = "General/live_buffer_hours";

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
            const QString VIEW_MODE = "General/images_window/view_mode";
            const QString TREE_VISIBLE = "General/images_window/nav_tree";
            const QString PREVIEW_VISIBLE = "General/images_window/preview";
            const QString WINDOW_GEOMETRY = "General/images_window/window_geom";

            // When true (default) and there are images for the current day, the current day is
            // visible when the images window is opened.
            const QString SHOW_CURRENT_DAY = "General/images_window/show_current_day";

            // When true (default) and there are images for the current day, the current day is
            // selected when the images window is opened and thumbnails for the current days
            // images will be visible in the list view. This may trigger the downloading of all
            // images for the current day.
            const QString SELECT_CURRENT_DAY = "General/images_window/select_current_day";
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

    namespace Chart {
        const QString CURSOR_ENABLED = "Chart/cursor";
        const QString CHART_WINDOW_STATE ="Chart/window_state";
        const QString CHART_WINDOW_GEOMETRY = "Chart/window_geom";

        namespace FontDefaults {
            const QString TITLE = "Chart/Fonts/title";
            const QString LEGEND = "Chart/Fonts/legend";
            const QString AXIS_LABEL = "Chart/Fonts/axis_label";
            const QString TICK_LABEL = "Chart/Fonts/tick_label";
        }
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
            const QString HIGH_TEMPERATURE = "Colours/Charts/high_temperature";
            const QString LOW_TEMPERATURE = "Colours/Charts/low_temperature";
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
            const QString GUST_WIND_DIRECTION = "Colours/Charts/gust_wind_direction";
            const QString UV_INDEX = "Colours/Charts/uv_index";
            const QString HIGH_UV_INDEX = "Colours/Charts/high_uv_index";
            const QString SOLAR_RADIATION = "Colours/Charts/solar_radiation";
            const QString HIGH_SOLAR_RADIATION = "Colours/Charts/high_solar_radiation";
            const QString EVAPOTRANSPIRATION = "Colours/Charts/evapotranspiration";
            const QString RECEPTION = "Colours/Charts/reception";
            const QString CONSOLE_BATTERY_VOLTAGE = "Colours/Charts/console_battery_voltage";
            const QString TITLE = "Colours/Charts/title";
            const QString BACKGROUND = "Colours/Charts/background";

            const QString LEAF_WETNESS_1 = "Colours/Charts/leaf_wetness_1";
            const QString LEAF_WETNESS_2 = "Colours/Charts/leaf_wetness_2";
            const QString LEAF_TEMPERATURE_1 = "Colours/Charts/leaf_temperature_1";
            const QString LEAF_TEMPERATURE_2 = "Colours/Charts/leaf_temperature_2";

            const QString SOIL_MOISTURE_1 = "Colours/Charts/soil_moisture_1";
            const QString SOIL_MOISTURE_2 = "Colours/Charts/soil_moisture_2";
            const QString SOIL_MOISTURE_3 = "Colours/Charts/soil_moisture_3";
            const QString SOIL_MOISTURE_4 = "Colours/Charts/soil_moisture_4";
            const QString SOIL_TEMPERATURE_1 = "Colours/Charts/soil_temperature_1";
            const QString SOIL_TEMPERATURE_2 = "Colours/Charts/soil_temperature_2";
            const QString SOIL_TEMPERATURE_3 = "Colours/Charts/soil_temperature_3";
            const QString SOIL_TEMPERATURE_4 = "Colours/Charts/soil_temperature_4";

            const QString EXTRA_HUMIDITY_1 = "Colours/Charts/extra_humidity_1";
            const QString EXTRA_HUMIDITY_2 = "Colours/Charts/extra_humidity_2";

            const QString EXTRA_TEMPERATURE_1 = "Colours/Charts/extra_temperature_1";
            const QString EXTRA_TEMPERATURE_2 = "Colours/Charts/extra_temperature_2";
            const QString EXTRA_TEMPERATURE_3 = "Colours/Charts/extra_temperature_3";
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

    namespace Reports {
        const QString SEARCH_PATH = "Reports/SearchPath";
    }
}

Settings::Settings() {
    settings = NULL;
    QString settingsFile;

#ifdef Q_OS_WIN
    settingsFile = QCoreApplication::applicationDirPath()
            + "/"
            + QFileInfo(QCoreApplication::applicationFilePath()).baseName() + ".ini";

    // The settings file used to be zxweather-desktop.ini in the current directory. Now
    // the file is expected to be alongside the executable with a matching basename.
    // We'll attempt to rename any existing file to the new filename.
    QString oldSettingsFileName = Constants::APP_NAME + ".ini";
    if (!QFile(settingsFile).exists()
            && QFile().exists(oldSettingsFileName)) {
        qDebug() << "Migrating configuration from" << oldSettingsFileName
                 << "to" << settingsFile;
        if (!QFile::rename(oldSettingsFileName, settingsFile)) {
            qDebug() << "Failed to rename config file. Running with" << oldSettingsFileName
                     << "instead of" << settingsFile;
            settingsFile = oldSettingsFileName;
        }
    }
#else
    settingsFile = "~/.zxweather-desktop.ini"
#endif


    setConfigFile(settingsFile);

    // These are the default colours for charts.
    defaultChartColours.apparentTemperature = QColor(Qt::darkRed);
    defaultChartColours.dewPoint = QColor(Qt::darkCyan);
    defaultChartColours.humidity = QColor(Qt::darkMagenta);
    defaultChartColours.indoorHumidity = QColor(Qt::darkYellow);
    defaultChartColours.indoorTemperature = QColor(Qt::darkGreen);
    defaultChartColours.pressure = QColor(Qt::black);
    defaultChartColours.temperature = QColor(Qt::darkBlue);
    defaultChartColours.highTemperature = defaultChartColours.temperature;
    defaultChartColours.lowTemperature = defaultChartColours.temperature;
    defaultChartColours.windChill = QColor(Qt::darkGray);
    defaultChartColours.rainfall = QColor(Qt::blue);
    defaultChartColours.rainRate = QColor(Qt::red);
    defaultChartColours.averageWindSpeed = QColor(Qt::cyan);
    defaultChartColours.gustWindSpeed = defaultChartColours.rainRate;
    defaultChartColours.windDirection = QColor(Qt::green);
    defaultChartColours.gustWindDirection = defaultChartColours.windDirection;
    defaultChartColours.uvIndex = QColor(Qt::magenta);
    defaultChartColours.highUVIndex = defaultChartColours.uvIndex;
    defaultChartColours.solarRadiation = QColor(Qt::yellow);
    defaultChartColours.highSolarRadiation = defaultChartColours.solarRadiation;
    defaultChartColours.evapotranspiration = QColor(Qt::gray);
    defaultChartColours.reception = QColor(Qt::lightGray);
    defaultChartColours.consoleBatteryVoltage = defaultChartColours.evapotranspiration;

    defaultChartColours.leafWetness1 = defaultChartColours.rainfall;
    defaultChartColours.leafWetness2 = defaultChartColours.rainfall;
    defaultChartColours.leafTemperature1 = defaultChartColours.temperature;
    defaultChartColours.leafTemperature2 = defaultChartColours.temperature;

    defaultChartColours.soilMoisture1 = defaultChartColours.rainRate;
    defaultChartColours.soilMoisture2 = defaultChartColours.rainRate;
    defaultChartColours.soilMoisture3 = defaultChartColours.rainRate;
    defaultChartColours.soilMoisture4 = defaultChartColours.rainRate;

    defaultChartColours.soilTemperature1 = defaultChartColours.temperature;
    defaultChartColours.soilTemperature2 = defaultChartColours.temperature;
    defaultChartColours.soilTemperature3 = defaultChartColours.temperature;
    defaultChartColours.soilTemperature4 = defaultChartColours.temperature;

    defaultChartColours.extraHumidity1 = defaultChartColours.humidity;
    defaultChartColours.extraHumidity2 = defaultChartColours.humidity;

    defaultChartColours.extraTemperature1 = defaultChartColours.temperature;
    defaultChartColours.extraTemperature2 = defaultChartColours.temperature;
    defaultChartColours.extraTemperature3 = defaultChartColours.temperature;


    /* Available default colours:
     *   Qt::white
     * white and light gray may not be real options.
     *
     * rain rate, gust wind speed and soil moistures 1-4 share default colours
     * evapotranspiration and console battery voltage share default colours
     * rainfall and leaf wetness 1&2 share default colours
     * outdoor temperature, high temperature, low temperature,
     *      soil temperatures 1-4 and extra temperatures 1-3 share default colours
     * outdoor humidity and extra humidities 1 & 2 share default colours
     * uv index and high uv index share default colours
     * solar radiation and high solar radiation share default colours
     */

    defaultChartColours.title = QColor(Qt::black);
    defaultChartColours.background = QColor(Qt::white);
}

Settings::~Settings() {
    if (settings != NULL) {
        delete settings;
        settings = NULL;
    }
}

void Settings::setConfigFile(const QString filename) {
    if (settings != NULL) {
        settings->sync();
        delete settings;
        settings = NULL;
    }

    if (QFile::exists(filename)) {
        // Load settings from there
        qDebug() << "Loading settings from file" << filename;
        settings = new QSettings(filename, QSettings::IniFormat, this);
    } else {
        qDebug() << "Local settings file not found:" << filename;
        qDebug() << "Loading settings from platform default location";
        settings = new QSettings("zxnet", "zxweather", this);
    }
}


/* General Settings */
void Settings::setMinimiseToSysTray(bool enabled) {
    settings->setValue(SettingsKey::General::MINIMISE_TO_SYSTRAY,
                       enabled);
}

bool Settings::miniseToSysTray() {
    return settings->value(SettingsKey::General::MINIMISE_TO_SYSTRAY,
                           false).toBool();
}

void Settings::setCloseToSysTray(bool enabled) {
    settings->setValue(SettingsKey::General::CLOSE_TO_SYSTRAY,
                       enabled);
}

bool Settings::closeToSysTray() {
    return settings->value(SettingsKey::General::CLOSE_TO_SYSTRAY,
                           false).toBool();
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
    settings->setValue(SettingsKey::Colours::Charts::HIGH_TEMPERATURE,
                       colours.highTemperature);
    settings->setValue(SettingsKey::Colours::Charts::LOW_TEMPERATURE,
                       colours.lowTemperature);
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
    settings->setValue(SettingsKey::Colours::Charts::GUST_WIND_DIRECTION,
                       colours.gustWindDirection);
    settings->setValue(SettingsKey::Colours::Charts::UV_INDEX,
                       colours.uvIndex);
    settings->setValue(SettingsKey::Colours::Charts::HIGH_UV_INDEX,
                       colours.highUVIndex);
    settings->setValue(SettingsKey::Colours::Charts::SOLAR_RADIATION,
                       colours.solarRadiation);
    settings->setValue(SettingsKey::Colours::Charts::HIGH_SOLAR_RADIATION,
                       colours.highSolarRadiation);
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
    settings->setValue(SettingsKey::Colours::Charts::LEAF_WETNESS_1, colours.leafWetness1);
    settings->setValue(SettingsKey::Colours::Charts::LEAF_WETNESS_2, colours.leafWetness2);
    settings->setValue(SettingsKey::Colours::Charts::LEAF_TEMPERATURE_1, colours.leafTemperature1);
    settings->setValue(SettingsKey::Colours::Charts::LEAF_TEMPERATURE_2, colours.leafTemperature2);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_MOISTURE_1, colours.soilMoisture1);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_MOISTURE_2, colours.soilMoisture2);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_MOISTURE_3, colours.soilMoisture3);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_MOISTURE_4, colours.soilMoisture4);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_1, colours.soilTemperature1);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_2, colours.soilTemperature2);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_3, colours.soilTemperature3);
    settings->setValue(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_4, colours.soilTemperature4);
    settings->setValue(SettingsKey::Colours::Charts::EXTRA_HUMIDITY_1, colours.extraHumidity1);
    settings->setValue(SettingsKey::Colours::Charts::EXTRA_HUMIDITY_2, colours.extraHumidity2);
    settings->setValue(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_1, colours.extraTemperature1);
    settings->setValue(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_2, colours.extraTemperature2);
    settings->setValue(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_3, colours.extraTemperature3);
}

ChartColours Settings::getChartColours() {
    ChartColours colours;
    colours.apparentTemperature = settings->value(
                SettingsKey::Colours::Charts::APPARENT_TEMPERATURE,
                defaultChartColours.apparentTemperature).value<QColor>();
    colours.dewPoint = settings->value(
                SettingsKey::Colours::Charts::DEW_POINT,
                defaultChartColours.dewPoint).value<QColor>();
    colours.humidity = settings->value(
                SettingsKey::Colours::Charts::HUMIDITY,
                defaultChartColours.humidity).value<QColor>();
    colours.indoorHumidity = settings->value(
                SettingsKey::Colours::Charts::INDOOR_HUMIDITY,
                defaultChartColours.indoorHumidity).value<QColor>();
    colours.indoorTemperature = settings->value(
                SettingsKey::Colours::Charts::INDOOR_TEMPERATURE,
                defaultChartColours.indoorTemperature).value<QColor>();
    colours.pressure = settings->value(
                SettingsKey::Colours::Charts::PRESSURE,
                defaultChartColours.pressure).value<QColor>();
    colours.temperature = settings->value(
                SettingsKey::Colours::Charts::TEMPERATURE,
                defaultChartColours.temperature).value<QColor>();
    colours.highTemperature = settings->value(
                SettingsKey::Colours::Charts::HIGH_TEMPERATURE,
                defaultChartColours.highTemperature).value<QColor>();
    colours.lowTemperature = settings->value(
                SettingsKey::Colours::Charts::LOW_TEMPERATURE,
                defaultChartColours.lowTemperature).value<QColor>();
    colours.windChill = settings->value(
                SettingsKey::Colours::Charts::WIND_CHILL,
                defaultChartColours.windChill).value<QColor>();
    colours.rainfall = settings->value(
                SettingsKey::Colours::Charts::RAINFALL,
                defaultChartColours.rainfall).value<QColor>();
    colours.rainRate = settings->value(
                SettingsKey::Colours::Charts::RAINRATE,
                defaultChartColours.rainRate).value<QColor>();
    colours.averageWindSpeed = settings->value(
                SettingsKey::Colours::Charts::AVG_WIND_SPEED,
                defaultChartColours.averageWindSpeed).value<QColor>();
    colours.gustWindSpeed = settings->value(
                SettingsKey::Colours::Charts::GUST_WIND_SPEED,
                defaultChartColours.gustWindSpeed).value<QColor>();
    colours.windDirection = settings->value(
                SettingsKey::Colours::Charts::WIND_DIRECTION,
                defaultChartColours.windDirection).value<QColor>();
    colours.gustWindDirection = settings->value(
                SettingsKey::Colours::Charts::GUST_WIND_DIRECTION,
                defaultChartColours.gustWindDirection).value<QColor>();
    colours.uvIndex = settings->value(
                SettingsKey::Colours::Charts::UV_INDEX,
                defaultChartColours.uvIndex).value<QColor>();
    colours.highUVIndex = settings->value(
                SettingsKey::Colours::Charts::HIGH_UV_INDEX,
                defaultChartColours.highUVIndex).value<QColor>();
    colours.solarRadiation = settings->value(
                SettingsKey::Colours::Charts::SOLAR_RADIATION,
                defaultChartColours.solarRadiation).value<QColor>();
    colours.highSolarRadiation = settings->value(
                SettingsKey::Colours::Charts::HIGH_SOLAR_RADIATION,
                defaultChartColours.highSolarRadiation).value<QColor>();
    colours.evapotranspiration = settings->value(
                SettingsKey::Colours::Charts::EVAPOTRANSPIRATION,
                defaultChartColours.evapotranspiration).value<QColor>();
    colours.reception = settings->value(
                SettingsKey::Colours::Charts::RECEPTION,
                defaultChartColours.reception).value<QColor>();
    colours.consoleBatteryVoltage = settings->value(
                SettingsKey::Colours::Charts::CONSOLE_BATTERY_VOLTAGE,
                defaultChartColours.consoleBatteryVoltage).value<QColor>();

    colours.leafWetness1 = settings->value(SettingsKey::Colours::Charts::LEAF_WETNESS_1, defaultChartColours.leafWetness1).value<QColor>();
    colours.leafWetness2 = settings->value(SettingsKey::Colours::Charts::LEAF_WETNESS_2, defaultChartColours.leafWetness1).value<QColor>();
    colours.leafTemperature1 = settings->value(SettingsKey::Colours::Charts::LEAF_TEMPERATURE_1, defaultChartColours.leafTemperature1).value<QColor>();
    colours.leafTemperature2 = settings->value(SettingsKey::Colours::Charts::LEAF_TEMPERATURE_1, defaultChartColours.leafTemperature2).value<QColor>();

    colours.soilMoisture1 = settings->value(SettingsKey::Colours::Charts::SOIL_MOISTURE_1, defaultChartColours.soilMoisture1).value<QColor>();
    colours.soilMoisture2 = settings->value(SettingsKey::Colours::Charts::SOIL_MOISTURE_2, defaultChartColours.soilMoisture2).value<QColor>();
    colours.soilMoisture3 = settings->value(SettingsKey::Colours::Charts::SOIL_MOISTURE_3, defaultChartColours.soilMoisture3).value<QColor>();
    colours.soilMoisture4 = settings->value(SettingsKey::Colours::Charts::SOIL_MOISTURE_4, defaultChartColours.soilMoisture4).value<QColor>();

    colours.soilTemperature1 = settings->value(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_1, defaultChartColours.soilTemperature1).value<QColor>();
    colours.soilTemperature2 = settings->value(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_2, defaultChartColours.soilTemperature2).value<QColor>();
    colours.soilTemperature3 = settings->value(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_3, defaultChartColours.soilTemperature3).value<QColor>();
    colours.soilTemperature4 = settings->value(SettingsKey::Colours::Charts::SOIL_TEMPERATURE_4, defaultChartColours.soilTemperature4).value<QColor>();

    colours.extraHumidity1 = settings->value(SettingsKey::Colours::Charts::EXTRA_HUMIDITY_1, defaultChartColours.extraHumidity1).value<QColor>();
    colours.extraHumidity1 = settings->value(SettingsKey::Colours::Charts::EXTRA_HUMIDITY_1, defaultChartColours.extraHumidity2).value<QColor>();

    colours.extraTemperature1 = settings->value(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_1, defaultChartColours.extraTemperature1).value<QColor>();
    colours.extraTemperature2 = settings->value(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_2, defaultChartColours.extraTemperature2).value<QColor>();
    colours.extraTemperature3 = settings->value(SettingsKey::Colours::Charts::EXTRA_TEMPERATURE_3, defaultChartColours.extraTemperature3).value<QColor>();

    colours.title = settings->value(
                SettingsKey::Colours::Charts::TITLE,
                defaultChartColours.title).value<QColor>();
    colours.background = settings->value(
                SettingsKey::Colours::Charts::BACKGROUND,
                defaultChartColours.background).value<QColor>();

    return colours;
}

QString Settings::stationCode() {

    if (!stationCodeOverride.isNull()) {
        return stationCodeOverride;
    }

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

void Settings::saveImagesWindowGeometry(QByteArray geom) {
    settings->setValue(SettingsKey::General::ImagesWindow::WINDOW_GEOMETRY, geom);
}

QByteArray Settings::imagesWindowGeometry() const {
    return settings->value(SettingsKey::General::ImagesWindow::WINDOW_GEOMETRY, QByteArray()).toByteArray();
}


void Settings::setImagesWindowViewMode(int viewMode) {
    settings->setValue(SettingsKey::General::ImagesWindow::VIEW_MODE,
                       viewMode);
}

int Settings::imagesWindowViewMode() {
    return settings->value(SettingsKey::General::ImagesWindow::VIEW_MODE, 0).toInt();

}

void Settings::setImagesWindowNavigationPaneVisible(bool visible) {
    settings->setValue(SettingsKey::General::ImagesWindow::TREE_VISIBLE,
                       visible);
}

bool Settings::imagesWindowNavigationPaneVisible() {
    return settings->value(SettingsKey::General::ImagesWindow::TREE_VISIBLE, true).toBool();
}

void Settings::setImagesWindowPreviewPaneVisible(bool visible) {
    settings->setValue(SettingsKey::General::ImagesWindow::PREVIEW_VISIBLE,
                       visible);
}

bool Settings::imagesWindowPreviewPaneVisible() {
    return settings->value(SettingsKey::General::ImagesWindow::PREVIEW_VISIBLE, true).toBool();
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

void Settings::setUnits(bool imperial, bool kmh) {
    bool i = this->imperial();
    bool k = this->kmh();
    settings->setValue(SettingsKey::General::IMPERIAL, imperial);
    settings->setValue(SettingsKey::General::METRIC_KMH, kmh);

    if (imperial != i || kmh != k) {
        emit unitsChanged(imperial, kmh);
    }
}

bool Settings::imperial() const {
    return settings->value(SettingsKey::General::IMPERIAL, false).toBool();
}

bool Settings::kmh() const {
        return settings->value(SettingsKey::General::METRIC_KMH, true).toBool();
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

void Settings::saveChartWindowState(QByteArray state) {
    settings->setValue(SettingsKey::Chart::CHART_WINDOW_STATE, state);
}

QByteArray Settings::chartWindowState() const {
    return settings->value(SettingsKey::Chart::CHART_WINDOW_STATE, QByteArray()).toByteArray();
}

void Settings::saveChartWindowGeometry(QByteArray geom) {
    settings->setValue(SettingsKey::Chart::CHART_WINDOW_GEOMETRY, geom);
}

QByteArray Settings::chartWindowGeometry() const {
    return settings->value(SettingsKey::Chart::CHART_WINDOW_GEOMETRY, QByteArray()).toByteArray();
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

bool Settings::chartCursorEnabled() {
    return settings->value(SettingsKey::Chart::CURSOR_ENABLED, true).toBool();
}

void Settings::setChartCursorEnabled(bool enabled) {
    settings->setValue(SettingsKey::Chart::CURSOR_ENABLED, enabled);
}

bool Settings::showCurrentDayInImageWindow() {
    return settings->value(SettingsKey::General::ImagesWindow::SHOW_CURRENT_DAY, true).toBool();
}

bool Settings::selectCurrentDayInImageWindow() {
    return settings->value(SettingsKey::General::ImagesWindow::SELECT_CURRENT_DAY, true).toBool();
}

QStringList Settings::reportSearchPath() const {
    QStringList result;

    foreach(QString s, extraReportSearchPaths) {
        if (s.isEmpty()) {
            continue;
        }

        QFileInfo dir(s);

        if (dir.exists() && dir.isDir() && dir.isReadable()) {
            result << s;
        }
    }

    QString val = settings->value(SettingsKey::Reports::SEARCH_PATH, "").toString();

    if (!val.isEmpty()) {
        if (val.contains(";")) {
            foreach(QString s, val.split(";")) {
                if (s.isEmpty()) {
                    continue;
                }

                QFileInfo dir(s);

                if (dir.exists() && dir.isDir() && dir.isReadable()) {
                    result << s;
                }
            }
        } else {
            QFileInfo dir(val);

            if (dir.exists() && dir.isDir() && dir.isReadable()) {
                result << val;
            }
        }
    }

//#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
//     result << QDir::cleanPath(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation) + "/reports");
//     result << QDir::cleanPath(QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation) + "/reports");
//#else
//     result << QDir::cleanPath(QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/reports");
//#endif

    result << "./reports";

    // Internal report definitions and assets come last so they can be overridden externally.
    result << ":/reports";

    foreach(QString blackListed, blacklistReportSearchPaths) {
        result.removeAll(blackListed);
    }

    return result;
}

void Settings::temporarilyAddReportSearchPath(const QString path) {
    extraReportSearchPaths.append(path);

    blacklistReportSearchPaths.removeAll(path);
}

void Settings::removeReportSearchPath(const QString path) {
    blacklistReportSearchPaths.append(path);
    extraReportSearchPaths.removeAll(path);
}


int Settings::liveBufferHours() const {
    return settings->value(SettingsKey::General::LIVE_BUFFER_HOURS, 1).toInt();
}


void Settings::setDefaultChartTitleFont(QFont font) {
    settings->setValue(SettingsKey::Chart::FontDefaults::TITLE, font.toString());
}

QFont Settings::defaultChartAxisTickLabelFont() const {
    QFont font;
    font.fromString(settings->value(SettingsKey::Chart::FontDefaults::TICK_LABEL,
                                    QApplication::font().toString()).toString());
    return font;
}

void Settings::setDefaultChartAxisTickLabelFont(QFont font) {
    settings->setValue(SettingsKey::Chart::FontDefaults::TICK_LABEL, font.toString());
}

QFont Settings::defaultChartAxisLabelFont() const {
    QFont font;
    font.fromString(settings->value(SettingsKey::Chart::FontDefaults::AXIS_LABEL,
                                    QApplication::font().toString()).toString());
    return font;
}

void Settings::setDefaultChartAxisLabelFont(QFont font) {
    settings->setValue(SettingsKey::Chart::FontDefaults::AXIS_LABEL, font.toString());
}

QFont Settings::defaultChartLegendFont() const {
    QFont font;
    font.fromString(settings->value(SettingsKey::Chart::FontDefaults::LEGEND,
                                    QApplication::font().toString()).toString());
    return font;
}

void Settings::setDefaultChartLegendFont(QFont font) {
    settings->setValue(SettingsKey::Chart::FontDefaults::LEGEND, font.toString());
}

QFont Settings::defaultChartTitleFont() const {
    QFont font;
    font.fromString(settings->value(
                        SettingsKey::Chart::FontDefaults::TITLE,
                        QFont("sans", 12, QFont::Bold).toString()).toString());
    return font;

}

void Settings::resetFontsToDefaults() const {
    settings->remove(SettingsKey::Chart::FontDefaults::TITLE);
    settings->remove(SettingsKey::Chart::FontDefaults::LEGEND);
    settings->remove(SettingsKey::Chart::FontDefaults::AXIS_LABEL);
    settings->remove(SettingsKey::Chart::FontDefaults::TICK_LABEL);
}

bool Settings::isStationCodeOverridden() const {
    return !stationCodeOverride.isNull();
}

void Settings::overrideStationCode(const QString &stationCode) {
    stationCodeOverride = stationCode;
}
