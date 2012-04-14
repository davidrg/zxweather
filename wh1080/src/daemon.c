/*****************************************************************************
 *            Created: 10/04/2012
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

/* For sleep calls */
#ifdef __WIN32
#include <windows.h>    /* Sleep() */
#elif _MSC_VER
#include <windows.h>    /* Sleep() */
#else
#include <unistd.h>     /* sleep() (note the small s) */
#endif

#include "daemon.h"
#include "deviceio.h"
#include "deviceconfig.h"
#include "history.h"
#include "pgout.h"

#define LIVE_UPDATE_INTERVAL 48

FILE *logfile;

void wait_for_next_live();
void setup(char *server, char *username, char *password, FILE *logfile);

/* Main function for daemon functionality.
 *
 * Process for this function is:
 *  1. Connect to the device
 *  2. Connect to the database server
 *  3. Attempt to figure timestamp for current history record
 *  4. Load any new history records into the database
 *  5. Update live data record in the database
 *  6. Sleep for 48 seconds
 *  7. Go to 4
 */
void daemon(char *server, char *username, char *password, FILE *log_file) {
    /* This is the live record. We will fetch this every 48 seconds */
    unsigned short live_record_id;
    history live_record;

    /* For dealing with history records */
    unsigned short latest_record_id;  /* Latest record in the DB */
    unsigned short current_record_id; /* Current record on the device */
    history_set hs;                   /* History downloaded from the device */
    time_t first_record_ts;           /* Calculated timestamp of the first hs
                                       * record */
    time_t final_record_ts;           /* Final record from previous hs */
    time_t clock_sync_current_ts;     /* Timestamp from clock_syc */

    /* misc */
    BOOL result;

    logfile = log_file;

    setup(server, username, password, logfile); /* Connect to device, db, etc */

    /* This will give us the current record and its timestamp */
    result = sync_clock(&current_record_id, &clock_sync_current_ts);

    /* as soon as sync_clock() observes a new live record it will do a few
     * small calculations and return. So the next live record should be
     * around 48 seconds after sync_clock() returns. The first time
     * wait_for_next_live() is called it will add 48 onto the current time
     * to compute when the next live record is due and then return immediately
     * instead of actually waiting. */
    wait_for_next_live();

    /* Loop forever waking up ever 48 seconds to grab live data and any
     * new history records. Any cleanup required will be done when we receive
     * a SIGTERM. */
    while (TRUE) {
        /* Find and broadcast the live record */
        get_live_record_id(NULL, NULL, &live_record_id);
        live_record = read_history_record(live_record_id);
        pgo_update_live(live_record);

        /* If we have a clock_sync_current_ts then current_record_id is already
         * set and valid thanks to clock_sync(). */
        if (clock_sync_current_ts == 0)
            current_record_id = live_record_id - 1;

        pgo_get_last_record_number(&latest_record_id, &final_record_ts);

        /* Download any history records that have appeared. */
        if (final_record_ts == 0 || current_record_id > latest_record_id) {
            /* final_record_ts == 0  means the database is empty */

            /* There are new history records to load into the database */
            fprintf(logfile, "Download history records %d to %d...",
                    latest_record_id, current_record_id);
            hs = read_history_range(latest_record_id, current_record_id);

            if (final_record_ts != 0) {
                /* Calculate timestamps. To do this we must figure out the time
                 * of the first record in the history set. We can do this by adding
                 * its interval onto the timestamp of the final record from the
                 * previous history set. Note time_t is in seconds and the
                 * interval is in minutes. */
                first_record_ts = final_record_ts + (hs.records[0].sample_time * 60);
                reverse_update_timestamps(&hs, first_record_ts);
                final_record_ts = hs.records[hs.record_count-1].time_stamp;
            } else {
                /* The database is empty. We must calculate timestamps from
                 * the other direction (using the timestamp of the current
                 * weather station record) */
                if (clock_sync_current_ts != 0) {
                    fprintf(logfile, "Database empty. Performing full load.\n");

                    clock_sync_current_ts = 0; /* Not valid anymore */
                } else {
                    fprintf(logfile, "ERROR: Database appears to be empty again.\n");
                    return;
                }
            }

            pgo_insert_history_set(hs);
            pgo_commit();
            free_history_set(hs);
        }

        wait_for_next_live();
    }
}

/* Setup the connection, log file, etc */
void setup(char *server, char *username, char *password, FILE *logfile) {
    extern FILE* history_log_file;

    /* This is where any log messages from history.c go to */
    history_log_file = logfile;

    fprintf(logfile, "Open Device...\n");
    open_device();

    fprintf(logfile, "Connect to Database...\n");
    pgo_connect(server, username, password);
}

/* Sleeps until the next live record is due. The first time this function is
 * called it initialises some static variables and returns immediately */
void wait_for_next_live() {
    static time_t next_live_due = 0;
    static time_t now;
    static unsigned long sleep_time;
    static unsigned long fixup;

    /* On first call calculate next live due time for use in subsequent
     * calls */
    if (next_live_due == 0) {
        next_live_due = time(NULL) + LIVE_UPDATE_INTERVAL;
        return;
    }

    /* Figure out when the next live record is due */
    now = time(NULL);
    if (now > next_live_due) {
        /* We missed the live record. Figure out when the next one should
         * be due.*/

        /* This is how far behind we are */
        fixup = now - next_live_due;

        /* Make sure what ever we increment next_live_due by is a multiple
         * of the live update interval */
        if (fixup % LIVE_UPDATE_INTERVAL)
            fixup = fixup + (LIVE_UPDATE_INTERVAL - fixup % LIVE_UPDATE_INTERVAL);

        next_live_due += fixup;

    }

    if (now == next_live_due) {
        /* Live record is due now. No need to sleep. */
        return;
    } else {
        sleep_time = next_live_due - now;
    }

    if (sleep_time > 60)
        fprintf(logfile,"WARNING: Sleep time is %d (should be ~48).\n",
                sleep_time);

    /* Sleep for a while */
#ifdef __WIN32
    /* The Win32 function is in miliseconds (POSIX is in seconds) */
    Sleep(sleep_time * 1000);
#else
    sleep(sleep_time);
#endif
}

void cleanup() {
    close_device();
    pgo_disconnect();
}
