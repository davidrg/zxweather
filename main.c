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

#include <stdio.h>
#include <string.h>

#include "deviceio.h"
#include "deviceconfig.h"
#include "debug.h"

#include "hidapi/hidapi.h"

/* This is what the WH1080 appears as.
 */
#define VENDOR_ID 0x1941
#define PRODUCT_ID 0x8021

/* For reading/writing to the WH1080 */
#define END_MARK 0x20
#define WRITE_COMMAND 0xA0
#define READ_COMMAND 0xA1
#define WRITE_COMMAND_WORD 0xA2

int main(void)
{
    open_device();

    device_config dc = load_device_config();
    print_device_config(dc);

    return 0;
}

