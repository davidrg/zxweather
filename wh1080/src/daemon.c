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

#include "daemon.h"
#include "deviceio.h"
#include "pgout.h"

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
void daemon(char *server, char *username, char *password, FILE *logfile) {

    unsigned short current_record_id;
    time_t current_record_timestamp;
    BOOL result;
    extern FILE* history_log_file;

    /* This is where any log messages from history.c go to */
    history_log_file = logfile;

    fprintf(logfile, "Open Device...\n");
    open_device();

    fprintf(logfile, "Connect to Database...\n");
    pgo_connect(server, username, password);

    result = sync_clock(&current_record_id, &current_record_timestamp);



}

void cleanup() {
    close_device();
    pgo_disconnect();
}
