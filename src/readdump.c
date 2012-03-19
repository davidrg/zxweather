/*****************************************************************************
 *            Created: 19/03/2012
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

#include "deviceconfig.h"
#include "history.h"
#include "deviceio.h"
#include "conout.h"
#include <stdio.h>

int main(void) {
    device_config dc;
    history_set hs;

    printf("WH1080 Memory Dump Reader v1.0\n");
    printf("\t(C) Copyright David Goodwin, 2012\n\n");

    open_device();

    printf("Loading Device Config...\n");
    dc = load_device_config();
    printf("Loading history data...\n");
    hs = read_history();

    print_device_config(dc);
    print_history_set(hs);

    free_history_set(hs);

    close_device();
    return 0;
}
