from __future__ import print_function
from optparse import OptionParser
import os
import psycopg2
from day_charts import charts_1_day, charts_7_days

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


def plot_day(dest_dir, cur, year, month, day):
    print("Plotting graphs for {0} {1} {2}...".format(year, month_name[month], day))

    dest_dir += str(day) + '/'

    try:
        os.makedirs(dest_dir)
    except:
        pass

    # Plot the 1-day charts
    charts_1_day(cur, dest_dir, day, month, year)
    charts_7_days(cur, dest_dir, day, month, year)
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
    print("Server version: {0}".format(data[0]))

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