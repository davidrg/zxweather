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

static char* wind_direction[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE",
                                "SSE", "S", "SSW", "SW", "WSW", "W", "WNW",
                                "NW", "NNW", "INVALID"};
#define WIND_DIR(byte) (wind_direction[byte > 15 ? 16 : byte])

void write_history_header(FILE* file) {
    fprintf(file,
            "\"Record Number\","
            "\"Download Time\","
            "\"Time Stamp\","
            "\"Last In Set\","
            "\"Sample Time (m)\","
            "\"Indoor Relative Humidity (%%)\","
            "\"Indoor Temperature (C)\","
            "\"Outdoor Relative Humidity (%%)\","
            "\"Outdoor Temperature (C)\","
            "\"Absolute Pressure (hPa)\","
            "\"Average Wind Speed (m/s)\","
            "\"Gust Wind Speed (m/s)\","
            "\"Wind Direction\","
            "\"Total Rain\","
            "\"Invalid Data\","
            "\"Rain Overflow\","
            "\"Status\"\n"
           );
}

void write_history_record(FILE* file, history h) {
    char dltime[50];
    char timestamp[50];

    strftime(dltime, 50, "%c", localtime(&h.download_time));
    strftime(timestamp, 50, "%c", localtime(&h.time_stamp));

    fprintf(file,
            "%d,\"%s\",\"%s\",\"%s\",%u,%u,%02.1f,%u,%02.1f,%02.1f,%02.1f,%02.1f,\"%s\",%02.1f,%d,%d,%u\n",
            h.record_number,
            dltime,
            timestamp,
            (h.last_in_set) ? "true" : "false",
            h.sample_time,
            h.indoor_relative_humidity,
            SFP(h.indoor_temperature),
            h.outdoor_relative_humidity,
            SFP(h.outdoor_temperature),
            SFP(h.absolute_pressure),
            SFP(h.average_wind_speed),
            SFP(h.gust_wind_speed),
            WIND_DIR(h.wind_direction),
            h.total_rain * RAIN_MULTIPLY,
            CHECK_BIT_FLAG(h.status, H_SF_INVALID_DATA),
            CHECK_BIT_FLAG(h.status, H_SF_RAINFALL_OVERFLOW),
            h.status
            );
}

void write_history_csv_file(FILE* file, history_set hs) {
    unsigned int i = 0;
    write_history_header(file);
    for (i = 0; i < hs.record_count; i += 1) {
        write_history_record(file,hs.records[i]);
    }
}
