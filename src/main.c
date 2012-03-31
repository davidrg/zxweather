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

#include "deviceio.h"
#include "deviceconfig.h"
#include "conout.h"
#include "fileout.h"
#include "history.h"
#include "pgout.h"

int main(int argc, char *argv[])
{
    char *server, *username, *password; /* database details */
    unsigned short load_start; /* First record the DB doesn't have */
    history_set new_data;   /* Data to load into database */
    BOOL result;
    char sbuf[50];      /* timestamp string */

    /* Latest history record available from the weather station */
    unsigned short current_ws_record_id;
    time_t current_ws_record_timestamp;

    /* Latest history record available from the database */
    unsigned int current_db_record_id;
    time_t current_db_record_timestamp;
    history current_db_record_from_ws;

    /* Real program code */
    printf("WH1080 Test Application v1.0\n");
    printf("\t(C) Copyright David Goodwin, 2012\n\n");
/*read_history_range(8175, 8173);*/
    if (argc < 4) {
        printf("Using defaults\n");
        server = "weather_dev@localhost:5432";
        username="zxweather";
        password="password";
    } else {
        server = argv[1];
        username = argv[2];
        password = argv[3];
    }

    printf("Open Device...\n");
    open_device();

    printf("Connect to Database...\n");
    pgo_connect(server, username, password);

    /* Determine WS Latest/current record & sync clocks */
    result = sync_clock(&current_ws_record_id, &current_ws_record_timestamp);
    if (!result) {
        fprintf(stderr, "Failed to sync clock.");
        close_device();
        return 1;
    } else {
        strftime(sbuf, 50, "%c", localtime(&current_ws_record_timestamp));
        printf("Weather Station current record is %d with time stamp %s\n",
               current_ws_record_id, sbuf);
    }

    /* Determine latest record in database */
    pgo_get_last_record_number(&current_db_record_id,
                               &current_db_record_timestamp);
    if (current_db_record_timestamp != 0) {
        /* Figure out if that record exists in the weather station and if so,
         * double-check that its the same as whats in the database */
        current_db_record_from_ws = read_history_record(current_db_record_id);

        load_start = next_record(current_db_record_id);
    } else {
        /* Database is empty. Fetch everything from the weather station */
        printf("Database is empty.\n");
        load_start = first_record();
    }

    /* Fetch the records from the weather station & compute timestamps */
    printf("Fetching records %d to %d...\n", load_start, current_ws_record_id);
    new_data = read_history_range(load_start, current_ws_record_id);
    update_timestamps(&new_data, current_ws_record_timestamp);

    /* Insert history data into database */
    pgo_insert_history_set(new_data);

    /* Commit transaction */
    pgo_commit();

    /* Finish up */
    pgo_disconnect();
    close_device();
    printf("Finished.\n");

    /* Testing crap *
    device_config dc;
    history_set hs;
    FILE* file;
    history h;

    if (FALSE) {
        printf("Loading history data...\n");
        hs = read_history();
        if (FALSE) {
            print_history_set(hs);
        } else if (TRUE){
            pgo_get_last_record_number(&current_db_record_id, &current_db_record_timestamp);
            printf("Insert history data...\n");
            pgo_insert_history_set(hs);
            pgo_commit();
        } else {
            printf("Dumping to CSV file...\n");
            file = fopen("out.csv","w");
            write_history_csv_file(file, hs);
            fclose(file);
        }
        free_history_set(hs);
    } else {
        h = read_history_record(dc.history_data_sets-1);
        printf("History Record #0:-\n");
        print_history_record(h);
    }
    */
    return 0;
}

