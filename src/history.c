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

#include "history.h"
#include "deviceio.h"
#include "deviceconfig.h"
#include "common.h"
#include <stdlib.h>

#define HISTORY_OFFSET 0x00100

history create_history(unsigned char* buffer) {
    history h;
    unsigned char gust_nibble, average_nibble;
    unsigned int gust_bit, average_bit;

    h.sample_time = buffer[0];
    h.indoor_relative_humidity = buffer[1];
    h.indoor_temperature = READ_SSHORT(buffer, 2, 3);
    h.outdoor_relative_humidity = buffer[4];
    h.outdoor_temperature = READ_SSHORT(buffer, 5, 6);
    h.absolute_pressure = READ_SHORT(buffer, 7, 8);

    /* average wind speed - [9] and part of [11]
     gust wind speed - [10] and part of [11] */
    gust_nibble = buffer[11] >> 4;
    average_nibble = buffer[11] & 0x0F;
    gust_bit = gust_nibble << 8;
    average_bit = average_nibble << 8;

    h.average_wind_speed = buffer[9] + average_bit;
    h.gust_wind_speed = buffer[10] + gust_bit;

    h.wind_direction = buffer[12];

    h.total_rain = buffer[13];

    h.status = buffer[14];

    h.last_in_set = FALSE;

    return h;
}

history read_history_record(int record_number) {
    unsigned char buffer[16];
    unsigned int record_offset = HISTORY_OFFSET + (record_number * 16);
    history h;
    time_t download_time;

    download_time = time(NULL);
    fill_buffer(record_offset, buffer, HISTORY_RECORD_SIZE, TRUE);

    h = create_history(buffer);
    h.record_number = record_number;
    h.download_time = download_time;
    h.last_in_set = FALSE; /* Not part of a history set */

    return h;
}

history* load_history(int record_count) {
    history* h = NULL;
    int i = 0;
    unsigned char * data_buffer;
    int buffer_size = HISTORY_RECORD_SIZE * record_count;
    time_t download_time;

    /* Allocate space for the history set */
    h = malloc(sizeof(history) * record_count);
    if (h == NULL) return h;

    /* Read in all the data from the device. This will be 16 bytes of data
     * for each history record. */
    data_buffer = malloc(buffer_size);
    if (data_buffer == NULL) {
        /* Failed to allocate the memory. Free the history set and give up */
        free(h);
        return NULL;
    }
    download_time = time(NULL);
    fill_buffer(HISTORY_OFFSET, data_buffer, buffer_size, TRUE);

    /* Load each history record */
    for (i = 0; i < record_count; i += 1) {
        h[i] = create_history(data_buffer + (i * HISTORY_RECORD_SIZE));
        h[i].record_number = i;
        h[i].download_time = download_time;
    }

    free(data_buffer);

    return h;
}

history_set read_history() {
    device_config dc;
    history_set hs;
    time_t last_record_timestamp;

    last_record_timestamp = time(NULL);
    dc = load_device_config();

    hs.record_count = dc.history_data_sets;
    hs.records = load_history(hs.record_count);

    hs.records[hs.record_count-1].last_in_set = TRUE;

    /* We treat the last records timestamp as if the last record was actually
     * the first to be downloaded. As such, its timestamp should be time time
     * we checked which record the last record was (when we loaded the device
     * configuration).
     */
    hs.records[hs.record_count-1].download_time = last_record_timestamp;

    if (hs.records == NULL)
        hs.record_count = 0;

    return hs;
}

void free_history_set(history_set hs) {
    free(hs.records);
    hs.records = NULL;
    hs.record_count = 0;
}
