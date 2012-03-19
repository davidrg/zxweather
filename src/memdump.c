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

#include "deviceio.h"
#include "debug.h"
#include <stdio.h>

#define MEM_SIZE 131071

int main(void) {
    unsigned char memory[MEM_SIZE];
    printf("WH1080 Memory Dumper v1.0\n");
    printf("\t(C) Copyright David Goodwin, 2012\n\n");

    printf("dumping to memdump.bin...\n");
    open_device();

    fill_buffer(0,memory, MEM_SIZE, TRUE);

    write_buffer(memory, MEM_SIZE, "memdump.bin");

    close_device();

    printf("Dump Complete.\n");
    return 0;
}
