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

/* Offsets for the history data sets field and history data stack field in
 * the device config data area */
#define HISTORY_DATA_SETS_OFFSET 0x0001B
#define HISTORY_DATA_STACK_OFFSET 0x0001E

/* Loads device configuration from the weather station
 */
device_config load_device_config() {
    /* Need 36 bytes of data from offset 0x00000 */
    const int dc_offset = 0x00000;
    const int dc_size = 0x00024;
    unsigned char dc_data[0x00024];

    const int alarm_offset = 0x00030;
    const int alarm_size = 0x00029;
    unsigned char alarm_data[0x00029];

    const int records_offset = 0x00062;
    const int records_size = 0x0009E;
    unsigned char records_data[0x0009E];

    fill_buffer(dc_offset, dc_data, dc_size, FALSE);
    fill_buffer(alarm_offset, alarm_data, alarm_size, FALSE);
    fill_buffer(records_offset, records_data, records_size, FALSE);

    return create_device_config(dc_data, alarm_data, records_data);
}

dc_alarm_settings create_alarm_settings(unsigned char* as_data) {
    dc_alarm_settings as;

    as.indoor_relative_humidity_high = as_data[0];
    as.indoor_relative_humidity_low = as_data[1];
    as.indoor_temperature_high = READ_SSHORT(as_data,2,3);
    as.indoor_temperature_low = READ_SSHORT(as_data,4,5);
    as.outdoor_relative_humidity_high = as_data[6];
    as.outdoor_relative_humidity_low = as_data[7];
    as.outdoor_temperature_high = READ_SSHORT(as_data,8,9);
    as.outdoor_temperature_low = READ_SSHORT(as_data,10,11);
    as.wind_chill_high = READ_SSHORT(as_data,12,13);
    as.wind_chill_low = READ_SSHORT(as_data,14,15);
    as.dew_point_high = READ_SSHORT(as_data,16,17);
    as.dew_point_low = READ_SSHORT(as_data,18,19);
    as.absolute_pressure_high = READ_SHORT(as_data,20,21);
    as.absolute_pressure_low = READ_SHORT(as_data,22,23);
    as.relative_pressure_high = READ_SHORT(as_data,24, 25);
    as.relative_pressure_low = READ_SHORT(as_data,26,27);
    as.average_bft_high = as_data[28];
    as.average_wind_speed_high = READ_SHORT(as_data,29,30);
    as.gust_bft_high = as_data[31];
    as.gust_wind_speed_high = READ_SHORT(as_data,32,33);
    as.wind_direction_alm = as_data[34];
    as.rainfall_1h_high = READ_SHORT(as_data,35,36);
    as.rainfall_24h_high = READ_SHORT(as_data,37,38);
    as.time_alarm_hour = READ_BCD(as_data[39]);
    as.time_alarm_minute = READ_BCD(as_data[40]);

    return as;
}

/* Functions for creating station records */
time_stamp create_time_stamp(unsigned char *offset) {
    time_stamp ts;

    ts.year = READ_BCD(offset[0]);
    ts.month = READ_BCD(offset[1]);
    ts.date = READ_BCD(offset[2]);
    ts.hour = READ_BCD(offset[3]);
    ts.minute = READ_BCD(offset[4]);

    return ts;
}

/* Creates a new ss_record initialising the min and max components */
ss_record create_ss_record(unsigned char *offset) {
    ss_record ss;

    ss.max = READ_SSHORT(offset, 0, 1);
    ss.min = READ_SSHORT(offset, 2, 3);

    return ss;
}

/* Creates a new us_record initialising the min and max components */
us_record create_us_record(unsigned char *offset) {
    us_record us;

    us.max = READ_SHORT(offset, 0, 1);
    us.min = READ_SHORT(offset, 2, 3);

    return us;
}

/* Creates a new uc_record initialising the min and max components */
uc_record create_uc_record(unsigned char *offset) {
    uc_record uc;

    uc.max = offset[0];
    uc.min = offset[1];

    return uc;
}

