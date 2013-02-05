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

/** Settings keys.
 */
namespace SettingsKey {
    /** General settings (minimise to system tray, etc).
     */
    namespace General {
        const QString MINIMISE_TO_SYSTRAY = "General/minimise_to_systray";
        const QString CLOSE_TO_SYSTRAY = "General/close_to_systray";
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
}

Settings::Settings() {
    QString settingsFile;
    settingsFile = QCoreApplication::applicationName() + ".ini";

    if (QFile::exists(settingsFile)) {
        // Load settings from there
        settings = new QSettings(settingsFile, QSettings::IniFormat, this);
    } else {
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
}

ChartColours Settings::getChartColours() {
    ChartColours colours;
    colours.apparentTemperature = settings->value(
                SettingsKey::Colours::Charts::APPARENT_TEMPERATURE,
                Qt::darkRed).value<QColor>();
    colours.dewPoint = settings->value(
                SettingsKey::Colours::Charts::DEW_POINT,
                Qt::darkCyan).value<QColor>();
    colours.humidity = settings->value(
                SettingsKey::Colours::Charts::HUMIDITY,
                Qt::darkMagenta).value<QColor>();
    colours.indoorHumidity = settings->value(
                SettingsKey::Colours::Charts::INDOOR_HUMIDITY,
                Qt::darkYellow).value<QColor>();
    colours.indoorTemperature = settings->value(
                SettingsKey::Colours::Charts::INDOOR_TEMPERATURE,
                Qt::darkGreen).value<QColor>();
    colours.pressure = settings->value(
                SettingsKey::Colours::Charts::PRESSURE,
                Qt::black).value<QColor>();
    colours.temperature = settings->value(
                SettingsKey::Colours::Charts::TEMPERATURE,
                Qt::darkBlue).value<QColor>();
    colours.windChill = settings->value(
                SettingsKey::Colours::Charts::WIND_CHILL,
                Qt::darkGray).value<QColor>();
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
