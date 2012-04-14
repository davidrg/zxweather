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

/* For sleep calls in sync_clock_r() */
#ifdef __WIN32
#include <windows.h>
#elif _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "history.h"
#include "deviceio.h"
#include "deviceconfig.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>  /* For sync_clock_r() */

#define HISTORY_OFFSET 0x00100

/* How long to wait between live record checks when attempting to sync clocks */
#define SYNC_CLOCK_WAIT_TIME 2

/* How many times to attempt clock sync */
#define SYNC_CLOCK_MAX_RETRY 5

/* This is where any logging in here will go */
FILE* history_log_file = NULL;

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

    h.total_rain = READ_SHORT(buffer, 13, 14);

    h.status = buffer[15];

    h.last_in_set = FALSE;

    return h;
}

history read_history_record(unsigned short record_number) {
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

history* load_history(unsigned short record_count) {
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

    if (hs.records == NULL)
        hs.record_count = 0;

    hs.records[hs.record_count-1].last_in_set = TRUE;

    /* We treat the last records timestamp as if the last record was actually
     * the first to be downloaded. As such, its timestamp should be time time
     * we checked which record the last record was (when we loaded the device
     * configuration).
     */
    hs.records[hs.record_count-1].download_time = last_record_timestamp;

    return hs;
}

history_set read_history_range(const unsigned short start,
                               const unsigned short end) {
    history_set hs;
    long buffer_size;
    unsigned short first_chunk_record_count;
    long first_chunk_size;
    unsigned short second_chunk_record_count = 0;
    long second_chunk_size = 0;
    unsigned long first_chunk_offset;
    unsigned char *data_buffer;
    time_t now = time(NULL);
    int record_counter;
    unsigned int i;
    int real_end = end + 1; /* Add 1 to the end so as to not miss the final
                               record */

    first_chunk_offset = HISTORY_OFFSET + (start * HISTORY_RECORD_SIZE);

    /* Work out the memory addresses and buffer sizes we need */
    if (start < real_end) {
        hs.record_count = real_end - start;
        buffer_size = hs.record_count * HISTORY_RECORD_SIZE;
        first_chunk_size = buffer_size;
        first_chunk_record_count = buffer_size / HISTORY_RECORD_SIZE;
    } else {
        /* Everything from start through to 8175 then from 0 through to end.
         * Because everything is zero based we need to add one on to the
         * record counts (otherwise we'll miss two records). */
        first_chunk_record_count = (FINAL_RECORD_SLOT - start) + 1;
        first_chunk_size = first_chunk_record_count * HISTORY_RECORD_SIZE;
        second_chunk_record_count = real_end;
        second_chunk_size = second_chunk_record_count * HISTORY_RECORD_SIZE;

        hs.record_count = first_chunk_record_count + second_chunk_record_count;
        buffer_size = first_chunk_size + second_chunk_size;
    }

    /* Allocate some memory */
    hs.records = malloc(sizeof(history) * hs.record_count);
    data_buffer = malloc(buffer_size);
    if (hs.records == NULL || data_buffer == NULL) {
        if (hs.records != NULL) free(hs.records);
        if (data_buffer != NULL) free(data_buffer);
        hs.record_count = 0;
        hs.records = NULL;
        return hs;
    }

/* Debugging Stuff *
    fprintf(log_file, "Start ID: %d\n", start);
    fprintf(log_file, "  End ID: %d\n\n", end);
    fprintf(log_file, "Chunk A Start Address: 0x%06X\n", first_chunk_offset);
    fprintf(log_file, "Chunk A Length: %d bytes (%d records)\n",
           first_chunk_size,
           first_chunk_record_count);
    if (first_chunk_size < buffer_size) {
        fprintf(log_file, "Chunk B Start Address: 0x0000\n");
        fprintf(log_file, "Chunk B Length: %d bytes (%d records)\n",
               second_chunk_size,
               second_chunk_record_count);
    }
    fprintf(log_file, "Total Buffer Size: %d bytes (%d records)\n",
           buffer_size, hs.record_count);

**/
    /* Read in first chunk */
    fill_buffer(first_chunk_offset, data_buffer, first_chunk_size, TRUE);

    /* Read in second chunk if there is one */
    if (first_chunk_size < buffer_size)
        fill_buffer(HISTORY_OFFSET,                 /* Memory Address */
                    data_buffer + first_chunk_size, /* Buffer */
                    second_chunk_size,              /* Buffer size */
                    TRUE);                          /* Validate */

    /* Convert all the data in the databuffer to history structs */
    record_counter = start;
    for (i=0; i < hs.record_count; i += 1) {
        hs.records[i] = create_history(data_buffer + (i * HISTORY_RECORD_SIZE));
        hs.records[i].download_time = now;
        hs.records[i].record_number = record_counter;
        record_counter = next_record(record_counter);
    }

    hs.records[hs.record_count-1].last_in_set = TRUE;

    /* Clean up */
    free(data_buffer);

    return hs;
}

void free_history_set(history_set hs) {
    free(hs.records);
    hs.records = NULL;
    hs.record_count = 0;
}

unsigned short previous_record(unsigned short current_record) {
    if (current_record == FIRST_RECORD_SLOT)
        return FINAL_RECORD_SLOT;
    return current_record - 1;
}

unsigned short next_record(unsigned short current_record) {
    if (current_record >= FINAL_RECORD_SLOT)
        return FIRST_RECORD_SLOT;
    return current_record + 1;
}

unsigned short first_record() {
    unsigned short total_records;
    unsigned short live_record_id;
    unsigned short first_record_id;

    get_live_record_id(&total_records,
                          NULL,   /* Don't care about the live record offset */
                          &live_record_id);

    /* If the buffer has wrapped around then the final record should be right
     * after the live record. However, as it might be overwritten by the time
     * we try to read it we'll skip over it and take the next one.
     */
    if (total_records >= MAX_RECORDS)
        first_record_id = live_record_id + 2;
    else
        first_record_id = FIRST_RECORD_SLOT;

    return first_record_id;
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

    if (history_log_file == NULL) history_log_file = stderr;

    /* So we don't keep retrying forever. */
    if (retry_count > SYNC_CLOCK_MAX_RETRY) {
        fprintf(stderr, "Failed to sync computers clock to weatherstation "
                "after %d retries.\n", SYNC_CLOCK_MAX_RETRY);
        return FALSE;
    }


    /* Figure out which is the current and which is the live record */
    get_live_record_id(&history_data_sets,  /* out */
                       &live_record_offset, /* out */
                       &live_record_id      /* out */
                      );

    fprintf(history_log_file,"Attempting to come up with a timestamp for current record %d.\n"
           "This could take a minute...\n", live_record_id - 1);

    /* Loop until we observe the live records time offset change or the
     * current history record becomes obsolete */
    while (TRUE) {
        timestamp = time(NULL);
        live = read_history_record(live_record_id);

        strftime(sbuf,50,"%c",localtime(&timestamp));
        fprintf(history_log_file,"%s: toffset %d\n", sbuf, live.sample_time);

        /* The live records time offset is the interval. This means it is no
         * longer the live record.
         *   - If this is the first time we checked the live record
         *     (prev_lstime is initialized) then the current record must have
         *     changed just after calling get_live_record_id. We will try
         *     again with the new live record.
         *   - If it has just changed from interval - 1 to interval then the
         *     live record is now the current record and we can just return it.
         */
        if (live.sample_time == interval && !prev_lstime_initialized) {
            /* What we thought was the live record isn't the live record. The
             * current record pointer probably changed just after we retrieved
             * it.*/
            fprintf(history_log_file,"Record %d is obsolete. Retrying with new live record.\n",
                   live_record_id);
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


/* Here is how this function works:
 *  rec    offset
 *  10/LIS  5      <--- this ones timestamp is the timestamp parameter
 *   9      5      rec 9 timestamp = rec 10 timestamp - rec 10 offset in seconds
 *   8      5      rec 8 timestamp = rec 9 timestamp - rec 9 offset in seconds
 *   7      5      rec 7 timestamp = rec 8 timestamp - rec 8 offset in seconds
 *   6      5
 *   5      5      <--- the offset is the number of minutes since record 4
 *   4      5
 *   3      5           Note:
 *   2      5             To start with, we only have a timestamp for the final
 *   1      5             record (rec 10) so we have to calculate everything
 *   0      5             from the 'wrong' end.
 *
 * (LIS = last_in_set == TRUE)
 */
void update_timestamps(history_set *hs, time_t timestamp) {
    int i;
    time_t this_timestamp;

    if (history_log_file == NULL) history_log_file = stderr;

    fprintf(history_log_file,"Computing timestamps...\n");

    /* Loop from the end as that is the record with the real timestamp */
    for (i = hs->record_count - 1; i >= 0; i -= 1) {
        if (hs->records[i].last_in_set) {
            hs->records[i].time_stamp = timestamp;
        } else {
            /* Each record stores the number of minutes since the previous
             * record. So, to compute the timestamp for this record (looping
             * from the newest to the oldest) we take the next newest records
             * timestamp and subtract its offset from this record. */
            this_timestamp = hs->records[i+1].time_stamp;
            this_timestamp -= hs->records[i+1].sample_time * 60;
            hs->records[i].time_stamp = this_timestamp;
        }
    }
}


void reverse_update_timestamps(history_set *hs, time_t timestamp) {
    unsigned int i;
    time_t this_timestamp;

    if (history_log_file == NULL) history_log_file = stderr;

    /* Loop from the start as that is the record with the real timestamp */
    for (i = 0; i < hs->record_count; i += 1) {

        if (i == 0) {
            hs->records[i].time_stamp = timestamp;
        } else {
            /* Each record stores the number of minutes since the previous
             * record. So, to compute the timestamp for t2his record (looping
             * from the newest to the oldest) we take the next newest records
             * timestamp and subtract its offset from this record. */
            this_timestamp = hs->records[i-1].time_stamp;
            this_timestamp += hs->records[i-1].sample_time * 60;
            hs->records[i].time_stamp = this_timestamp;
        }
    }
}
