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
    device_config dc;
    history_set hs;
    FILE* file;
    history h;
    unsigned int record_number;
    time_t time_stamp;
    char *server;
    char *username;
    char *password;
    unsigned short current_ws_record_id;
    time_t current_ws_record_timestamp;
    BOOL result;
    char sbuf[50];

    printf("WH1080 Test Application v1.0\n");
    printf("\t(C) Copyright David Goodwin, 2012\n\n");

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

    dc = load_device_config();
    printf("RecordCount: %d\n",dc.history_data_sets);

    result = sync_clock(&current_ws_record_id, &current_ws_record_timestamp);
    if (!result) {
        fprintf(stderr, "Failed to sync clock.");
        close_device();
        return 1;
    } else {
        strftime(sbuf, 50, "%c", localtime(&current_ws_record_timestamp));
        printf("Current record is %d with time stamp %s\n",
               current_ws_record_id, sbuf);
    }

    printf("Connect to Database...\n");
    pgo_connect(server, username, password);

    if (FALSE) {
        /* Determine WS Latest/current record & sync clocks */

        /* Determine latest record in database */

        /* Fetch that record from the weather station */

        /* Decide if the databases latest record is in the database or not */

        /* If DB Latest is in weatherstation, grab everything from that record
         * up to WS Latest */

        /* If DB Latest is not in weather station, grab everything from the
         * weather station. */

        /* Insert history data into database */
        pgo_insert_history_set(hs);

        /* Commit transaction */
        pgo_commit();
    }


    printf("Load Device Configuration...\n");
    dc = load_device_config();
    /*print_device_config(dc);*/

    if (FALSE) {
        printf("Loading history data...\n");
        hs = read_history();
        if (TRUE) {
            print_history_set(hs);
        } else if (FALSE){
            pgo_get_last_record_number(&record_number, &time_stamp);
            printf("%d\n%d\n", record_number, (long)time_stamp);
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

    printf("Disconnecting...\n");
    if (TRUE)
        pgo_disconnect();
    close_device();

    printf("Finished.\n");

    return 0;
}

