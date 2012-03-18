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
    const int dc_offset = 0x00000;
    const int dc_end = 0x00023;
    const int dc_size = dc_end - dc_offset;
    unsigned char dc_data[dc_size];

    const int alarm_offset = 0x00030;
    const int alarm_end = 0x00058;
    const int alarm_size = alarm_end - alarm_offset;
    unsigned char alarm_data[alarm_size];

    const int records_offset = 0x00062;
    const int records_end = 0x000FF;
    const int records_size = records_end - records_offset;
    unsigned char records_data[records_size];

    fill_buffer(dc_offset, dc_data, dc_size, FALSE);
    fill_buffer(alarm_offset, alarm_data, alarm_size, FALSE);
    fill_buffer(records_offset, records_data, records_size, FALSE);


    return create_device_config(dc_data, alarm_data, records_data);
}

#define READ_SHORT(buffer, LSB, MSB) ((buffer[MSB] << 8) + buffer[LSB]);

dc_alarm_settings create_alarm_settings(unsigned char* as_data) {
    dc_alarm_settings as;

    as.indoor_relative_humidity_high = as_data[0];
    as.indoor_relative_humidity_low = as_data[1];
    as.indoor_temperature_high = READ_SHORT(as_data,2,3);
    as.indoor_temperature_low = READ_SHORT(as_data,4,5);
    as.outdoor_relative_humidity_high = as_data[6];
    as.outdoor_relative_humidity_low = as_data[7];
    as.outdoor_temperature_high = READ_SHORT(as_data,8,9);
    as.outdoor_temperature_low = READ_SHORT(as_data,10,11);
    as.wind_chill_high = READ_SHORT(as_data,12,13);
    as.wind_chill_low = READ_SHORT(as_data,14,15);
    as.dew_point_high = READ_SHORT(as_data,16,17);
    as.dew_point_low = READ_SHORT(as_data,18,19);
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
    as.time_alarm_hour = as_data[39];
    as.time_alarm_minute = as_data[40];

    return as;
}

/* Functions for creating station records */
time_stamp create_time_stamp(unsigned char *offset) {
    time_stamp ts;

    return ts;
}

ss_record create_ss_record(unsigned char *offset) {
    ss_record ss;
    return ss;
}

us_record create_us_record(unsigned char *offset) {
    us_record us;

    return us;
}

uc_record create_uc_record(unsigned char *offset) {
    uc_record uc;

    return uc;
}

dc_station_records create_station_records(unsigned char* sr_data) {

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

/*****************************************************************************
 * Device config printing code below                                         *
 *****************************************************************************/

int print_flag(int char_total, char* flag) {
    int flag_len = strlen(flag) + 1;
    int new_total = char_total + flag_len;

    if (flag_len + char_total >= 80) {
        printf("\n\t");
        new_total = flag_len + 8;
    } else if (char_total == -1) {
        printf("\t");
        new_total = flag_len + 8;
    }

    printf("%s ", flag);
    return new_total;
}

#define TWO_FLAG_STRING(byte, flag, str_a, str_b) (CHECK_BIT_FLAG(byte,flag) ? str_a : str_b)
#define PF(byte, flag, str_a, str_b) (ct = print_flag(ct,TWO_FLAG_STRING(byte,flag,str_a, str_b)))

/* Dumps strings for the device config flags to the console */
void print_device_config_flags(device_config dc) {
    int ct = -1;

    PF(dc.config_flags_A,
       DC_SAF_INSIDE_TEMP_UNIT,
       "INDOOR_TEMP_DEG_F",
       "INDOOR_TEMP_DEG_C");
    PF(dc.config_flags_A,
       DC_SAF_OUTDOOR_TEMP_UNIT,
       "OUTDOOR_TEMP_DEG_F",
       "OUTDOOR_TEMP_DEG_C");
    PF(dc.config_flags_A,
       DC_SAF_RAINFALL_UNIT,
       "RAINFALL_UNIT_IN",
       "RAINFALL_UNIT_MM");

    if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_MMHG))
        ct = print_flag(ct,"PRESSURE_UNIT_MMHG");
    else if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_INHG))
        ct = print_flag(ct,"PRESSURE_UNIT_INHG");
    else if (CHECK_BIT_FLAG(dc.config_flags_A, DC_SAF_PRESSURE_UNIT_HPA))
        ct = print_flag(ct,"PRESSURE_UNIT_HPA");
    else
        ct = print_flag(ct,"INVALID_PRESSURE_UNIT");

    if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_MS))
        ct = print_flag(ct,"WIND_SPEED_UNIT_MS");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_BFT))
        ct = print_flag(ct,"WIND_SPEED_UNIT_BFT");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_MH))
        ct = print_flag(ct,"WIND_SPEED_UNIT_MH");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_KNOT))
        ct = print_flag(ct,"WIND_SPEED_UNIT_KNOT");
    else if (CHECK_BIT_FLAG(dc.config_flags_B, DC_SBF_WIND_SPEED_UNIT_KMH))
        ct = print_flag(ct,"WIND_SPEED_UNIT_KMH");
    else
        ct = print_flag(ct,"INVALID_WIND_SPEED_UNIT");
    printf("\n");
}

