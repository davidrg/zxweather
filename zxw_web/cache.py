"""
Functions for applying cache-control headers.
"""

from datetime import date, datetime, time, timedelta
import web
from config import db
import config
from data.util import rfcformat

__author__ = 'David Goodwin'

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
    cache_control_headers(data_age, year)

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
    cache_control_headers(data_age, year, month)

def day_cache_control(data_age, day):
    """
    Gets cache control headers for a day page.
    :param data_age: Age of the data if known (if not it will be looked up
                     in the database as the most recent sample for that day).
    :type data_age: datetime.datetime or None
    :param day: Page day
    :type day: date
    """
    if data_age is None:
        age_data = db.query("select max(time_stamp) as max_ts from sample where "
                            "time_stamp::date = $date", dict(date = day))
        data_age = age_data[0].max_ts
    elif isinstance(data_age, time):
        # We already have the _time_ for the page - just need to add the
        # date on.
        data_age = datetime.combine(day, data_age)
    cache_control_headers(data_age, day.year, day.month, day.day)