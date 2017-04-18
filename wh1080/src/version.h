/*****************************************************************************
 *            Created: 22/03/2012
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

#ifndef VERSION_H
#define VERSION_H

/* Version of the WH1080 tools. These applications are versioned separately
 * from zxweather as they only tend to be updated when the database schema
 * changes or to fix issues arising from newer library/compiler versions. */
#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_REV 1

/* REMEMBER: update the version string in main.c */


/* v2.0.1 - shipped with zxweather v1.0.0.
 *        -> Fix compile warnings
 * v2.0.0 - shipped with zxweather v0.2.x.
 *        -> Added muli-station support
 *        -> Database version checking. Checks application '' version 0.2.0.
 * v1.0.0 - shipped with zxweather v0.1.x.
 *        -> First release.
 */

#endif // VERSION_H
