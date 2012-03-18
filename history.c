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

    h.sample_time = buffer[0];
    h.indoor_relative_humidity = buffer[1];
    h.indoor_temperature = READ_SSHORT(buffer, 2, 3);
    h.outdoor_relative_humidity = buffer[4];
    h.outdoor_temperature = READ_SSHORT(buffer, 5, 6);
    h.absolute_pressure = READ_SHORT(buffer, 7, 8);

    /* average wind speed - [9] and part of [11]
     gust wind speed - [10] and part of [11] */

    h.wind_direction = buffer[12];
    h.total_rain = buffer[13];
    h.status = buffer[14];

    return h;
}

history read_history_record(int record_number) {
    unsigned char buffer[16];
    unsigned int record_offset = HISTORY_OFFSET + (record_number * 16);

    fill_buffer(record_offset, buffer, HISTORY_RECORD_SIZE, TRUE);

    return create_history(buffer);
}

history* load_history(int record_count) {
    history* h = NULL;
    int i = 0;
    unsigned char * data_buffer;
    int buffer_size = HISTORY_RECORD_SIZE * record_count;

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
    fill_buffer(HISTORY_OFFSET, data_buffer, buffer_size, TRUE);

    /* Load each history record */
    for (i = 0; i < record_count; i += 1) {
        h[i] = create_history(data_buffer + (i * HISTORY_RECORD_SIZE));
    }

    free(data_buffer);

    return h;
}

history_set read_history() {
    device_config dc;
    history_set hs;

    dc = load_device_config();

    hs.record_count = dc.history_data_sets;
    hs.records = load_history(hs.record_count);

    if (hs.records == NULL)
        hs.record_count = 0;

    return hs;
}

void free_history_set(history_set hs) {
    free(hs.records);
    hs.records = NULL;
    hs.record_count = 0;
}
