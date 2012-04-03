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

#ifndef COMMON_H
#define COMMON_H

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef BOOL
#define BOOL int
#endif

/* Read an unsigned short */
#define READ_SHORT(buffer, LSB, MSB) ((buffer[MSB] << 8) + buffer[LSB])

/* Read a signed short */
#define READ_SSHORT(buffer, LSB, MSB) ((buffer[MSB] >= 128 ? -1 : 1) * (((buffer[MSB] >= 128 ? buffer[MSB] - 128 : buffer[MSB]) * 256) + buffer[LSB]))

/* Read a Binary-coded decimal value */
#define READ_BCD(byte)((((byte / 16) & 0x0F) * 10) + (byte & 0x0F))

/* Convert a single-digit fixed point integer (eg, 100) to a float (eg, 10.0) */
#define SFP(val) (val / 10.0)

/* To check if a bit is set. */
#define CHECK_BIT_FLAG(byte, bit) ((byte & bit) != 0)

#endif /* COMMON_H */
