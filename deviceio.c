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

#include <hidapi.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

/* This is what the WH1080 appears as.
 */
#define VENDOR_ID 0x1941
#define PRODUCT_ID 0x8021

/* For reading/writing to the WH1080 */
#define END_MARK 0x20
#define WRITE_COMMAND 0xA0
#define READ_COMMAND 0xA1
#define WRITE_COMMAND_WORD 0xA2

/* Number of bytes to read from the device. 16 is the minimum. */
#define READ_SIZE_BYTES 32

static hid_device *handle;

void open_device() {
    handle = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
}

/* Reads a block of data in a single pass. No validation is performed.
 */
void read_block(int memory_address, unsigned char *buffer) {
    unsigned char command_buffer[9];
    int result;
    int read_size = READ_SIZE_BYTES;
    unsigned char read_buffer[READ_SIZE_BYTES];
    unsigned char address_high = memory_address / 256;
    unsigned char address_low = memory_address % 256;
    unsigned char foo[9];

    printf("Read address %d (0x%05X)\n", memory_address, memory_address);

    /* Send a command requesting 32 bytes of data from the specified address */
    command_buffer[0] = 0x0;
    command_buffer[1] = READ_COMMAND;
    command_buffer[2] = address_high;
    command_buffer[3] = address_low;
    command_buffer[4] = END_MARK;
    command_buffer[5] = READ_COMMAND;
    command_buffer[6] = address_high;
    command_buffer[7] = address_low;
    command_buffer[8] = END_MARK;

    result = hid_write(handle,          /* HID Device handle */
                       command_buffer,  /* Buffer to write */
                       sizeof(command_buffer));              /* Buffer length */

    /* And then read the 32 bytes of data back in */
    memset(read_buffer, 0, read_size);



    hid_read(handle, foo, 9);
    memcpy(read_buffer, foo, 8);
    hid_read(handle, foo, 9);
    memcpy(&read_buffer[8], foo, 8);
    hid_read(handle, foo, 9);
    memcpy(&read_buffer[16], foo, 8);
    hid_read(handle, foo, 9);
    memcpy(&read_buffer[24], foo, 8);

    memcpy(buffer, read_buffer, READ_SIZE_BYTES);
}

/* Repeatedly reads the requested block of data until two consecutive reads
 * return the same data.
 */
void read_and_validate_block(int memory_address, unsigned char *output_buffer) {
    unsigned char oldbuffer[READ_SIZE_BYTES];
    unsigned char buffer[READ_SIZE_BYTES];

    memset(&buffer, 0, READ_SIZE_BYTES);
    memset(&oldbuffer, 0, READ_SIZE_BYTES);

    read_block(memory_address, buffer);

    /* Loop until the two buffers are equal (memcmp returns zero) */
    while (memcmp(&oldbuffer, &buffer, READ_SIZE_BYTES) != 0) {
        /* Two buffers do not match. Re-read. */
        memcpy(&oldbuffer, &buffer, READ_SIZE_BYTES);
        memset(&buffer, 0, READ_SIZE_BYTES);

        read_block(memory_address, buffer);
    }

    memcpy(output_buffer, &buffer, READ_SIZE_BYTES);
}

void fill_buffer(int memory_address,
                 unsigned char *buffer,
                 int buffer_size,
                 BOOL validate) {

    unsigned char read_buffer[READ_SIZE_BYTES];
    int pos = 0;

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
