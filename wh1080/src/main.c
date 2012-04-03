/*****************************************************************************
 *            Created: 17/03/2012
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "deviceio.h"
#include "deviceconfig.h"
#include "conout.h"
#include "fileout.h"
#include "history.h"
#include "pgout.h"

/* Use the bundled version of getopt on non-POSIX systems */
#ifdef __WIN32
#include "getopt/getopt.h"
#else
#include <unistd.h>
#endif

/* Determine latest record in database */
BOOL find_range_start(unsigned short *range_start,
                      const unsigned short current_ws_record_id) {
    /* Latest history record available from the database */
    unsigned int current_db_record_id;
    time_t current_db_record_timestamp;
    /*history current_db_record_from_ws;*/

    pgo_get_last_record_number(&current_db_record_id,
                               &current_db_record_timestamp);
    if (current_db_record_timestamp != 0) {

        /* Figure out if that record exists in the weather station and if so,
         * double-check that its the same as whats in the database */
        /*current_db_record_from_ws = read_history_record(current_db_record_id);*/

        *range_start = next_record(current_db_record_id);

        if (current_db_record_id == current_ws_record_id)
            return FALSE;
        return TRUE;
    }

    /* Database is empty. Fetch everything from the weather station */
    printf("Database is empty.\n");
    *range_start = first_record();
    return TRUE;
}

void insert_history_range(const unsigned short start,
                          const unsigned short end,
                          const time_t end_timestamp) {
    history_set new_data;   /* Data to load into database */

    /* Fetch the records from the weather station & compute timestamps */
    printf("Fetching records %d to %d...\n", start, end);
    new_data = read_history_range(start, end);
    update_timestamps(&new_data, end_timestamp);

    /* Insert history data into database */
    printf("Inserting records into database...\n");
    pgo_insert_history_set(new_data);
    free_history_set(new_data);
}

int db_update(char* server,
              char* username,
              char* password,
              const BOOL full_load) {
    unsigned short load_start; /* First record the DB doesn't have */
    BOOL result;
    BOOL new_data_available;/* If there is new data on the weather station */
    char sbuf[50];      /* timestamp string */

    /* Latest history record available from the weather station */
    unsigned short current_ws_record_id;
    time_t current_ws_record_timestamp;

    /* Real program code */
    printf("Open Device...\n");
    open_device();

    printf("Connect to Database...\n");
    pgo_connect(server, username, password);

    /* Determine WS Latest/current record & sync clocks */
    result = sync_clock(&current_ws_record_id, &current_ws_record_timestamp);
    if (result) {
        strftime(sbuf, 50, "%c", localtime(&current_ws_record_timestamp));
        printf("Weather Station current record is %d with time stamp %s\n",
               current_ws_record_id, sbuf);

        if (full_load) {
            /* Download *all* records from the very start. */
            load_start = first_record();
            new_data_available = TRUE;
        } else {
            /* Only download new records */
            new_data_available = find_range_start(&load_start,
                                                  current_ws_record_id);
        }
        if (new_data_available) {
            /* Load and insert a range of history records */
            insert_history_range(load_start,
                                 current_ws_record_id,
                                 current_ws_record_timestamp);

            /* Commit transaction */
            printf("Commit transaction...\n");
            pgo_commit();
        } else {
            printf("Nothing to do: new data on weather station.\n");
        }
    } else {
        fprintf(stderr, "Failed to sync clock.\n");
    }

    /* Finish up */
    printf("Disconnect...\n");
    pgo_disconnect();
    close_device();
    printf("Finished.\n");

    return EXIT_SUCCESS;
}


/* Writes the devices configuration information to the console */
void show_dc(void) {
    device_config dc;
    open_device();
    dc = load_device_config();
    print_device_config(dc);
    close_device();
}

/* Shows a specific record. If record_id is -1 then all records are shown */
void show_record_id(const int record_id) {
    history h;
    history_set hs;
    unsigned short live_record_id;

    open_device();

    /* Show all records */
    if (record_id == -1) {
        printf("Loading all records...");
        get_current_record_id(NULL, NULL, &live_record_id);
        hs = read_history_range(first_record(), previous_record(live_record_id));
        print_history_set(hs);
        free_history_set(hs);
    } else if (record_id >= FIRST_RECORD_SLOT && record_id <= FINAL_RECORD_SLOT){
        /* Show only one record */
        printf("Weather station record %d:\n", record_id);
        h = read_history_record(record_id);
        print_history_record(h);
    }

    close_device();
}

void write_csv(char* filename) {
    FILE* file;
    history_set hs;
    unsigned short current_ws_record_id;
    time_t current_ws_record_timestamp;
    BOOL result;
    char sbuf[50];      /* timestamp string */

    file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file.\n");
        exit(EXIT_FAILURE);
    }

    open_device();

    /* Determine WS Latest/current record & sync clocks */
    result = sync_clock(&current_ws_record_id, &current_ws_record_timestamp);
    if (result) {
        strftime(sbuf, 50, "%c", localtime(&current_ws_record_timestamp));
        printf("Weather Station current record is %d with time stamp %s\n",
               current_ws_record_id, sbuf);

        printf("Download history data. This could take some time...\n");
        hs = read_history_range(first_record(), current_ws_record_id);
        printf("Updating timestamps...\n");
        update_timestamps(&hs, current_ws_record_timestamp);

        printf("Write file...\n");
        write_history_csv_file(file, hs);
        free_history_set(hs);
    } else {
        fprintf(stderr, "Failed to sync clock.\n");
    }

    close_device();
    fclose(file);
}

