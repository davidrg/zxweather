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

#include "fileout.h"
#include "common.h"

void write_history_header(FILE* file) {
    fprintf(file,
            "\"Record Number\","
            "\"Download Time\","
            "\"Last In Set\","
            "\"Sample Time (m)\","
            "\"Indoor Relative Humidity (%)\","
            "\"Indoor Temperature (C)\","
            "\"Outdoor Relative Humidity (%)\","
            "\"Outdoor Temperature (C)\","
            "\"Absolute Pressure (Hpa)\","
            "\"Average Wind Speed (m/s)\","
            "\"Gust Wind Speed (m/s)\","
            "\"Wind Direction\","
            "\"Total Rain\","
            "\"Invalid Data\","
            "\"Rain Overflow\"\n"
           );
}

void write_history_record(FILE* file, history h) {
    fprintf(file,
            "%d,%d,%d,%u,%u,%02.1f,%u,%02.1f,%02.1f,%02.1f,%02.1f,%u,%02.1f,%d,%d\n",
            h.record_number,
            h.download_time,
            h.last_in_set,
            h.sample_time,
            h.indoor_relative_humidity,
            SFP(h.indoor_temperature),
            h.outdoor_relative_humidity,
            SFP(h.outdoor_temperature),
            SFP(h.absolute_pressure),
            SFP(h.average_wind_speed),
            SFP(h.gust_wind_speed),
            h.wind_direction,
            h.total_rain * RAIN_MULTIPLY,
            CHECK_BIT_FLAG(h.status, H_SF_INVALID_DATA),
            CHECK_BIT_FLAG(h.status, H_SF_RAINFALL_OVERFLOW)
            );
}

void write_history_csv_file(FILE* file, history_set hs) {
    unsigned int i = 0;
    write_history_header(file);
    for (i = 0; i < hs.record_count; i += 1) {
        write_history_record(file,hs.records[i]);
    }
}
