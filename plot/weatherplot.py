#!/usr/bin/env python
# coding=utf-8
from __future__ import print_function
from datetime import date
import json
from optparse import OptionParser
import os
import pickle
import datetime
import psycopg2
import time
import signal
from day_charts import charts_1_day, charts_7_days, rainfall_1_day
import gnuplot
from month_charts import month_charts

__author__ = 'David Goodwin'

# TODO: Refactor this entire program. Its a horrible mess.

month_name = {1: 'january',
              2: 'february',
              3: 'march',
              4: 'april',
              5: 'may',
              6: 'june',
              7: 'july',
              8: 'august',
              9: 'september',
              10: 'october',
              11: 'november',
              12: 'december'}


class StationConfig(object):
    def __init__(self, hw_type, config_data):
        self._hw_type = hw_type
        if hw_type == 'DAVIS' and config_data is not None:
            self._load_davis_data(json.loads(config_data))
        else:
            self._is_wireless = False
            self._has_solar_sensor = False
            self._has_uv_sensor = False
            self._broadcast_id = None

    def _load_davis_data(self, config_document):
        self._is_wireless = config_document['is_wireless']
        self._has_solar_sensor = config_document['has_solar_and_uv']
        self._has_uv_sensor = config_document['has_solar_and_uv']
        self._broadcast_id = config_document['broadcast_id']

    @property
    def hardware_type(self):
        return self._hw_type

    @property
    def is_wireless(self):
        return self._is_wireless

    @property
    def has_solar_sensor(self):
        return self._has_solar_sensor

    @property
    def has_uv_sensor(self):
        return self._has_uv_sensor

    @property
    def broadcast_id(self):
        return self._broadcast_id


# Handle Ctr;+C nicely.
def handler(signum, frame):
    print("weatherplot stopped.")
    exit()


signal.signal(signal.SIGINT, handler)


def plot_day(dest_dir, cur, plot_date, station_code, start_date, output_format,
             hw_config, write_data):
    """
    Plots charts for a single day.

    :param dest_dir: Directory to write charts to
    :type dest_dir: str
    :param cur: Database cursor
    :param plot_date: Date to plot for
    :type plot_date: date
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param start_date: The date to plot from
    :type start_date: date
    :param output_format: The output format (eg, "pngcairo")
    :type output_format: str
    :param hw_config: Station hardware configuration
    :type hw_config: StationConfig
    :param write_data: If data file should be re-written.
    :type write_data: bool
    :return:
    """

    print("Plotting graphs for {0} {1} {2}, station {3}...".format(
        plot_date.year, month_name[plot_date.month], plot_date.day,
        station_code), end='')

    if plot_date < start_date:
        print("Skip")
        return
    else:
        print("")  # newline

    dest_dir += str(plot_date.day) + '/'

    try:
        os.makedirs(dest_dir)
    except Exception:
        pass

    # Plot the 1-day charts
    charts_1_day(cur, dest_dir, plot_date, station_code, output_format,
                 hw_config, write_data)
    rainfall_1_day(cur, dest_dir, plot_date, station_code, output_format)
    charts_7_days(cur, dest_dir, plot_date, station_code, output_format,
                  hw_config, write_data)

    # Disabled because the graph is fairly unreadable
    # rainfall_7_day(cur, dest_dir, plot_date, station_code, output_format)


def plot_month(dest_dir, cur, year, month, station_code, start_date,
               output_format, hw_config, write_data):
    """
    Plots charts for a particular month
    :param dest_dir:
    :param cur: Database cursor
    :param year: Year to plot charts for
    :param month: Month to plot charts for
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param start_date: The date to plot from
    :type start_date: date
    :param output_format: The output format (eg, "pngcairo")
    :type output_format: str
    :param hw_config: Station hardware configuration
    :type hw_config: StationConfig
    :param write_data: If data should be re-written
    :type write_data: bool
    :return: The final date plotted for
    :rtype: date
    """

    print("Plotting graphs for {0} {1}, station {2}...".format(
        year, month_name[month], station_code), end="")

    if year < start_date.year or \
            (year == start_date.year and month < start_date.month):
        print("Skip")
        return
    else:
        print("")  # newline

    dest_dir += month_name[month] + '/'

    try:
        os.makedirs(dest_dir)
    except Exception:
        pass  # Directory probably already exists.

    # Generate graphs for the entire month
    month_charts(cur, dest_dir, month, year, station_code, output_format,
                 hw_config, write_data)

    # Then deal with each day
    cur.execute("""select distinct extract(day from s.time_stamp)
        from sample s
        inner join station st on st.station_id = s.station_id
        where extract(year from s.time_stamp) = %s
          and extract(month from s.time_stamp) = %s
          and lower(st.code) = lower(%s)
        order by extract(day from s.time_stamp)""", (year,month, station_code,))
    days = cur.fetchall()

    final_date = None
    for day in days:
        final_date = date(year, month, int(day[0]))
        plot_day(dest_dir, cur, final_date, station_code, start_date,
                 output_format, hw_config, write_data)
    return final_date


