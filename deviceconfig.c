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

#include "deviceconfig.h"
#include "deviceio.h"
#include "debug.h"
#include <stdio.h>


/* Loads device configuration from the weather station
 */
device_config load_device_config() {
    /* Need 36 bytes of data from offset 0x00000 */
    const int dc_size = 36;
    unsigned char dc_data[dc_size];

    fill_buffer(0x0000, dc_data, dc_size, FALSE);

    dump_buffer(&dc_data, dc_size);

    return create_device_config(dc_data);
}

device_config create_device_config(unsigned char* dc_data) {
    device_config dc;

    if (dc_data[0] != 0x55 || dc_data[1] != 0xAA) {
        printf("ERROR: Expected 0x55 0xAA at offset 0. Got 0x%X 0x%X\n",
               dc_data[0], dc_data[1]);
    }

    dc.current_sampling_time_interval = dc_data[0x10];
    dc.config_flags_A = dc_data[0x11];
    dc.config_flags_B = dc_data[0x12];
    dc.display_format_flags_A = dc_data[0x13];
    dc.display_format_flags_B = dc_data[0x14];
    dc.alarm_enable_flags_A = dc_data[0x15];
    dc.alarm_enable_flags_B = dc_data[0x16];
    dc.alarm_enable_flags_C = dc_data[0x17];
    dc.timezone = (signed char)dc_data[0x18];
    dc.history_data_sets = dc_data[0x1C] << 8; /* MSB */
    dc.history_data_sets &= dc_data[0x1B]; /* LSB */
    dc.history_data_stack_address = dc_data[0x1F] << 8; /* MSB */
    dc.history_data_stack_address &= dc_data[0x1E]; /* LSB */
    dc.relative_pressure = dc_data[0x21] << 8; /* MSB */
    dc.relative_pressure &= dc_data[0x20]; /* LSB */
    dc.absolute_pressure = dc_data[0x23] << 8; /* MSB */
    dc.absolute_pressure &= dc_data[0x22]; /* LSB */

    return dc;
}

int print_flag(int char_total, char* flag) {
    int flag_len = strlen(flag) + 1;
    int new_total = char_total + flag_len;

    if (flag_len + char_total >= 80 || char_total == -1) {
        printf("\n\t");
        new_total = flag_len;
    }

    printf("%s ", flag);
    return new_total;
}

/* Dumps strings for the device config flags to the console */
void print_device_config_flags(device_config dc) {
    if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_INSIDE_TEMP_UNIT))
        printf("INDOOR_TEMP_DEG_F ");
    else
        printf("INDOOR_TEMP_DEG_C ");
    if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_OUTDOOR_TEMP_UNIT))
        printf("OUTDOOR_TEMP_DEG_F ");
    else
        printf("OUTDOOR_TEMP_DEG_C ");
    if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_RAINFALL_UNIT))
        printf("RAINFALL_UNIT_IN\n\t");
    else
        printf("RAINFALL_UNIT_MM\n\t");
    if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_MMHG))
        printf("PRESSURE_UNIT_MMHG ");
    else if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_INHG))
        printf("PRESSURE_UNIT_INHG ");
    else if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_HPA))
        printf("PRESSURE_UNIT_HPA ");
    else
        printf("INVALID_PRESSURE_UNIT ");
    if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_MS))
        printf("WIND_SPEED_UNIT_MS");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_BFT))
        printf("WIND_SPEED_UNIT_BFT");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_MH))
        printf("WIND_SPEED_UNIT_MH");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_KNOT))
        printf("WIND_SPEED_UNIT_KNOT");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_KMH))
        printf("WIND_SPEED_UNIT_KMH");
    else
        printf("INVALID_WIND_SPEED_UNIT");
    printf("\n");
}

#define TWO_FLAG_STRING(byte, flag, str_a, str_b) (CHECK_BIT_FLAG(byte,flag) ? str_a : str_b)

void print_display_format_flags(device_config dc) {
    int ct = -1;

    ct = print_flag(ct, TWO_FLAG_STRING(dc.alarm_enable_flags_A,
                                        DC_DAF_PRESSURE,
                                        "PRESSURE_RELATIVE",
                                        "PRESSURE_ABSOLUTE"));
    ct = print_flag(ct, TWO_FLAG_STRING(dc.alarm_enable_flags_A,
                                        DC_DAF_WIND_SPEED,
                                        "WIND_SPEED_GUST",
                                        "WIND_SPEED_AVERAGE"));

}

void print_alarm_enable_flags(device_config dc) {

}

void print_device_config(device_config dc) {
    printf("Current sampling time interval: %d\n",
           dc.current_sampling_time_interval);
    printf("Config flags: 0x%02X 0x%02X\n\t",
           dc.config_flags_A,
           dc.config_flags_B);
    print_device_config_flags(dc);
    printf("Display format flags: 0x%02X 0x%02X\n",
           dc.display_format_flags_A,
           dc.display_format_flags_B);
    print_display_format_flags(dc);
    printf("Alarm enable flags: 0x%02X 0x%02X 0x%02X\n",
           dc.alarm_enable_flags_A,
           dc.alarm_enable_flags_B,
           dc.alarm_enable_flags_C);
    print_alarm_enable_flags(dc);
    printf("Timezone: %d\n", dc.timezone);
    printf("History data sets: %d\n", dc.history_data_sets);
    printf("History data stack address: %d\n", dc.history_data_stack_address);
    printf("Relative pressure (Hpa): %d\n", dc.relative_pressure);
    printf("Absolute pressure (Hpa): %d\n", dc.absolute_pressure);
}
