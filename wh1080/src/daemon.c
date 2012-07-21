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
unsigned short update_live_data(FILE* logfile);
void calculate_timestamps(time_t clock_sync_time, history_set *hs);
void wait_for_new_sample(FILE* logfile);
BOOL check_for_station_reset(FILE* logfile);
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

    /* Make sure the device hasn't been reset since we last ran. wh1080d can't
     * be started on a freshly reset device */
    if (check_for_station_reset(log_file)) {
        fprintf(log_file, "Fatal error detected. Terminating.\n");
        return;
    }

    /* Make sure there is at least one new record on the device. If there isn't
     * then wait for one. */
    wait_for_new_sample(log_file);

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

/* Checks to see if the station has been reset since we last started. This is
 * a fatal error for the daemon - it cannot be started on a freshly reset
 * device.
 */
BOOL check_for_station_reset(FILE* logfile) {

    unsigned short history_data_sets;   /* Number of records on the device */
    unsigned short live_record_id;      /* Live data record ID */
    unsigned short current_record_id;   /* Latest sample ID */
    unsigned short db_latest_id;        /* Latest sample ID in the database */
    time_t db_latest_ts;                /* Timestamp of above */

    fprintf(logfile, "Checking for station reset condition...");
    pgo_get_last_record_number(&db_latest_id, &db_latest_ts);
    get_live_record_id(&history_data_sets, NULL, &live_record_id);
    current_record_id = previous_record(live_record_id);

    if (history_data_sets < MAX_RECORDS) {
        /* The stations memory is not full so its been reset as recently as
         * within the last two weeks (could be longer depending on its sample
         * interval. */

        if (current_record_id < db_latest_id) {
            /* The device is not full (so there can't have been a wrap-around)
             * and the stations highest record ID is less that the databases.
             * This means that the device must have been reset
             */
            fprintf(logfile, "\nFatal Error: wh1080d cannot be restarted after "
                    "a device reset. Consult\ninstallation reference manual "
                    "for maintenance procedure to clear error\ncondition.\n");
            return TRUE;
        }
    }

    fprintf(logfile, "OK\n");
    return FALSE;
}

/* This function checks to see if there is new data on the device. If there is
 * then it returns immediately. If there is not then it waits for new data to
 * appear before returning.
 *
 * Its purpose is to ensure that the sync_clock function returns an ID for a
 * new record as the daemon can not start on old data.
 */