/* Writes some basic usage information to the console */
void usage(char* appname) {
    printf("\nUsage: %s [options]\n", appname);
    printf("Downloads history records from a Fine Offset WH1080-compatible "
           "weather station.\n");
    printf("\nDatabase update options:\n");
    printf("\t-d database\tThe database to connect to\n");
    printf("\t-u user\t\tThe user account on the database server\n");
    printf("\t-p password\tThe database account password\n");
    printf("\t-l\t\tForce all records to be downloaded, not just new ones.\n");
    printf("\nOther output options:\n");
    printf("\t-c\t\tDisplay device configuration\n");
    printf("\t-f filename\tWrite all history records to a CSV file\n");
    printf("\t-r<record_id>\tDisplay a specific record from the weather station.\n");
    printf("\t\t\tIf no recordid is specified, all records are shown.\n");
    printf("\n\nNote that -c, -f, -r and -dupl are mutually exclusive. You ");
    printf("can not, for\nexample, update the database *and* write to a CSV ");
    printf("file at once.\n");
    printf("\nThe -l option should only be used in the following cases:\n");
    printf("\t* The weather stations memory has been cleared\n");
    printf("\t* The database has not been updated in a long time. No records "
           "in the\n\t  database exist on the weather station.\n");
    printf("Otherwise you will end up with duplicate data in the database.\n");
}

/* Program entry point */
int main(int argc, char *argv[])
{        
    /* getopt stuff */
    extern char *optarg;
    int c;
    BOOL arg_error = FALSE;

    /* Just show device configuration */
    BOOL print_dc = FALSE;

    /* Dump all records to a CSV file */
    BOOL dump_to_csv = FALSE;
    char* filename = NULL;

    /* Show a record */
    BOOL show_record = FALSE;
    int record_id = -1;

    /* Stuff for database updates */
    BOOL full_load = FALSE;         /* Force update from first WS record */
    BOOL update_database = FALSE;
    char* database = NULL;
    char* username = NULL;
    char* password = NULL;

    /* arguments
     * -d database      database connection string (database@host:port)
     * -u username      database username
     * -p password      database password
     * -l               force full load from the first weatherstation record
     * -c               print device configuration instead of updating the
     *                  database
     * -f filename      dump all records to a CSV file instead of updating the
     *                  database
     * -rrecordid       Display recordid from the database.
     */

    /* Note: This uses the GNU extension for optional arguments (::) */
    while ((c = getopt(argc, argv, "d:u:p:lcf:r::")) != -1) {
        switch (c) {
        case 'd':
            if (print_dc || dump_to_csv || show_record)
                arg_error = TRUE;
            else {
                update_database = TRUE;
                database = optarg;
            }
            break;
        case 'u':
            if (print_dc || dump_to_csv || show_record)
                arg_error = TRUE;
            else {
                update_database = TRUE;
                username = optarg;
            }
            break;
        case 'p':
            if (print_dc || dump_to_csv || show_record)
                arg_error = TRUE;
            else {
                update_database = TRUE;
                password = optarg;
            }
            break;
        case 'l':
            if (print_dc || dump_to_csv || show_record)
                arg_error = TRUE;
            else {
                update_database = TRUE;
                full_load = TRUE;
            }
            break;
        case 'c':
            if (update_database || dump_to_csv || show_record)
                arg_error = TRUE;
            else
                print_dc = TRUE;
            break;
        case 'f':
            if (print_dc || update_database || show_record)
                arg_error = TRUE;
            else {
                dump_to_csv = TRUE;
                filename = optarg;
            }
            break;
        case 'r':
            if (print_dc || dump_to_csv || update_database)
                arg_error = TRUE;
            else {
                show_record = TRUE;
                if (optarg != NULL)
                    record_id = atoi(optarg);
            }
            break;
        case '?':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        default:
            fprintf(stderr, "Unrecognised option");
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

/* testing stuff *
    printf("print_dc: %s\n", print_dc ? "TRUE" : "FALSE");
    printf("dump_to_csv: %s\n", dump_to_csv ? "TRUE" : "FALSE");
    if (filename != NULL) printf("\tFilename: %s\n", filename);
    printf("show_record: %s\n", show_record ? "TRUE" : "FALSE");
    if (record_id != -1) printf("\tRecord ID: %d\n", record_id);
    printf("full_load: %s\n", full_load ? "TRUE" : "FALSE");
    printf("update_database: %s\n", update_database ? "TRUE" : "FALSE");
    if (database != NULL) printf("\tDatabase: %s\n", database);
    if (username != NULL) printf("\tUsername: %s\n", username);
    if (password != NULL) printf("\tPassword: %s\n", password);
    printf("arg_error: %s\n", arg_error ? "TRUE" : "FALSE");
**/

    printf("WH1080 Update Tool v1.0\n");
    printf("\t(C) Copyright David Goodwin, 2012\n");

    if (arg_error) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    } else if (print_dc) {
        show_dc();
    } else if (dump_to_csv) {
        write_csv(filename);
    } else if (show_record) {
        show_record_id(record_id);
    } else if (update_database) {
        return db_update(database, username, password, full_load);
    } else {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}