void print_display_format_flags(device_config dc) {
    int ct = -1;

    PF(dc.display_format_flags_A,
       DC_DAF_PRESSURE,
       "PRESSURE_RELATIVE",
       "PRESSURE_ABSOLUTE");
    PF(dc.display_format_flags_A,
       DC_DAF_WIND_SPEED,
       "WIND_SPEED_GUST",
       "WIND_SPEED_AVERAGE");
    PF(dc.display_format_flags_A,
       DC_DAF_TIME_FORMAT,
       "TIME_FORMAT_12H",
       "TIME_FORMAT_24H");
    PF(dc.display_format_flags_A,
       DC_DAF_DATE_FORMAT,
       "DATE_FORMAT_MMDDYY",
       "DATE_FORMAT_DDMMYY");
    PF(dc.display_format_flags_A,
       DC_DAF_TIME_SCALE,
       "TIME_SCALE_24H",
       "TIME_SCALE_12H");

    if (CHECK_BIT_FLAG(dc.display_format_flags_A, DC_DAF_DATE_COMPLETE))
        ct = print_flag(ct, "SHOW_DATE_COMPLETE");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_A, DC_DAF_DATE_DATE_AND_WKDATE))
        ct = print_flag(ct, "SHOW_DATE_AND_WK_DATE");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_A, DC_DAF_DATE_ALARM_TIME))
        ct = print_flag(ct, "SHOW_DATE_ALARM_TIME");
    else
        ct = print_flag(ct, "INVALID_DATE_DISPLAY_MODE");

    if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_OUTDR_TMP_TEMP))
        ct = print_flag(ct, "SHOW_OUTDOOR_TEMP");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_OUTDR_TMP_WINDCHILL))
        ct = print_flag(ct, "SHOW_OUTDOOR_WINDCHILL");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_OUTDR_TMP_DEW_POINT))
        ct = print_flag(ct, "SHOW_OUTDOOR_DEWPOINT");
    else
        ct = print_flag(ct, "INVALID_OUTDOOR_TEMP_DISPLAY_MODE");

    if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_RAINFALL_1H))
        ct = print_flag(ct, "SHOW_RAINFALL_1H");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_RAINFALL_24H))
        ct = print_flag(ct, "SHOW_RAINFALL_24H");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_RAINFALL_WEEK))
        ct = print_flag(ct, "SHOW_RAINFALL_WEEK");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_RAINFALL_MONTH))
        ct = print_flag(ct, "SHOW_RAINFALL_MONTH");
    else if (CHECK_BIT_FLAG(dc.display_format_flags_B, DC_DBF_RAINFALL_TOTAL))
        ct = print_flag(ct, "SHOW_RAINFALL_TOTAL");
    else
        ct = print_flag(ct, "INVALID_RAINFALL_DISPLAY_MODE");

    printf("\n");
}

#define ENABLED(byte, flag) (CHECK_BIT_FLAG(byte, flag) ? "Enabled" : "Disabled")
#define PRINT_ALARM(byte, flag, name) (printf("\t%s: %s\n", name, ENABLED(byte,flag)))

