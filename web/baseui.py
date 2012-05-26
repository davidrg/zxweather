"""
Base class for UIs
"""

import os
import web
from web.contrib.template import render_jinja
import config
from datetime import datetime, date
from config import db

__author__ = 'David Goodwin'



class BaseUI:
    """
    Base class for user interfaces.
    """
    def __init__(self, template_dir):
        """
        Initialise the base UI functionality.
        :param template_dir: The directory to look for templates in.
        :type template_dir: string
        """

        self.template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join(template_dir))
        self.render = render_jinja(self.template_dir, encoding='utf-8')



    @staticmethod
    def get_live_indoor_data():
        """
        Gets live indoor data from the database. If config.live_data_available
        is set then the data will come from the live data table and will be at
        most 48 seconds old. If it is not set then the data returned will be
        the most recent sample from the sample table.

        :return: Timestamp for the data and the actual data.
        """
        now = datetime.now()
        params = dict(date=date(now.year, now.month, now.day))

        if config.live_data_available:
            current_data = db.query("""select download_timestamp::time as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
                from live_data""")[0]
            current_data_ts = current_data.time_stamp
        else:
            # Fetch the latest data for today
            current_data = db.query("""select time_stamp::time as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
            from sample
            where date(time_stamp) = $date
            order by time_stamp desc
            limit 1""",params)[0]
            current_data_ts = current_data.time_stamp

        return current_data_ts, current_data

    @staticmethod
    def get_month_days(year, month):
        """
        Returns a list of all days in the specified month for which there
        exists data in the database.

        :param year: Months year
        :type year: integer
        :param month: Month
        :type month: integer
        :return: A list of days in the month that have data
        :type: [integer]
        :raise: web.NotFound() if there is no data for the month.
        """
        month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp,
           extract(day from time_stamp) as day_stamp
    from sample
    group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
    order by day_stamp""", dict(year=year, month=month))

        if not len(month_data):
            # If there are no months in this year then there can't be any data.
            raise web.NotFound()

        days = []
        for day in month_data:
            day = int(day.day_stamp)
            days.append(day)

        return days

    @staticmethod
    def get_year_months(year):
        """
        Returns a list of all months in the specified year for which there
        exists data in the database
        :param year: The year to get months for
        :type year: integer
        :return: A list of months in the year that have data
        :type: [integer]
        :raise: web.NotFound() if there is no data for the year.
        """
        month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    group by year_stamp, month_stamp) as md where md.year_stamp = $year""", dict(year=year))


        if not len(month_data):
            # If there are no months in this year then there can't be any data.
            raise web.NotFound()

        months = []
        for month in month_data:
            the_month_name = month_name[month.month_stamp]
            months.append(the_month_name)

        return months

    @staticmethod
    def get_yearly_records(year):
        """
        Gets the records for the year (data from the yearly_records view).
        :param year: Year to get records for.
        :type year: integer
        :return: Records for the year.
        :raise: web.NotFound() if there is no data for the year.
        """
        params = dict(year=year)
        yearly_records = db.query("""SELECT year_stamp, total_rainfall, max_gust_wind_speed, max_gust_wind_speed_ts::timestamp,
       max_average_wind_speed, max_average_wind_speed_ts::timestamp, min_absolute_pressure,
       min_absolute_pressure_ts::timestamp, max_absolute_pressure, max_absolute_pressure_ts::timestamp,
       min_apparent_temperature, min_apparent_temperature_ts::timestamp, max_apparent_temperature,
       max_apparent_temperature_ts::timestamp, min_wind_chill, min_wind_chill_ts::timestamp,
       max_wind_chill, max_wind_chill_ts::timestamp, min_dew_point, min_dew_point_ts::timestamp,
       max_dew_point, max_dew_point_ts::timestamp, min_temperature, min_temperature_ts::timestamp,
       max_temperature, max_temperature_ts::timestamp, min_humidity, min_humidity_ts::timestamp,
       max_humidity, max_humidity_ts::timestamp
  FROM yearly_records
 WHERE year_stamp = $year""", params)

        if not len(yearly_records):
            # Bad url or something.
            raise web.NotFound()

        return yearly_records[0]

    @staticmethod
    def get_monthly_records(year, month):
        """
        Gets the records for the month (data from the monthly_records view).
        :param year: Year to get records for.
        :type year: integer
        :param month:  Month to get records for
        :type month: integer
        :return: Records for the year.
        :raise: web.NotFound() if there is no data for the year/month.
        """

        params = dict(date=date(year,month,01))
        monthly_records = db.query("""SELECT date_stamp, total_rainfall, max_gust_wind_speed, max_gust_wind_speed_ts::timestamp,
       max_average_wind_speed, max_average_wind_speed_ts::timestamp, min_absolute_pressure,
       min_absolute_pressure_ts::timestamp, max_absolute_pressure, max_absolute_pressure_ts::timestamp,
       min_apparent_temperature, min_apparent_temperature_ts::timestamp, max_apparent_temperature,
       max_apparent_temperature_ts::timestamp, min_wind_chill, min_wind_chill_ts::timestamp,
       max_wind_chill, max_wind_chill_ts::timestamp, min_dew_point, min_dew_point_ts::timestamp,
       max_dew_point, max_dew_point_ts::timestamp, min_temperature, min_temperature_ts::timestamp,
       max_temperature, max_temperature_ts::timestamp, min_humidity, min_humidity_ts::timestamp,
       max_humidity, max_humidity_ts::timestamp
  FROM monthly_records
 WHERE date_stamp = $date""", params)

        if not len(monthly_records):
            # Bad url or something.
            raise web.NotFound()

        return monthly_records[0]
