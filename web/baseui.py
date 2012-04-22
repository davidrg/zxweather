"""
Base class for UIs
"""

import os
import web
from web.contrib.template import render_jinja
import config
from data.util import rfcformat
from datetime import datetime, timedelta, date, time
from config import db

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
month_number = {'january'  : 1,
                'february' : 2,
                'march'    : 3,
                'april'    : 4,
                'may'      : 5,
                'june'     : 6,
                'july'     : 7,
                'august'   : 8,
                'september': 9,
                'october'  : 10,
                'november' : 11,
                'december' : 12}

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
    def cache_control_headers(data_age, year, month=None, day=None):
        """
        Adds cache control headers (Cache-Control, Expires, Last-Modified).

        :param data_age: Age of the data on the page
        :type data_age: datetime.datetime
        :param year: Year the page is for
        :type year: integer
        :param month: Month the page is for (if it is a month or day page)
        :type month: integer
        :param day: Day the page is for (if it is a day page)
        :type day: integer
        """

        now = datetime.now()

        if year == now.year and (month is None or month == now.month)\
        and (day is None or day == now.day):
            # The page is still getting new data.
            web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
            web.header('Expires', rfcformat(data_age + timedelta(0, config.sample_interval)))
        else:
            web.header('Expires',rfcformat(now + timedelta(60,0)))
        web.header('Last-Modified', rfcformat(data_age))

    @staticmethod
    def year_cache_control(year):
        """
        Cache Control headers for a year page.
        :param year: The year to get cache control headers for
        :type year: integer
        """
        params = dict(date=date(year, 01, 01))
        age_data = db.query(
            "select max(time_stamp) as max_ts from sample where "
            "date_trunc('year', time_stamp) = $date", params)
        data_age = age_data[0].max_ts
        BaseUI.cache_control_headers(data_age, year)

    @staticmethod
    def month_cache_control(year, month):
        """
        Cache control headers for a month page.
        :param year: Year to get cache control headers for
        :type year: integer
        :param month: Month to get cache control headers for
        :type month: integer
        """
        params = dict(date=date(year, month, 01))
        age_data = db.query(
            "select max(time_stamp) as max_ts from sample where "
            "date_trunc('month', time_stamp) = $date", params)
        data_age = age_data[0].max_ts
        BaseUI.cache_control_headers(data_age, year, month)

    @staticmethod
    def day_cache_control(data_age, year, month, day):
        """
        Gets cache control headers for a day page.
        :param data_age: Age of the data if known (if not it will be looked up
                         in the database).
        :type data_age: datetime.datetime
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :param day: Page day
        :type day: integer
        """
        if data_age is None:
            params = dict(date = date(year,month,day))
            age_data = db.query("select max(time_stamp) as max_ts from sample where "
                                "time_stamp::date = $date", params)
            data_age = age_data[0].max_ts
        elif isinstance(data_age, time):
            # We already have the _time_ for the page - just need to add the
            # date on.
            data_age = datetime.combine(date(year,month,day),
                                                 data_age)
        BaseUI.cache_control_headers(data_age, year, month, day)

    @staticmethod
    def get_live_data():
        """
        Gets live data from the database. If config.live_data_available is set
        then the data will come from the live data table and will be at most
        48 seconds old. If it is not set then the data returned will be the
        most recent sample from the sample table.

        :return: Timestamp for the data and the actual data.
        """

        now = datetime.now()
        params = dict(date=date(now.year, now.month, now.day))

        if config.live_data_available:
            # No need to filter or anything - live_data only contains one record.
            current_data = db.query("""select timetz(download_timestamp) as time_stamp,
                    invalid_data, relative_humidity, temperature, dew_point,
                    wind_chill, apparent_temperature, absolute_pressure,
                    average_wind_speed, gust_wind_speed, wind_direction
                    from live_data""")[0]
            current_data_ts = current_data.time_stamp
        else:
            # Fetch the latest data for today
            current_data = db.query("""select timetz(time_stamp) as time_stamp, relative_humidity,
                    temperature,dew_point, wind_chill, apparent_temperature,
                    absolute_pressure, average_wind_speed, gust_wind_speed,
                    wind_direction, invalid_data
                from sample
                where date(time_stamp) = $date
                order by time_stamp desc
                limit 1""",params)[0]
            current_data_ts = current_data.time_stamp

        return current_data_ts, current_data


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
            current_data = db.query("""select timetz(download_timestamp) as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
                from live_data""")[0]
            current_data_ts = current_data.time_stamp
        else:
            # Fetch the latest data for today
            current_data = db.query("""select timetz(time_stamp) as time_stamp,
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
        :return: Records for the year.
        :raise: web.NotFound() if there is no data for the year.
        """
        params = dict(year=year)
        yearly_records = db.select('yearly_records',params,
                                   where='year_stamp=$year')

        if not len(yearly_records):
            # Bad url or something.
            raise web.NotFound()

        return yearly_records[0]
