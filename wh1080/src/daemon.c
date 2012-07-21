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

/* Try and figure out what platform we're targeting */
#if defined _WIN32 || defined __WIN32 || defined __TOS_WIN__ || \
    defined __WINDOWS__
/* __WIN32__ is used by Borland compilers,
 * __TOS_WIN__ is used by IBM xLC
 * __WINDOWS__ is defined by Watcom compilers
 * The rest are fairly standard used by MSVC, GCC, etc.
 */
#define ZXW_OS_WINDOWS
#else /* default is unix */
#define ZXW_OS_UNIX
#endif

/* For sleep calls */
#ifdef ZXW_OS_WINDOWS
#include <windows.h>    /* Sleep() */
#else
#include <unistd.h>     /* sleep() (note the small s) */
#endif

#include "daemon.h"
#include "deviceio.h"
#include "deviceconfig.h"
#include "history.h"
#include "pgout.h"

/* Number of seconds between live data updates on the device. */
#define LIVE_UPDATE_INTERVAL 48

void wait_for_next_live(FILE *logfile);
void setup(char *server, char *username, char *password, FILE *logfile);
void main_loop(FILE* logfile,
               unsigned short initial_current_record_id,
               time_t clock_sync_current_ts);

/* Main function for daemon functionality.
 *
 * Process for this function is:
 *  1. Connect to the device
 *  2. Connect to the database server
 *  3. Attempt to figure timestamp for current history record
 *  4. Record current timestamp for future sleep calculations
 *  5. Start main loop.
 */
void daemon_main(char *server, char *username, char *password, FILE *log_file) {

    time_t clock_sync_current_ts;     /* Timestamp from clock_syc */
    unsigned short current_record_id; /* Current record on the device */
    /* misc */
    BOOL result;

    fprintf(log_file, "Daemon started.\n");

    setup(server, username, password, log_file); /* Connect to device, db, etc */

    /* This will give us the current record and its timestamp */
    result = sync_clock(&current_record_id,     /* OUT */
                        &clock_sync_current_ts  /* OUT */
                        );

    /* as soon as sync_clock() observes a new live record it will do a few
     * small calculations and return. So the next live record should be
     * around 48 seconds after sync_clock() returns. The first time
     * wait_for_next_live() is called it will add 48 onto the current time
     * to compute when the next live record is due and then return immediately
     * instead of actually waiting. */
    wait_for_next_live(log_file);

    main_loop(log_file, current_record_id, clock_sync_current_ts);
}


/* Main program loop. Handles downloading new data:
 *
 *  1. Load any new history records into the database
 *  2. Update live data record in the database
 *  3. Sleep for 48 seconds
 *  4. Go to 1
 *
 * Parameters are:
 *  logfile:  The stream to write messages to
 *  initial_current_record_id: The current record to start from. This value
 *            comes from sync_clock().
 *  clock_sync_current_ts: Current timestamp from the clock_sync() function.
 */
