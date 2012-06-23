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

/** Live data from the database.
 */
typedef struct _live_data_record {
    float indoor_temperature; /*!< Indoor temperature (°C). 1 DP. */
    int indoor_relative_humidity; /*!< Indoor relative humidity (%). 0 DP. */
    float temperature; /*!< Outdoor temperature (°C). 1 DP. */
    int relative_humidity; /*!< Outdoor relative humidity (%). 0 DP. */
    float dew_point; /*!< Dew point (°C). 1 DP. */
    float wind_chill; /*!< Wind chill (°C). 1 DP. */
    float apparent_temperature; /*!< Apparent temperature (°C). 1 DP. */
    float absolute_pressure; /*!< Absolute pressure (hPa). */
    float average_wind_speed; /*!< Average wind speed. */
    float gust_wind_speed; /*!< Gust wind speed. */
    char wind_direction[4]; /*!< Wind direction (N, NE, NNE, etc). */
    long download_timestamp; /*!< Time when the live data was last refreshed. UNIX time (time_t). */
} live_data_record;

/**
 * @brief wdb_set_signal_adapter sets the signal adapter to be used for
 * converting database errors into Qt Signals.
 * @param adapter The database adapter to use.
 */
void wdb_set_signal_adapter(DBSignalAdapter *adapter);

/**
 * @brief wdb_connect opens two connections to the specified server. The first
 * is used for regular database work (selects and such). The second is used to
 * listen for notifications from other systems accessing the database.
 *
 * @param target The database to connect to. Format of the string is
 *               database@hostname:port. Hostname and port are optional. If no
 *               hostname is specified then localhost is assumed.
 * @param username User to connect as.
 * @param password Password for the user account.
 * @return True if the connection was successful.
 */
bool wdb_connect(const char *target, const char *username, const char* password);

/**
 * @brief wdb_disconnect closes all database connections.
 */
void wdb_disconnect();

/* Gets the current live data */
/**
 * @brief wdb_get_live_data reads the current data from the live_data table
 * and returns it.
 *
 * @return Current conditions from the weather station.
 */
live_data_record wdb_get_live_data();

/* Checks to see if there is new live data available */
/**
 * @brief wdb_live_data_available checks for any notifications from other
 * systems on the database about the live data being updated.
 * @return True if another system has updated the live data since the last time
 * this function was called.
 */
bool wdb_live_data_available();

#endif // DATABASE_H