/* Creates station records (min/max values) struct */
dc_station_records create_station_records(unsigned char* sr_data) {
    dc_station_records sr;
    unsigned char rainfall_nibbles;
    unsigned long month_bit, total_bit;

    sr.indoor_relative_humidity = create_uc_record(&sr_data[0]);
    sr.outdoor_relative_humidity = create_uc_record(&sr_data[2]);
    sr.indoor_temperature = create_ss_record(&sr_data[4]);
    sr.outdoor_temperature = create_ss_record(&sr_data[8]);
    sr.windchill = create_ss_record(&sr_data[12]);
    sr.dewpoint = create_ss_record(&sr_data[16]);
    sr.absolute_pressure = create_us_record(&sr_data[20]);
    sr.relative_pressure = create_us_record(&sr_data[24]);

    sr.average_wind_speed_max = READ_SHORT(sr_data,28,29);
    sr.gust_wind_speed_max = READ_SHORT(sr_data,30,31);
    sr.rainfall_1h_max = READ_SHORT(sr_data,32,33);
    sr.rainfall_24h_max = READ_SHORT(sr_data,34,35);
    sr.rainfall_week_max = READ_SHORT(sr_data,36,37);

    sr.rainfall_month_max = READ_SHORT(sr_data,38,39);
    sr.rainfall_total_max = READ_SHORT(sr_data,40,41);

    /* The upper four bits of both the month and total maximums are stored
     * together at offset 0x0008C */
    rainfall_nibbles = sr_data[42];
    month_bit = (rainfall_nibbles & 0xF0) << 12; /* High Nibble */
    total_bit = (rainfall_nibbles & 0x0F) << 16;
    sr.rainfall_month_max += month_bit;
    sr.rainfall_total_max += total_bit;

    /* Now read timestamps. */
    sr.indoor_relative_humidity.max_ts = create_time_stamp(&sr_data[43]);
    sr.indoor_relative_humidity.min_ts = create_time_stamp(&sr_data[48]);

    sr.outdoor_relative_humidity.max_ts = create_time_stamp(&sr_data[53]);
    sr.outdoor_relative_humidity.min_ts = create_time_stamp(&sr_data[58]);

    sr.indoor_temperature.max_ts = create_time_stamp(&sr_data[63]);
    sr.indoor_temperature.min_ts = create_time_stamp(&sr_data[68]);

    sr.outdoor_temperature.max_ts = create_time_stamp(&sr_data[73]);
    sr.outdoor_temperature.min_ts = create_time_stamp(&sr_data[78]);

    sr.windchill.max_ts = create_time_stamp(&sr_data[83]);
    sr.windchill.min_ts = create_time_stamp(&sr_data[88]);

    sr.dewpoint.max_ts = create_time_stamp(&sr_data[93]);
    sr.dewpoint.min_ts = create_time_stamp(&sr_data[98]);

    sr.absolute_pressure.max_ts = create_time_stamp(&sr_data[103]);
    sr.absolute_pressure.min_ts = create_time_stamp(&sr_data[108]);

    sr.relative_pressure.max_ts = create_time_stamp(&sr_data[113]);
    sr.relative_pressure.min_ts = create_time_stamp(&sr_data[118]);

    sr.average_wind_speed_max_ts = create_time_stamp(&sr_data[123]);
    sr.gust_wind_speed_max_ts = create_time_stamp(&sr_data[128]);
    sr.rainfall_1h_max_ts = create_time_stamp(&sr_data[133]);
    sr.rainfall_24h_max_ts = create_time_stamp(&sr_data[138]);
    sr.rainfall_week_max_ts = create_time_stamp(&sr_data[143]);
    sr.rainfall_month_max_ts = create_time_stamp(&sr_data[148]);
    sr.rainfall_total_max_ts = create_time_stamp(&sr_data[153]);

    return sr;
}

/* Loads device configuration (everything that isn't sample data) */
device_config create_device_config(unsigned char* dc_data,
                                   unsigned char* as_data,
                                   unsigned char* sr_data) {
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
    dc.history_data_sets = dc_data[0x1C] << 8;          /* MSB */
    dc.history_data_sets += dc_data[0x1B];              /* LSB */
    dc.history_data_stack_address = dc_data[0x1F] << 8; /* MSB */
    dc.history_data_stack_address += dc_data[0x1E];     /* LSB */
    dc.relative_pressure = dc_data[0x21] << 8;          /* MSB */
    dc.relative_pressure += dc_data[0x20];              /* LSB */
    dc.absolute_pressure = dc_data[0x23] << 8;          /* MSB */
    dc.absolute_pressure += dc_data[0x22];              /* LSB */
    dc.alarm_settings = create_alarm_settings(as_data);
    dc.station_records = create_station_records(sr_data);
    return dc;
}

void get_history_data_info(unsigned short *history_data_sets,
                           unsigned short *history_data_stack) {
    /* History data sets - 2 bytes, history data stack - 2 bytes. There is one
     * reserved byte between the two. That makes for five bytes.
     */
    unsigned char data[5];
    fill_buffer(HISTORY_DATA_SETS_OFFSET, data, 5, TRUE);

    *history_data_sets = READ_SHORT(data, 0, 1);
    /* data[2] is just some reserved byte that we had to read as well */
    *history_data_stack = READ_SHORT(data, 3, 4);
}
