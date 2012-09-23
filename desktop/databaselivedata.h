/*****************************************************************************
 *            Created: 28/08/2012
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

#ifndef DATABASELIVEDATA_H
#define DATABASELIVEDATA_H

#include "datasource.h"
#include "database.h"

/** An implementation of AbstractLiveData that stores the live data in a
 * live_data_record from the database layer.
 */
class DatabaseLiveData : public AbstractLiveData {
    Q_OBJECT
public:
    /** Constructs a new DatabaseLiveData object using the supplied
     * live_data_record from the database layer.
     *
     * @param rec The live data record. Obtained from wdb_get_live_data()
     * @param parent The parent QObject.
     */
    DatabaseLiveData(live_data_record rec, QObject *parent=0):
        AbstractLiveData(parent) {record = rec;}

    float getIndoorTemperature() const { return record.indoor_temperature; }
    int   getIndoorRelativeHumidity() const { return record.indoor_relative_humidity; }
    float getTemperature() const { return record.temperature; }
    int   getRelativeHumidity() const { return record.relative_humidity; }
    float getDewPoint() const { return record.dew_point; }
    float getWindChill() const { return record.wind_chill; }
    float getApparentTemperature() const { return record.apparent_temperature; }
    float getAbsolutePressure() const { return record.absolute_pressure; }
    float getAverageWindSpeed() const { return record.average_wind_speed; }
    float getGustWindSpeed() const { return record.gust_wind_speed; }
    QString getWindDirection() const { return QString(record.wind_direction); }
    QDateTime getTimestamp() const { return QDateTime::fromTime_t(record.download_timestamp); }

private:
    live_data_record record;
};

#endif // DATABASELIVEDATA_H
