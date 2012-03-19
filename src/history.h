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

#include <time.h>
#include "common.h"

#define HISTORY_RECORD_SIZE 16

/* A single data sample from the weather station
 * record_number: This is not a field that the weather station stores but
 *                rather the 'slot' the record was stored in the weather
 *                stations memory.
 * download_time: When this particular history struct was created according
 *                to the computers clock. If last_in_set is set then the value
 *                of this field will actually be the time that the last record
 *                was determined (when the ID of the last record was looked up
 *                in the device configuration structures)
 * last_in_set: If this history record was the last in a history set. If this
 *              is set then the download_time on this history record is
 *              probably fairly close to the stations time for that record.
 * total_rain: Multiply this by 0.3 to get the real rainfall. This counter
 *             actually counts the number of times the tipping bucket rain
 *             gauges bucket has tipped (which requires 0.3mm of rain to do).
 */
typedef struct _WSH {
    unsigned char sample_time;               /* minute */
    unsigned char indoor_relative_humidity;  /* % */
    signed short indoor_temperature;         /* C, fixed point */
    unsigned char outdoor_relative_humidity; /* % */
    signed short outdoor_temperature;        /* C, fixed point */
    unsigned short absolute_pressure;        /* Hpa, fixed point */
    unsigned short average_wind_speed;       /* m/s, fixed point */
    unsigned short gust_wind_speed;          /* m/s, fixed point */
    unsigned char wind_direction;            /* Wind Dir */
    unsigned short total_rain;               /* Tip bucket tips */
    unsigned char status;                    /* flags */

    /* This data does not come from the weather station */
    unsigned short record_number;
    time_t download_time;
    BOOL last_in_set;
} history;
#define RAIN_MULTIPLY 0.3

/* Stores multiple history records together */
typedef struct _WSHS {
    unsigned int record_count;
    history* records;
} history_set;

/* Reads a single record from the device */
history read_history_record(int record_number);

/* Reads all history from the device. These are read two at a time rather than
 * one at a time as with the read_history_record function. That makes this one
 * a little bit more efficient if you need more than one record. */
history_set read_history();

/* Frees memory allocated to a history_set */
void free_history_set(history_set hs);

/* History record status flags */
#define H_SF_RESERVED_A        0x01 /* Reserved A */
#define H_SF_RESERVED_B        0x02 /* Reserved B */
#define H_SF_RESERVED_C        0x04 /* Reserved C */
#define H_SF_RESERVED_D        0x08 /* Reserved D */
#define H_SF_RESERVED_E        0x10 /* Reserved E */
#define H_SF_RESERVED_F        0x20 /* Reserved F */
#define H_SF_INVALID_DATA      0x40 /* If set, no sensor data received */
#define H_SF_RAINFALL_OVERFLOW 0x80 /* If set then rainfall overflow */

#endif /* HISTORY_H */
