/*****************************************************************************
 *            Created: 20/03/2012
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

#ifndef PGOUT_H
#define PGOUT_H

#include "history.h"

/* Connect to the target server as the specified user */
void pgo_connect(const char* target, const char* username, const char *password);

/* Gets the ID for the specified station code */
long pgo_get_station_id(const char* station_code);

/* Gets the record number and time stamp of the most recent sample in the
 * database. Used to determine which samples from the weather station need
 * to be imported into the database. */
void pgo_get_last_record_number(unsigned short *record_number,
                                time_t* time_stamp,
                                long station_id);

/* Inserts an entire history_set, in order, into the database. */
void pgo_insert_history_set(history_set hs, long station_id);

/* Updates live data in the database. Note that this commits the transaction.*/
void pgo_update_live(history live_record, long station_id);

/* Called to notify other database users that we've finished updating data
 * for now.
 * WARNING: This commits the current transaction
 */
void pgo_updates_complete();

/* Commits the current transaction */
void pgo_commit();

/* Rolls back the current transaction */
void pgo_rollback();

/* Disconnects from the current server */
void pgo_disconnect();

#endif /* PGOUT_H */
