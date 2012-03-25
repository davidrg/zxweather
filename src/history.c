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

/* For sleep function */
#ifdef __WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "history.h"
#include "deviceio.h"
#include "deviceconfig.h"
#include "common.h"
#include <stdlib.h>

#define HISTORY_OFFSET 0x00100

/* How long to wait between live record checks when attempting to sync clocks */
#define SYNC_CLOCK_WAIT_TIME 2

/* How many times to attempt clock sync */
#define SYNC_CLOCK_MAX_RETRY 5

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

/* Reads *all* history records in memory-order */
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

unsigned short previous_record(unsigned short current_record) {
    if (current_record == 0)
        return 8175;
    return current_record - 1;
}

unsigned short next_record(unsigned short current_record) {
    if (current_record >= 8175)
        return 0;
    return current_record + 1;
}

/* This function attempts to come up with a timestamp for the latest history
 * record by waiting for the time offset field in the live record to change.
 *
 * Parameters:
 *    *current_record_id          OUT: Return parameter for the latest history
 *                                record
 *    *current_record_timestamp   OUT: The timestamp for current_record_id
 *    retry_count                 IN: To control the maximum number of retries.
 *
 * Returns:
 *    TRUE if successfull, FALSE if it was unable to come up with a timestamp
 *    for the current record. This could happen if the interval is set to
 *    something very low like 1 minute (where case as soon as the live
 *    records time offset changes its obsolete and a retry is triggered).
 */
BOOL sync_clock_r(unsigned short *current_record_id,
                  time_t *current_record_timestamp,
                  unsigned short retry_count) {
    unsigned short history_data_sets, live_record_offset, live_record_id;
    history live;
    time_t timestamp;
    BOOL prev_lstime_initialized = FALSE;
    unsigned char prev_lstime = 0;
    unsigned char interval = get_interval();
    char sbuf[50];

    /* So we don't keep retrying forever. */
    if (retry_count > SYNC_CLOCK_MAX_RETRY) {
        fprintf(stderr, "Failed to sync computers clock to weatherstation "
                "after %d retries.\n", SYNC_CLOCK_MAX_RETRY);
        return FALSE;
    }


    /* Figure out which is the current and which is the live record */
    get_current_record_id(&history_data_sets,  /* out */
                          &live_record_offset, /* out */
                          &live_record_id      /* out */
                          );

    printf("Attempting to come up with a timestamp for current record %d.\n"
           "This could take a minute...\n", live_record_id - 1);

    /* Loop until we observe the live records time offset change or the
     * current history record becomes obsolete */
    while (TRUE) {
        timestamp = time(NULL);
        live = read_history_record(live_record_id);

        strftime(sbuf,50,"%c",localtime(&timestamp));
        printf("%s: toffset %d\n", sbuf, live.sample_time);

        /* The live records time offset is the interval. This means it is no
         * longer the live record.
         *   - If this is the first time we checked the live record
         *     (prev_lstime is initialized) then the current record must have
         *     changed just after calling get_current_record_id. We will try
         *     again with the new live record.
         *   - If it has just changed from interval - 1 to interval then the
         *     live record is now the current record and we can just return it.
         */
        if (live.sample_time == interval && !prev_lstime_initialized) {
            /* What we thought was the live record isn't the live record. The
             * current record pointer probably changed just after we retrieved
             * it.*/
            printf("Record %d is obsolete. Retrying with new live record.\n");
            return sync_clock_r(current_record_id,          /* out */
                                current_record_timestamp,   /* out */
                                retry_count + 1);    /* to limit retries */
        } else if (live.sample_time == interval &&
                   prev_lstime == interval - 1) {
            /* The live record has just become the current history record. That
             * is good enough for us. */
            *current_record_id = live_record_id;
            /* Timestamp doesn't need adjusting as the live record only just
             * became the current history record (making its timestamp *now*) */
            *current_record_timestamp = timestamp;
            return TRUE;
        }


        /* We don't have a previous sample time to compare with */
        if (!prev_lstime_initialized) {
            prev_lstime = live.sample_time;
            prev_lstime_initialized = TRUE;
        }

        /* The sample time on the live record has just changed */
        if (prev_lstime < live.sample_time) {
            *current_record_id = previous_record(live_record_id);
            /* live.sample_time is the number of minutes since the current
             * history record. timestamp is seconds since since the epoch.*/
            *current_record_timestamp = timestamp - (live.sample_time * 60);
            return TRUE;
        }

#ifdef __WIN32
        /* The Win32 function is in miliseconds (POSIX is in seconds) */
        Sleep(SYNC_CLOCK_WAIT_TIME * 1000);
#else
        sleep(SYNC_CLOCK_WAIT_TIME);
#endif
    }
}

/* This function attempts to come up with a timestamp for the latest history
 * record by waiting for the time offset field in the live record to change.
 */
BOOL sync_clock(unsigned short *current_record_id,
                time_t *current_record_timestamp) {
    return sync_clock_r(current_record_id, current_record_timestamp, 0);
}
