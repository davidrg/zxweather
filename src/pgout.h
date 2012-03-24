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

/* Gets the record number and time stamp of the most recent sample in the
 * database. Used to determine which samples from the weather station need
 * to be imported into the database. */
void pgo_get_last_record_number(unsigned int *record_number, time_t* time_stamp);

/* Inserts an entire history_set, in order, into the database. */
void pgo_insert_history_set(history_set hs);

/* Commits the current transaction */
void pgo_commit();

/* Rolls back the current transaction */
void pgo_rollback();

/* Disconnects from the current server */
void pgo_disconnect();

#endif /* PGOUT_H */
