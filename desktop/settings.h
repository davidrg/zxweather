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

class QSettings;

typedef struct _chart_colours {
    QColor temperature;
    QColor indoorTemperature;
    QColor apparentTemperature;
    QColor windChill;
    QColor dewPoint;

    QColor humidity;
    QColor indoorHumidity;

    QColor pressure;

    QColor rainfall;

    QColor averageWindSpeed;
    QColor gustWindSpeed;
    QColor windDirection;

    QColor uvIndex;
    QColor solarRadiation;

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

    QStringList imageTypeSortOrder();
private:
    Settings();
    ~Settings();

    Settings(Settings const&); /* Not implemented. Don't use it. */
    void operator=(Settings const&); /* Not implemented. Don't use it. */

    QSettings *settings;

    QStringList imageTypePriority;
};

#endif // SETTINGS_H
