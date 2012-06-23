/*****************************************************************************
 *            Created: 23/06/2012
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

#ifndef DATABASE_H
#define DATABASE_H

#include "dbsignaladapter.h"

typedef struct _live_data_record {
    float indoor_temperature;
    int indoor_relative_humidity;
    float temperature;
    int relative_humidity;
    float dew_point;
    float wind_chill;
    float apparent_temperature;
    float absolute_pressure;
    float average_wind_speed;
    float gust_wind_speed;
    char wind_direction[4];
    long download_timestamp;
} live_data_record;

void wdb_set_signal_adapter(DBSignalAdapter *adapter);

/* Connects to the database */
bool wdb_connect(const char *target, const char *username, const char* password);

void wdb_disconnect();

/* Gets the current live data */
live_data_record wdb_get_live_data();

/* Checks to see if there is new live data available */
bool wdb_live_data_available();

#endif // DATABASE_H