def plot_year(dest_dir, cur, year, station_code, start_date, output_format, hw_config, write_data):
    """
    Plots summary graphs for the specified year. It will then call
    plot_month() for each month in the database for this year.
    :param dest_dir: The directory to write graphs to
    :type dest_dir: string
    :param cur: Database cursor to use for queries
    :type cur: cursor
    :param year: The year to plot charts for
    :type year: int
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param start_date: The date to plot from
    :type start_date: date
    :param output_format: The output format (eg "pngcairo")
    :type output_format: str
    :param hw_config: Hardware configuration data
    :type hw_config: StationConfig
    :param write_data: If data file should be rewritten
    :type write_data: bool
    :return: The final date plotted for
    :rtype: date
    """

    print("Plotting graphs for {0}, station {1}...".format(year, station_code),
          end="")

    if year < start_date.year:
        print("Skip")
        return
    else:
        print("") # newline

    dest_dir += str(year) + '/'

    # Generate graphs for the entire year

    # Then deal with each month
    cur.execute("""select distinct extract(month from s.time_stamp)
        from sample s
        inner join station st on st.station_id = s.station_id
        where extract(year from s.time_stamp) = %s
          and lower(st.code) = lower(%s)
        order by extract(month from s.time_stamp)""", (year, station_code,))
    months = cur.fetchall()

    final_date = None
    for month in months:
        final_date = plot_month(dest_dir, cur, year, int(month[0]),
                                station_code, start_date, output_format,
                                hw_config, write_data)

    return final_date


def plot_for_station(code, cur, dest_dir, start_date, output_format, write_data):

    # First up, figure out what years we have
    cur.execute("""select distinct extract(year from s.time_stamp)
from sample s
inner join station st on st.station_id = s.station_id
where lower(st.code) = lower(%s)
order by extract(year from s.time_stamp)
                           """, (code,))
    years = cur.fetchall()

    cur.execute("""select s.station_config as config_data,
       st.code as hw_type
from station s
inner join station_type st on st.station_type_id = s.station_type_id
where lower(s.code) = lower(%s)""", (code,))
    result = cur.fetchone()

    hw_config = StationConfig(result[1], result[0])

    dest_dir += code + '/'

    final_date = None
    for year in years:
        final_date = plot_year(dest_dir, cur, int(year[0]), code, start_date,
                               output_format, hw_config, write_data)

    print("Plot completed at {0} for station {1}".format(
        datetime.datetime.now(), code))

    return final_date


