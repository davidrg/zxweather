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

#ifndef DEVICEIO_H
#define DEVICEIO_H

#include "common.h"

void close_device();
void open_device();

/* For reading from the weather station */
void read_block(long memory_address, unsigned char *buffer);
void read_and_validate_block(long memory_address, unsigned char* buffer);

/* Fills a buffer with data read from the weather station */
void fill_buffer(long memory_address,
                 unsigned char *buffer,
                 long buffer_size,
                 BOOL validate); /* Validate data read? */

#endif /* DEVICEIO_H */