void main_loop(FILE* logfile,
               unsigned short initial_current_record_id,
               time_t clock_sync_current_ts) {
    /* This is the live record. We will fetch this every 48 seconds */
    unsigned short live_record_id;
    history live_record;

    /* For dealing with history records */
    unsigned short range_start_id;    /* First history record to download */
    unsigned short latest_record_id;  /* Latest record in the DB */
    unsigned short current_record_id; /* Current record on the device */
    history_set hs;                   /* History downloaded from the device */
    time_t first_record_ts;           /* Calculated timestamp of the first hs
                                       * record */
    time_t final_record_ts = 0;       /* Final record from previous hs */

    time_t database_ts;               /* Timestamp from latest DB record */

    current_record_id = initial_current_record_id;

    /* Loop forever waking up ever 48 seconds to grab live data and any
     * new history records. Any cleanup required will be done when we receive
     * a SIGTERM. */
    while (TRUE) {
        /* Find and broadcast the live record */
        get_live_record_id(NULL, NULL, &live_record_id);
        live_record = read_history_record(live_record_id);
        pgo_update_live(live_record);
        fprintf(logfile, "LIVE is #%d\n", live_record_id);

        /* If we have a clock_sync_current_ts then current_record_id is already
         * set and valid thanks to clock_sync(). */
        if (clock_sync_current_ts == 0)
            current_record_id = previous_record(live_record_id);

        fprintf(logfile, "CURRENT is #%d\n", current_record_id);

        pgo_get_last_record_number(&latest_record_id, &database_ts);

        /* Download any history records that have appeared. */
        if (final_record_ts == 0 || current_record_id > latest_record_id) {
            /* final_record_ts == 0  means the database is empty.
             * The current > latest check will fail when a wrap around occurs
             * but the new records will be picked up next time. */

            if (database_ts == 0 && latest_record_id == 0)
                range_start_id = 0; /* Database is empty. Get everything. */
            else /* Otherwise, don't duplicate the latest DB record */
                range_start_id = next_record(latest_record_id);

            /* There are new history records to load into the database */
            fprintf(logfile, "Download history records %d to %d...\n",
                    range_start_id, current_record_id);
            hs = read_history_range(range_start_id,
                                    current_record_id);

            /**** Timestamp Calculations ****/

            if (clock_sync_current_ts != 0 || final_record_ts == 0) {
                /* Either the database is empty or we've just started. Either
                 * way we use the current record timestamp we just calculated.*/
                update_timestamps(&hs,clock_sync_current_ts);

                /* Then we throw it away. From now on we calculate timestamps
                 * based on the most recent sample record */
                clock_sync_current_ts = 0;
            } else {
                /* Calculate timestamps. To do this we must figure out the time
                 * of the first record in the history set. We can do this by adding
                 * its interval onto the timestamp of the final record from the
                 * previous history set. Note time_t is in seconds and the
                 * interval is in minutes. */
                first_record_ts = final_record_ts + (hs.records[0].sample_time * 60);
                reverse_update_timestamps(&hs, first_record_ts);
            }

            /* We will calculate the next set of history records from this */
            final_record_ts = hs.records[hs.record_count-1].time_stamp;

            /**** END: Timestamp Calculations ****/

            pgo_insert_history_set(hs);
            pgo_commit();
            free_history_set(hs);
        }

        pgo_updates_complete();

        wait_for_next_live(logfile);
        fprintf(logfile, "WAKE!\n");
    }
}

/* Setup the connection, log file, etc */
void setup(char *server, char *username, char *password, FILE *logfile) {
    extern FILE* history_log_file;
    extern FILE* database_log_file;

    /* This is where any log messages from history.c go to */
    history_log_file = logfile;

    /* Redirect error messages from pgout.pgc */
    database_log_file = logfile;

    fprintf(logfile, "Open Device...\n");
    open_device();

    fprintf(logfile, "Connect to Database...\n");
    pgo_connect(server, username, password);
}

/* Sleeps until the next live record is due. The first time this function is
 * called it initialises some static variables and returns immediately */
void wait_for_next_live(FILE* logfile) {
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

        fprintf(logfile, "Missed live. Recalculating...\n");

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
        fprintf(logfile, "No need for sleep\n");
        next_live_due += LIVE_UPDATE_INTERVAL;
        return;
    } else {
        sleep_time = next_live_due - now;
    }

    if (sleep_time > 60)
        fprintf(logfile,"WARNING: Sleep time is %d (should be ~48).\n",
                sleep_time);

    fprintf(logfile, "Current time is %d\n", now);
    fprintf(logfile, "Next live due at %d\n", next_live_due);
    fprintf(logfile, "Sleep for %d seconds\n", sleep_time);

    /* Sleep for a while */
#ifdef ZXW_OS_WINDOWS
    /* The Win32 function is in miliseconds (POSIX is in seconds) */
    Sleep(sleep_time * 1000);
#else
    sleep(sleep_time);
#endif
    next_live_due += LIVE_UPDATE_INTERVAL;
}

void cleanup() {
    close_device();
    pgo_disconnect();
}