def main():
    """
    Program entry point. Parses options, connects to the database and then
    calls plot_year() once for each year present in the database.
    :return:
    :rtype:
    """

    # Configure and run the option parser
    parser = OptionParser()
    parser.add_option("-t", "--database", dest="dbname",
                      help="Database name")
    parser.add_option("-n", "--host", dest="hostname",
                      help="PostgreSQL Server Hostname")
    parser.add_option("-u","--user", dest="username",
                      help="PostgreSQL Username")
    parser.add_option("-p","--password", dest="password",
                      help="PostgreSQL Password")
    parser.add_option("-d", "--directory", dest="directory",
                      help="Output Directory")
    parser.add_option("-a", "--plot-new", dest="plot_new",
                      help="Plot charts for days on or after the date in the "
                           "specified file")
    parser.add_option("-r", "--replot-pause", dest="replot_pause",
                      help="Charts will be replotted every x seconds until "
                           "Ctrl+C is used to terminate the program")
    parser.add_option("-g", "--gnuplot-binary", dest="gnuplot_bin",
                      help="Gnuplot binary to use")
    parser.add_option("-c", "--gnuplot-processes", dest="gnuplot_count",
                      help="Number of concurrent gnuplot processes to use. "
                           "Default is 1.")
    parser.add_option("-s", "--station", dest="station_codes", action="append",
                      help="The stations to plot charts for")

    # TODO: Don't rewrite the data file for each format
    parser.add_option("-f", "--output-format", dest="output_formats",
                      action="append",
                      help="The formats to generate output in. Default is "
                           "'pngcairo' (high quality PNG). Other options are "
                           "'png',  'gif', 'txt' and 'txtwide'.")

    (options, args) = parser.parse_args()

    print("Weather data plotting application v1.2 (zxweather v1.0)")
    print("\t(C) Copyright David Goodwin, 2012-2021\n\n")

    error = False
    if options.station_codes is None or len(options.station_codes) == 0:
        print("ERROR: no station codes specified")
        error = True
    if options.hostname is None:
        print("ERROR: hostname not specified")
        error = True
    if options.username is None:
        print("ERROR: username not specified")
        error = True
    if options.password is None:
        print("ERROR: password not specified")
        error = True
    if options.dbname is None:
        print("ERROR: database name not specified")
        error = True
    if options.directory is None:
        print("ERROR: output directory not specified")
        error = True
    if options.output_formats is None or len(options.output_formats) == 0:
        # Default output format.
        options.output_formats = ["pngcairo"]

    if error:
        print("Required parameters were not supplied. Re-run with --help for options.")
        return

    dest_dir = options.directory

    if '\\' in dest_dir:
        print("ERROR: back slashes ('\\') are not allowed in the destination "
              "directory name. Use the '/' character as the directory "
              "separator.")
        return

    if not dest_dir.endswith('/'):
        dest_dir += '/'

    if options.gnuplot_bin is not None:
        gnuplot.gnuplot_binary = options.gnuplot_bin

    if options.gnuplot_count is None:
        gnuplot.gnuplot_count = 1
    else:
        gnuplot.gnuplot_count = int(options.gnuplot_count)

    print("Connecting to database...")
    connection_string = "host=" + options.hostname
    connection_string += " user=" + options.username
    connection_string += " password=" + options.password
    connection_string += " dbname=" + options.dbname

    con = psycopg2.connect(connection_string)

    cur = con.cursor()

    cur.execute("select version()")
    data = cur.fetchone()
    print("Server version: {0}".format(data[0]))

    cur.execute(
        "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")
    result = cur.fetchone()

    if result is None:
        db_version = 1
    else:
        cur.execute("select v::integer from db_info where k = 'DB_VERSION'")
        db_version = cur.fetchone()[0]

    if db_version < 3:
        print("*** ERROR: Plot requires at least database "
              "version 3 (zxweather 1.0.0). Please upgrade your "
              "database.")
        exit(1)

    cur.execute("select version_check('PLOT',1,0,0)")
    result = cur.fetchone()
    if not result[0]:
        cur.execute("select minimum_version_string('PLOT')")
        result = cur.fetchone()

        print("*** ERROR: This version of Plot is "
              "incompatible with the configured database. The "
              "minimum Plot (or zxweather) version "
              "supported by this database is: {0}.".format(
                result[0]))
        exit(1)

    print("Generating plots in {0} for stations {1}".format(
        options.directory, options.station_codes))

    plot_dates = {}
    if options.plot_new is not None:
        try:
            with open(options.plot_new, "rb") as update_file:
                plot_dates = pickle.load(update_file)
                if isinstance(plot_dates, date):
                    plot_dates = {}
        except IOError:
            print("Update file does not exist. It will be created.")
            plot_dates = {}
        except EOFError:
            print("Update file is empty or corrupt. It will be recreated.")
            plot_dates = {}

    for code in options.station_codes:
        for output_format in options.output_formats:
            if len(options.output_formats) == 1 and output_format == "pngcairo":
                format_key = code
            else:
                format_key = "{0}_{1}".format(code, output_format)
            if format_key not in plot_dates:
                plot_dates[format_key] = date(1900, 1, 1)

            print("Plotting from {0} for station {1}, format {2}".format(
                code, plot_dates[format_key], output_format))

    while True:
        exec_start = time.time()

        for station in options.station_codes:
            write_data = True
            for output_format in options.output_formats:
                if len(options.output_formats) == 1 and \
                        output_format == "pngcairo":
                    format_key = station
                else:
                    format_key = "{0}_{1}".format(station, output_format)

                plot_dates[format_key] = plot_for_station(
                    station, cur, dest_dir, plot_dates[format_key],
                    output_format, write_data)
                write_data = False

                # Update stored date for next time
                if options.plot_new is not None:
                    with open(options.plot_new, "wb") as update_file:
                        pickle.dump(plot_dates, update_file)

        print("Plot completed in {0} seconds".format(time.time() - exec_start))

        if options.replot_pause is None:
            break  # Only doing one plot
        else:
            # Replotting every x seconds.
            print("Waiting for {0} seconds to plot again. Press Ctrl+C to "
                  "terminate.".format(options.replot_pause))
            time.sleep(float(options.replot_pause))

    print("Finished.")


if __name__ == "__main__":
    main()
