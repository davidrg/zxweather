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

/* This file is a drop-in replacement for deviceio.c which is backed by a
 * memory dump rather than a real device accessed over USB. Use the memdump
 * utility to create a suitable dump file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "deviceio.h"

/* Number of bytes to read from the device. 16 is the minimum. */
#define READ_SIZE_BYTES 32

static FILE* infile;
char* fileio_filename;

void open_device() {
    infile = fopen(fileio_filename, "rb");

    if (infile == NULL) {
        printf("Failed to open memdump.bin\n");
        exit(1);
    }

    fseek(infile, 0, SEEK_SET);
}

void close_device() {
    if (infile != NULL) {
        fclose(infile);
        infile = NULL;
    }
}

void read_block(long memory_address, unsigned char* buffer) {
    memset(buffer, 0, READ_SIZE_BYTES);
    fseek(infile, memory_address, SEEK_SET);
    fread((void*)buffer, READ_SIZE_BYTES, 1, infile);
}

void read_and_validate_block(long memory_address, unsigned char *output_buffer) {
    read_block(memory_address, output_buffer);
}

void fill_buffer(long memory_address,
                 unsigned char *buffer,
                 long buffer_size,
                 BOOL validate) {

    unsigned char read_buffer[READ_SIZE_BYTES];
    long pos = 0;

    while (pos < buffer_size) {
        if (validate)
            read_and_validate_block(memory_address + pos, read_buffer);
        else
            read_block(memory_address + pos, read_buffer);

        if (pos + READ_SIZE_BYTES > buffer_size) {
            printf("Taking only %d bytes from block\n", buffer_size-pos);
            memcpy(&buffer[pos], &read_buffer, buffer_size - pos);
            pos += buffer_size - pos;
        } else {
            memcpy(&buffer[pos], &read_buffer, READ_SIZE_BYTES);
            pos += READ_SIZE_BYTES;
        }
    }
}
