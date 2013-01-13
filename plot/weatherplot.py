#!/usr/bin/env python
from __future__ import print_function
from datetime import date
from optparse import OptionParser
import os
import pickle
import datetime
import psycopg2
import time
import signal
from day_charts import charts_1_day, charts_7_days
import gnuplot
from month_charts import month_charts

__author__ = 'David Goodwin'

month_name = {1 : 'january',
              2 : 'february',
              3 : 'march',
              4 : 'april',
              5 : 'may',
              6 : 'june',
              7 : 'july',
              8 : 'august',
              9 : 'september',
              10: 'october',
              11: 'november',
              12: 'december'}

# Where we should start plotting from. That is, we will plot all charts for
# years, months and days greater than or equal to this one.
start_date = date(1900,01,01)

# Last date that we plotted for. This will climb as the program runs and then
# be written to disk when we finish to be the start_date next time.
final_date = date(1900,01,01)

# Handle Ctr;+C nicely.
def handler(signum, frame):
    print("weatherplot stopped.")
    exit()
signal.signal(signal.SIGINT, handler)

def plot_day(dest_dir, cur, year, month, day, station_code):
    """

    :param dest_dir:
    :param cur:
    :param year:
    :param month:
    :param day:
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :return:
    """
    global start_date, final_date

    print("Plotting graphs for {0} {1} {2}...".format(year, month_name[month], day),end='')

    if date(year,month,day) < start_date:
        print("Skip")
        return
    else:
        print("") # newline

    dest_dir += str(day) + '/'

    try:
        os.makedirs(dest_dir)
    except Exception:
        pass

    # Plot the 1-day charts
    charts_1_day(cur, dest_dir, day, month, year, station_code)
    charts_7_days(cur, dest_dir, day, month, year, station_code)

    final_date = date(year,month, day)
    pass

def plot_month(dest_dir, cur, year, month, station_code):
    """
    Plots charts for a particular month
    :param dest_dir:
    :param cur: Database cursor
    :param year: Year to plot charts for
    :param month: Month to plot charts for
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :return:
    """
    global start_date

    print("Plotting graphs for {0} {1}...".format(year, month_name[month]),end="")

    if year < start_date.year or (year == start_date.year and month < start_date.month):
        print("Skip")
        return
    else:
        print("") # newline


    dest_dir += month_name[month] + '/'

    try:
        os.makedirs(dest_dir)
    except Exception:
        pass

    # Generate graphs for the entire month
    month_charts(cur,dest_dir, month, year, station_code)

    # Then deal with each day
    cur.execute("""select distinct extract(day from s.time_stamp)
        from sample s
        inner join station st on st.station_id = s.station_id
        where extract(year from s.time_stamp) = %s
          and extract(month from s.time_stamp) = %s
          and st.code = %s
        order by extract(day from s.time_stamp)""", (year,month, station_code,))
    days = cur.fetchall()

    for day in days:
        plot_day(dest_dir, cur, year, month, int(day[0]), station_code)


def plot_year(dest_dir, cur, year, station_code):
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
    :return:
    :rtype:
    """
    global start_date

    print("Plotting graphs for {0}...".format(year),end="")

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
          and st.code = %s
        order by extract(month from s.time_stamp)""", (year, station_code,))
    months = cur.fetchall()
    for month in months:
        plot_month(dest_dir, cur, year, int(month[0]), station_code)


def main():
    """
    Program entry point. Parses options, connects to the database and then
    calls plot_year() once for each year present in the database.
    :return:
    :rtype:
    """

    global start_date, final_date

    # Configure and run the option parser
    parser = OptionParser()
    parser.add_option("-t", "--database",dest="dbname",
                      help="Database name")
    parser.add_option("-n", "--host", dest="hostname",
                      help="PostgreSQL Server Hostname")
    parser.add_option("-u","--user",dest="username",
                      help="PostgreSQL Username")
    parser.add_option("-p","--password",dest="password",
                      help="PostgreSQL Password")
    parser.add_option("-d", "--directory", dest="directory",
                      help="Output Directory")
    parser.add_option("-a", "--plot-new", dest="plot_new",
                      help="Plot charts for days on or after the date in the specified file")
    parser.add_option("-r", "--replot-pause", dest="replot_pause",
                      help="Charts will be replotted every x seconds until Ctrl+C is used to terminate the program")
    parser.add_option("-g", "--gnuplot-binary", dest="gnuplot_bin",
                      help="Gnuplot binary to use")
    parser.add_option("-s", "--station", dest="station_code",
                      help="The station to plot charts for")

    (options, args) = parser.parse_args()


    print("Weather data plotting application v1.1")
    print("\t(C) Copyright David Goodwin, 2012, 2013\n\n")

    error = False
    if options.station_code is None:
        print("ERROR: station code not specified")
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

    if error:
        print("Required parameters were not supplied. Re-run with --help for options.")
        return


    if options.gnuplot_bin is not None:
        gnuplot.gnuplot_binary = options.gnuplot_bin

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

    print("Generating temperature plots in {0} for station {1}".format(
        options.directory, options.station_code))

    dest_dir = options.directory
    if not dest_dir.endswith('/'):
        dest_dir += '/'

    if options.plot_new is not None:
        try:
            with open(options.plot_new, "r") as update_file:
                start_date = pickle.load(update_file)
                print("Plotting from {0}".format(start_date))
        except IOError:
            print("Update file does not exist. It will be created.")
            # If it doesn't exist the start date is already initialised to 1900/01/01

    while True:
        # First up, figure out what years we have
        cur.execute("""select distinct extract(year from s.time_stamp)
                       from sample s
                       inner join station st on st.station_id = s.station_id
                       where st.code = %s
                       """, (options.station_code,))
        years = cur.fetchall()

        for year in years:
            plot_year(dest_dir, cur, int(year[0]), options.station_code)

        # Update stored date for next time
        if options.plot_new is not None:
            with open(options.plot_new,"w") as update_file:
                pickle.dump(final_date, update_file)
            start_date = final_date

        print("Plot completed at {0}".format(datetime.datetime.now()))

        if options.replot_pause is None:
            break # Only doing one plot
        else:
            # Replotting every x seconds.
            print("Waiting for {0} seconds to plot again. Press Ctrl+C to terminate.".format(options.replot_pause))
            time.sleep(float(options.replot_pause))

    print("Finished.")



if __name__ == "__main__": main()