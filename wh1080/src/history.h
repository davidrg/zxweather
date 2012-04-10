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
#define FIRST_RECORD_SLOT 0
#define FINAL_RECORD_SLOT 4079
#define MAX_RECORDS 4080

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
    unsigned short absolute_pressure;        /* hPa, fixed point */
    unsigned short average_wind_speed;       /* m/s, fixed point */
    unsigned short gust_wind_speed;          /* m/s, fixed point */
    unsigned char wind_direction;            /* Wind Dir */
    unsigned short total_rain;               /* Tip bucket tips */
    unsigned char status;                    /* flags */

    /* This data does not come from the weather station */
    unsigned short record_number;
    time_t download_time;
    time_t time_stamp;
    BOOL last_in_set;
} history;
#define RAIN_MULTIPLY 0.3

/* Stores multiple history records together */
typedef struct _WSHS {
    unsigned int record_count;
    history* records;
} history_set;

/* Reads a single record from the device */
history read_history_record(unsigned short record_number);

/* Reads all history from the device. These are read two at a time rather than
 * one at a time as with the read_history_record function. That makes this one
 * a little bit more efficient if you need more than one record. */
history_set read_history();

/* Reads a range of records from the weather station in chronological order.
 * If end < start then it will:
 *    read from start to end of history memory
 *    read from start of history memory to end
 * (meaning it follows the circular buffer)
 */
history_set read_history_range(const unsigned short start, const unsigned short end);

/* Frees memory allocated to a history_set */
void free_history_set(history_set hs);

/* Figure out what record ID comes next (or is previous) from the current one
 * in the circular buffer */
unsigned short previous_record(unsigned short current_record);
unsigned short next_record(unsigned short current_record);

/* The first record in the circular buffer */
unsigned short first_record();

/* This function attempts to come up with a timestamp for the latest history
 * record by waiting for the time offset field in the live record to change.
 *
 * Parameters:
 *    *current_record_id          OUT: Return parameter for the latest history
 *                                record
 *    *current_record_timestamp   OUT: The timestamp for current_record_id
 *
 * Returns:
 *    TRUE if successfull, FALSE if it was unable to come up with a timestamp
 *    for the current record. This could happen if the interval is set to
 *    something very low like 1 minute (where case as soon as the live
 *    records time offset changes its obsolete and a retry is triggered).
 */
BOOL sync_clock(unsigned short *current_record_id,
                time_t *current_record_timestamp);

/* Calculates timestamps for all records in the history set. Timestamps are
 * calculated from the last record in the set (which must have last_in_set set)
 * whose timestamp is specified by the timestamp parameter.
 */
void update_timestamps(history_set *hs, time_t timestamp);

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
