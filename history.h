/*****************************************************************************
 *            Created: 18/03/2012
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

#ifndef HISTORY_H
#define HISTORY_H

#define HISTORY_RECORD_SIZE 16

typedef struct _WSH {
    unsigned char sample_time;
    unsigned char indoor_relative_humidity;
    signed short indoor_temperature;
    unsigned char outdoor_relative_humidity;
    signed short outdoor_temperature;
    unsigned short absolute_pressure;
    signed short average_wind_speed;
    signed short gust_wind_speed;
    unsigned char wind_direction;
    unsigned short total_rain;
    unsigned char status;
} history;

typedef struct _WSHS {
    unsigned int record_count;
    history* records;
} history_set;

history read_history_record(int record_number);

history_set read_history();

void free_history_set(history_set hs);

#endif /* HISTORY_H */
