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

/* deviceconfig.h: Everything you need to read the device configuration. This
 * includes station records and alarm settings.
 *
 * There is not currently any ability to update settings.
 */

#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include "common.h"

/* Records alarm settings (levels at which the alarm will go off if it is
 * enabled */
typedef struct _DCAS {
    unsigned char indoor_relative_humidity_high;
    unsigned char indoor_relative_humidity_low;
    signed short indoor_temperature_high;
    signed short indoor_temperature_low;
    unsigned char outdoor_relative_humidity_high;
    unsigned char outdoor_relative_humidity_low;
    signed short outdoor_temperature_high;
    signed short outdoor_temperature_low;
    signed short wind_chill_high;
    signed short wind_chill_low;
    signed short dew_point_high;
    signed short dew_point_low;
    unsigned short absolute_pressure_high;
    unsigned short absolute_pressure_low;
    unsigned short relative_pressure_high;
    unsigned short relative_pressure_low;
    unsigned char average_bft_high;
    unsigned short average_wind_speed_high;
    unsigned char gust_bft_high;
    unsigned short gust_wind_speed_high;
    unsigned char wind_direction_alm;
    unsigned short rainfall_1h_high;
    unsigned short rainfall_24h_high;
    unsigned char time_alarm_hour;
    unsigned char time_alarm_minute;
} dc_alarm_settings;

/* A time stamp for a station record */
typedef struct _TS {
    unsigned char year;
    unsigned char month;
    unsigned char date;
    unsigned char hour;
    unsigned char minute;
} time_stamp;

/* A station record using signed 16bit integers. Used for temperature */
typedef struct _SSREC {
    signed short min;
    signed short max;
    time_stamp min_ts;
    time_stamp max_ts;
} ss_record;

/* A station record using unsigned 16bit integers. */
typedef struct _USREC {
    unsigned short min;
    unsigned short max;
    time_stamp min_ts;
    time_stamp max_ts;
} us_record;

/* A station record using unsigned 8bit integers */
typedef struct _UCREC {
    unsigned char min;
    unsigned char max;
    time_stamp min_ts;
    time_stamp max_ts;
} uc_record;

/* Station records */
typedef struct _DCSR {
    uc_record indoor_relative_humidity;
    uc_record outdoor_relative_humidity;
    ss_record indoor_temperature;
    ss_record outdoor_temperature;
    ss_record windchill;
    ss_record dewpoint;
    us_record absolute_pressure;
    us_record relative_pressure;
    unsigned short average_wind_speed_max;
    unsigned short gust_wind_speed_max;
    unsigned short rainfall_1h_max;
    unsigned short rainfall_24h_max;
    unsigned short rainfall_week_max;
    unsigned long rainfall_month_max; /* must be > 16 bits */
    unsigned long rainfall_total_max; /* must be > 16 bits */
    time_stamp average_wind_speed_max_ts;
    time_stamp gust_wind_speed_max_ts;
    time_stamp rainfall_1h_max_ts;
    time_stamp rainfall_24h_max_ts;
    time_stamp rainfall_week_max_ts;
    time_stamp rainfall_month_max_ts;
    time_stamp rainfall_total_max_ts;
} dc_station_records;

/* Device Configuration */
typedef struct _DCFG {
    unsigned char current_sampling_time_interval;
    unsigned char config_flags_A;
    unsigned char config_flags_B;
    unsigned char display_format_flags_A;
    unsigned char display_format_flags_B;
    unsigned char alarm_enable_flags_A;
    unsigned char alarm_enable_flags_B;
    unsigned char alarm_enable_flags_C;
    signed char timezone;
    unsigned short history_data_sets;
    unsigned short history_data_stack_address;
    unsigned short relative_pressure; /* display format is - nnnn.n */
    unsigned short absolute_pressure; /* display format is - nnnn.n */
    dc_alarm_settings alarm_settings;
    dc_station_records station_records;
} device_config;

/* Loads device configuration from the weather station */
device_config load_device_config();

/* Prints weather station configuration to the console */
void print_device_config(device_config dc);

/* Creates a new device_config struct from the data in the supplied buffer.
 */
device_config create_device_config(unsigned char* dc_data,
                                   unsigned char *as_data,
                                   unsigned char *sr_data);

/* To check if a bit is set. There are plenty of bits to check below. */
#define CHECK_BIT_FLAG(byte, bit) ((byte & bit) != 0)

/* Unit settings flag byte A (offset 0x00011) */
#define DC_SAF_INSIDE_TEMP_UNIT     0x01 /* set = degF, not set = degC */
#define DC_SAF_OUTDOOR_TEMP_UNIT    0x02 /* set = degF, not set = degC */
#define DC_SAF_RAINFALL_UNIT        0x04 /* set = inches, not set = mm */
#define DC_SAF_RESERVED_A           0x08 /* Reserved */
#define DC_SAF_RESERVED_B           0x10 /* Reserved */
#define DC_SAF_PRESSURE_UNIT_HPA    0x20 /* If the pressure unit is Hpa */
#define DC_SAF_PRESSURE_UNIT_INHG   0x40 /* If the pressure unit is inHg */
#define DC_SAF_PRESSURE_UNIT_MMHG   0x80 /* If the pressure unit is mmHg */