void wait_for_new_sample(FILE* logfile) {
    unsigned short history_data_sets; /* Number of records on the device */
    unsigned short live_record_id;    /* ID of the current live record */
    time_t db_latest_ts;              /* Timestamp of the latest DB record */
    unsigned short db_latest_id;      /* ID of latest DB record */
    unsigned short station_current_id; /* ID of current record on station */

    unsigned int loop_counter = 0;

    fprintf(logfile, "Waiting for new data before attempting clock sync...\n");

    while (TRUE) {
        pgo_get_last_record_number(&db_latest_id, &db_latest_ts);
        get_live_record_id(&history_data_sets, NULL, &live_record_id);

        station_current_id = previous_record(live_record_id);

        if (station_current_id != db_latest_id) {
            /* The device and DB have different opinions of what is the latest
             * sample. This means there is probably new data to download.
             */
            fprintf(logfile, "\n"); /* Keep the log file tidy */
            return;
        } else {
            fprintf(logfile, "%d...", loop_counter);
            loop_counter = loop_counter + 1;
        }


        /* No new data yet. Wait for 48 seconds or so and check again. */
#ifdef ZXW_OS_WINDOWS
        /* The Win32 function is in miliseconds (POSIX is in seconds) */
        Sleep(LIVE_UPDATE_INTERVAL * 1000);
#else
        sleep(LIVE_UPDATE_INTERVAL);
#endif
    }
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
 *            comes from sync_clock(). It *must* be a record that does not
 *            exist in the database (that is, it must be a new record).
 *  clock_sync_current_ts: Current timestamp from the clock_sync() function.
 */
void main_loop(FILE* logfile,
               unsigned short initial_current_record_id,
               time_t clock_sync_current_ts) {
    /* This is the live record. We will fetch this every 48 seconds */
    unsigned short live_record_id;

    /* For dealing with history records */
    unsigned short range_start_id;    /* First history record to download */
    unsigned short latest_record_id;  /* Latest record in the DB */
    unsigned short current_record_id; /* Current record on the device */
    history_set hs;                   /* History downloaded from the device */

    time_t database_ts;               /* Timestamp from latest DB record */

    BOOL first_run = TRUE;            /* If the clock_sync data is still ok */

    /* Use the current record calculated by sync_clock() initially as that has
     * a known timestamp that we can work from later. */
    current_record_id = initial_current_record_id;

    /* Loop forever waking up ever 48 seconds to grab live data and any
     * new history records. Any cleanup required will be done when we receive
     * a SIGTERM. */
    while (TRUE) {
        /* Update the live data in the database. */
        live_record_id = update_live_data(logfile);

        /* If we have a clock_sync_current_ts then current_record_id is already
         * set and valid thanks to clock_sync(). This is only valid for the
         * first lot of samples downloaded. */
        if (!first_run)
            current_record_id = previous_record(live_record_id);

        fprintf(logfile, "CURRENT is #%d\n", current_record_id);

        pgo_get_last_record_number(&latest_record_id, &database_ts);

        /* Download any history records that have appeared.
         * current_record_id is the latest non-live record on the device
         * latest_record_id is the latest record in the database.
         */
        if (database_ts == 0 || current_record_id != latest_record_id) {
            /* if database_ts == 0 then the database is empty and we need to
             * download everything.
             *
             * If current != latest then there could be new data */

            if (database_ts == 0 && latest_record_id == 0)
                range_start_id = 0; /* Database is empty. Get everything. */
            else  {
                /* There is new data. If the device has actually been reset
                 * then that should have been caught on startup and reported
                 * as an error. */

                /* Start from one after what ever is most recent in the DB */
                range_start_id = next_record(latest_record_id);
            }

            /* There are new history records to load into the database */
            fprintf(logfile, "Download history records %d to %d...\n",
                    range_start_id, current_record_id);
            hs = read_history_range(range_start_id,
                                    current_record_id);

            calculate_timestamps(clock_sync_current_ts, &hs);

            pgo_insert_history_set(hs);
            pgo_commit();
            free_history_set(hs);

            first_run = FALSE; /* mark the clock_sync stuff as invalid. */
        }

        pgo_updates_complete();

        wait_for_next_live(logfile);
        fprintf(logfile, "WAKE!\n");
    }
}

/* Calculates timestamps for the history set using either the sync_clock time
 * stamp or the timestamp calculated by a previous call.
 */
void calculate_timestamps(time_t clock_sync_time, history_set *hs) {
    /* The timestamp we calculated last time. On first call the clock_sync_time
     * will be used for calculations instead. */
    static time_t previous_timestamp = 0;

    /* Calculated timestamp for first record in history set */
    time_t first_record_ts;

    if (clock_sync_time != 0 && previous_timestamp == 0) {
        /* This is the first call so we'll calculate the time using the
         * value produced by sync_clock(). This works on the assumption that
         * the first record in the history set is the same record that
         * sync_clock calculated a timestamp for. */

        update_timestamps(hs,clock_sync_time);
    } else {
        /* Calculate timestamps. To do this we must figure out the time
         * of the first record in the history set. We can do this by adding
         * its interval onto the timestamp of the final record from the
         * previous history set. Note time_t is in seconds and the
         * interval is in minutes. */

        first_record_ts = previous_timestamp + (hs->records[0].sample_time * 60);
        reverse_update_timestamps(hs, first_record_ts);
    }

    /* We will calculate the next set of history records from this */
    previous_timestamp = hs->records[hs->record_count-1].time_stamp;
}

/* Updates the live data in the database and returns the ID of the live
 * data record. */
unsigned short update_live_data(FILE* logfile) {
    unsigned short live_record_id;
    history live_record;

    get_live_record_id(NULL, NULL, &live_record_id);
    live_record = read_history_record(live_record_id);
    pgo_update_live(live_record);
    fprintf(logfile, "LIVE is #%d\n", live_record_id);

    return live_record_id;
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