void print_alarm_enable_flags(device_config dc) {
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_RESERVED_A, "Reserved A");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_TIME, "Time");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_WIND_DIRECTION, "Wind Direction");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_RESERVED_B, "Reserved B");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_INDOOR_RELHUMID_LOW, "Indoor Relative Humidity Low");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_INDOOR_RELHUMID_HIGH, "Indoor Relative Humidity High");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_OUTDOR_RELHUMID_LOW, "Outdoor Relative Humidity Low");
    PRINT_ALARM(dc.alarm_enable_flags_A, DC_AAF_OUTDOR_RELHUMID_HIGH, "Outdoor Relative Humidity High");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_AVG_WIND_SPEED, "Average Wind Speed");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_GUST_WIND_SPEED, "Gust Wind Speed");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_1H_RAINFALL, "1-Hour Rainfall");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_24H_RAINFALL, "24-Hour Rainfall");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_ABS_PRESSURE_LOW, "Absolute Pressure Low");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_ABS_PRESSURE_HIGH, "Absolute Pressure High");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_REL_PRESSURE_LOW, "Relative Pressure Low");
    PRINT_ALARM(dc.alarm_enable_flags_B, DC_ABF_REL_PRESSURE_HIGH, "Relative Pressure High");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_INDOOR_TEMP_LOW, "Indoor Temperature Low");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_INDOOR_TEMP_HIGH, "Indoor Temperature High");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_OUTDOOR_TEMP_LOW, "Outdoor Temperature Low");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_OUTDOOR_TEMP_HIGH, "Outdoor Temperature High");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_WINDCHILL_LOW, "Windchill Low");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_WINDCHILL_HIGH, "Windchill High");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_DEWPOINT_LOW, "Dewpoint Low");
    PRINT_ALARM(dc.alarm_enable_flags_C, DC_ACF_DEWPOINT_HIGH, "Dewpoint High");
}

void print_alarm_settings(dc_alarm_settings as) {
    printf("Alarm Settings:-\n");
    printf("\tIndoor Relative Humidity High: %d%\n", as.indoor_relative_humidity_high);
    printf("\tIndoor Relative Humidity Low: %d%\n", as.indoor_relative_humidity_low);
    printf("\tIndoor Temperature High: %d C\n", as.indoor_temperature_high);
    printf("\tIndoor Temperature Low: %d C\n", as.indoor_temperature_low);
    printf("\tOutdoor Relative Humidity High: %d%\n", as.outdoor_relative_humidity_high);
    printf("\tOutdoor Relative Humidity Low: %d%\n", as.outdoor_relative_humidity_low);
    printf("\tOutdoor Temperature High: %d C\n", as.outdoor_temperature_high);
    printf("\tOutdoor Temperature Low: %d C\n", as.outdoor_temperature_low);
    printf("\tWind Chill High: %d C\n", as.wind_chill_high);
    printf("\tWind Chill Low: %d C\n", as.wind_chill_low);
    printf("\tDew Point High: %d C\n", as.dew_point_high);
    printf("\tDew Point Low: %d C\n", as.dew_point_low);
    printf("\tAbsolute Pressure High: %d Hpa\n", as.absolute_pressure_high);
    printf("\tAbsolute Pressure Low: %d Hpa\n", as.absolute_pressure_low);
    printf("\tRelative Pressure High: %d Hpa\n", as.relative_pressure_high);
    printf("\tRelative Pressure Low: %d Hpa\n", as.relative_pressure_low);
    printf("\tAverage BFT High: %d bft\n", as.average_bft_high);
    printf("\tAverage Wind Speed High: %d m/s\n", as.average_wind_speed_high);
    printf("\tGust BFT High: %d bft\n", as.gust_bft_high);
    printf("\tGust Wind Speed High: %d m/s\n", as.gust_wind_speed_high);
    printf("\tWind Direction ALM: 0x%02X\n", as.wind_direction_alm);
    printf("\t1H Rainfall High: %d mm\n", as.rainfall_1h_high);
    printf("\t24H Rainfall High: %d mm\n", as.rainfall_24h_high);
}

void print_device_config(device_config dc) {
    printf("Current sampling time interval: %d\n",
           dc.current_sampling_time_interval);
    printf("Config flags: 0x%02X 0x%02X\n",
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
    printf("\n");
    print_alarm_settings(dc.alarm_settings);
}
