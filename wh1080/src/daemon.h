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

#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>

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
 *
 * This function does not return.
 */
void daemon(char* server, char* username, char* password, FILE* logfile);

/* Closes files, disconencts from servers, etc */
void cleanup();

#endif // DAEMON_H