/* Unit settings flag byte B (offset 0x00012) */
#define DC_SBF_WIND_SPEED_UNIT_MS   0x01 /* If the wind speed unit is m/s */
#define DC_SBF_WIND_SPEED_UNIT_KMH  0x02 /* If the wind speed unit is km/h */
#define DC_SBF_WIND_SPEED_UNIT_KNOT 0x04 /* If the wind speed unit is knots */
#define DC_SBF_WIND_SPEED_UNIT_MH   0x08 /* If the wind speed unit is m/h */
#define DC_SBF_WIND_SPEED_UNIT_BFT  0x10 /* If the wind speed unit is bft */
#define DC_SBF_RESERVED_A           0x20 /* Reserved */
#define DC_SBF_RESERVED_B           0x40 /* Reserved */
#define DC_SBF_RESERVED_C           0x80 /* Reserved */

/* Display format flag byte A (offset 0x00013) */
#define DC_DAF_PRESSURE             0x01 /* Set = relative, not set = abs */
#define DC_DAF_WIND_SPEED           0x02 /* Set = gust, not set = average */
#define DC_DAF_TIME_FORMAT          0x04 /* Set = 12H, not set = 24H */
#define DC_DAF_DATE_FORMAT          0x08 /* Set = MMDDYY, not set = DDMMYY */
#define DC_DAF_TIME_SCALE           0x10 /* Set = 24H, not set = 12H */
#define DC_DAF_DATE_COMPLETE        0x20 /* Show complete date */
#define DC_DAF_DATE_DATE_AND_WKDATE 0x40 /* Show date and wk date */
#define DC_DAF_DATE_ALARM_TIME      0x80 /* Show alarm time */

/* Display format flag byte B (offset 0x00014) */
#define DC_DBF_OUTDR_TMP_TEMP       0x01 /* Show outdoor temp */
#define DC_DBF_OUTDR_TMP_WINDCHILL  0x02 /* Show wind chill */
#define DC_DBF_OUTDR_TMP_DEW_POINT  0x04 /* show dew point */
#define DC_DBF_RAINFALL_1H          0x08 /* show 1H rainfall */
#define DC_DBF_RAINFALL_24H         0x10 /* show 24H rainfall */
#define DC_DBF_RAINFALL_WEEK        0x20 /* show weeks rainfall */
#define DC_DBF_RAINFALL_MONTH       0x40 /* show the months rainfall */
#define DC_DBF_RAINFALL_TOTAL       0x80 /* Show total rainfall */

/* Alarm enable flag byte A (offset 0x00014) */
#define DC_AAF_RESERVED_A           0x01 /* Reserved */
#define DC_AAF_TIME                 0x02
#define DC_AAF_WIND_DIRECTION       0x04
#define DC_AAF_RESERVED_B           0x08 /* Reserved */
#define DC_AAF_INDOOR_RELHUMID_LOW  0x10 /* Indoor relative humidity low */
#define DC_AAF_INDOOR_RELHUMID_HIGH 0x20 /* Indoor relative humidity high */
#define DC_AAF_OUTDOR_RELHUMID_LOW  0x40 /* Outdoor relative humidity low */
#define DC_AAF_OUTDOR_RELHUMID_HIGH 0x80 /* Outdoor relative humidity high */

/* Alarm enable flag byte B (offset 0x00015) */
#define DC_ABF_AVG_WIND_SPEED       0x01 /* Average wind speed */
#define DC_ABF_GUST_WIND_SPEED      0x02 /* Gust wind speed */
#define DC_ABF_1H_RAINFALL          0x04 /* 1-hour rainfall */
#define DC_ABF_24H_RAINFALL         0x08 /* 24-hour rainfall */
#define DC_ABF_ABS_PRESSURE_LOW     0x10 /* absolute pressure low */
#define DC_ABF_ABS_PRESSURE_HIGH    0x20 /* absolute pressure high */
#define DC_ABF_REL_PRESSURE_LOW     0x40 /* relative pressure low */
#define DC_ABF_REL_PRESSURE_HIGH    0x80 /* relative pressure high */

/* Alarm enable flag byte C (offset 0x00016) */
#define DC_ACF_INDOOR_TEMP_LOW      0x01 /* Indoor temperature low */
#define DC_ACF_INDOOR_TEMP_HIGH     0x02 /* Indoor temperature high */
#define DC_ACF_OUTDOOR_TEMP_LOW     0x04 /* Outdoor temperature low */
#define DC_ACF_OUTDOOR_TEMP_HIGH    0x08 /* Outdoor temperature high */
#define DC_ACF_WINDCHILL_LOW        0x10 /* Wind chill low */
#define DC_ACF_WINDCHILL_HIGH       0x20 /* Wind chill high */
#define DC_ACF_DEWPOINT_LOW         0x40 /* Dew point low */
#define DC_ACF_DEWPOINT_HIGH        0x50 /* Dew point high */

/* Written to offset 0x1A to notify the weather station that configuration
 * data has been changed by the PC */
#define PC_DATA_REFRESH 0xAA

#endif // DEVICECONFIG_H
