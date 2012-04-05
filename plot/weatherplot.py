from __future__ import print_function
from optparse import OptionParser
import os
import psycopg2
import subprocess


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

gnuplot_binary = r'C:\Program Files (x86)\gnuplot\bin\gnuplot.exe'

def plot_day(dest_dir, cur, year, month, day):
    print("Plotting graphs for {0} {1} {2}...".format(year, month_name[month], day))

    dest_dir += str(day) + '/'

    try:
        os.makedirs(dest_dir)
    except:
        pass

    date = '{0}-{1}-{2}'.format(day,month,year)

    cur.execute("""select time_stamp::time, temperature, dew_point,
        apparent_temperature, wind_chill, relative_humidity, absolute_pressure
        from sample where date(time_stamp) = %s::date
        order by time_stamp asc""", (date,))
    temperature_data = cur.fetchall()

    # Write the data file for gnuplot
    file_data = ['# timestamp  temperature  dew point  apparent temperature  wind chill  relative humidity  absolute pressure\n']
    for record in temperature_data:
        file_data.append('{0}        {1}        {2}        {3}        {4}        {5}        {6}\n'
            .format(str(record[0]),str(record[1]),str(record[2]),
                    str(record[3]), str(record[4]),str(record[5]),
                    str(record[6])))

    data_filename = dest_dir + 'gnuplot_data.dat'
    file = open(data_filename,'w+')
    file.writelines(file_data)
    file.close()

    # Plot Temperature and Dew Point
    output_filename = dest_dir + 'temperature_tdp.png'
    gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
    gnuplot.communicate('''set style data lines
set title "Temperature and Dew Point"
set grid
set xdata time
set timefmt "%H:%M:%S"
set format x "%H:%M"
set xlabel "Time"
set ylabel "Temperature"
set terminal png
set output "{0}"
plot '{1}' using 1:2 title "Temperature", '{2}' using 1:3 title "Dew Point" '''
    .format(output_filename,data_filename,data_filename))

    # Plot Apparent Temperature and Wind Chill
    output_filename = dest_dir + 'temperature_awc.png'
    gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
    gnuplot.communicate('''set style data lines
set title "Apparent Temperature and Wind Chill"
set grid
set xdata time
set timefmt "%H:%M:%S"
set format x "%H:%M"
set xlabel "Time"
set ylabel "Temperature"
set terminal png
set output "{0}"
plot '{1}' using 1:4 title "Apparent Temperature", '{2}' using 1:5 title "Wind Chill" '''
    .format(output_filename,data_filename,data_filename))

    # Humidity
    output_filename = dest_dir + 'humidity.png'
    gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
    gnuplot.communicate('''set style data lines
set title "Humidity"
set grid
set xdata time
set timefmt "%H:%M:%S"
set format x "%H:%M"
set xlabel "Time"
set ylabel "Relative Humidity (%)"
set terminal png
set output "{0}"
plot '{1}' using 1:6 title "Relative Humidity"'''
    .format(output_filename,data_filename))

    # Absolute Pressure
    output_filename = dest_dir + 'pressure.png'
    gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
    gnuplot.communicate('''set style data lines
set title "Absolute Pressure"
set grid
set xdata time
set timefmt "%H:%M:%S"
set format x "%H:%M"
set xlabel "Time"
set ylabel "Absolute Pressure (Hpa)"
set terminal png
set output "{0}"
plot '{1}' using 1:7 title "Absolute Pressure"'''
    .format(output_filename,data_filename))
    pass

def plot_month(dest_dir, cur, year, month):
    print("Plotting graphs for {0} {1}...".format(year, month_name[month]))

    dest_dir += month_name[month] + '/'

    # Generate graphs for the entire month

    # Then deal with each day
    cur.execute("""select distinct extract(day from time_stamp)
        from sample
        where extract(year from time_stamp) = %s
          and extract(month from time_stamp) = %s
        order by extract(day from time_stamp)""", (year,month,))
    days = cur.fetchall()

    for day in days:
        plot_day(dest_dir, cur, year, month, int(day[0]))

def plot_year(dest_dir, cur, year):
    """
    Plots summary graphs for the specified year. It will then call
    plot_month() for each month in the database for this year.
    :param dest_dir: The directory to write graphs to
    :type dest_dir: string
    :param cur: Database cursor to use for queries
    :type cur: cursor
    :param year: The year to plot charts for
    :type year: int
    :return:
    :rtype:
    """
    print("Plotting graphs for {0}...".format(year))
    dest_dir += str(year) + '/'

    # Generate graphs for the entire year

    # Then deal with each month
    cur.execute("""select distinct extract(month from time_stamp)
        from sample where extract(year from time_stamp) = %s
        order by extract(month from time_stamp)""", (year,))
    months = cur.fetchall()
    for month in months:
        plot_month(dest_dir, cur, year, int(month[0]))


def main():
    """
    Program entry point. Parses options, connects to the database and then
    calls plot_year() once for each year present in the database.
    :return:
    :rtype:
    """

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

    (options, args) = parser.parse_args()


    print("Weather data plotting application v1.0")
    print("\t(C) Copyright David Goodwin, 2012\n\n")

    print("Connecting to database...")
    connection_string = "host=" + options.hostname
    connection_string += " user=" + options.username
    connection_string += " password=" + options.password
    connection_string += " dbname=" + options.dbname

    con = psycopg2.connect(connection_string)

    cur = con.cursor()

    cur.execute("select version()")
    data = cur.fetchone()
    print("Database version: {0}".format(data[0]))

    print("Generating temperature plots in {0}".format(options.directory))

    dest_dir = options.directory
    if not dest_dir.endswith('/'):
        dest_dir += '/'

    # First up, figure out what years we have
    cur.execute("select distinct extract(year from time_stamp) from sample")
    years = cur.fetchall()

    for year in years:
        plot_year(dest_dir, cur, int(year[0]))

    print("Finished.")







if __name__ == "__main__": main()