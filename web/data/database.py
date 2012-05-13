"""
Various common functions to get data from the database.
"""

from config import db
from datetime import datetime, date

__author__ = 'David Goodwin'

def day_exists(day):
    """
    Checks to see if a specific day exists in the database.
    :param day: Day to check for
    :type day: date or datetime
    :return: True if the day exists.
    """

    if isinstance(day, datetime):
        day = day.date()

    # Tomorrow never exists.
    if day > datetime.now().date():
        return False

    result = db.query("""select 42 from sample
    where date(time_stamp) = $date limit 1""", dict(date=day))
    if len(result):
        return True
    else:
        return False

def month_exists(year, month):
    """
    Checks to see if a specific month exists in the database.
    :param year: Year to check for
    :type year: integer
    :param month: Month to check for
    :type month: integer
    :return: True if the day exists.
    """

    month = date(year, month, 1)

    now = datetime.now().date()

    # Tomorrow never exists.
    if month > date(now.year,now.month,1):
        return False

    result = db.query("""select 42 from sample
    where date_trunc('month',date(time_stamp)) = $date limit 1""", dict(date=month))
    if len(result):
        return True
    else:
        return False

def year_exists(year):
    """
    Checks to see if a specific year exists in the database.
    :param year: Year to check for
    :type year: integer
    :return: True if the day exists.
    """

    d = date(year, 1, 1)

    now = datetime.now().date()

    # Tomorrow never exists.
    if d > date(now.year,1,1):
        return False

    result = db.query("""select 42 from sample
    where date_trunc('year',date(time_stamp)) = $date limit 1""", dict(date=d))
    if len(result):
        return True
    else:
        return False

def total_rainfall_in_last_7_days(end_date):
    """
    Gets the total rainfall for the 7 day period ending at the specified date.
    :param end_date: Final day in the 7 day period
    :return: Total rainfall for the 7 day period.
    """
    params = dict(date=end_date)
    result = db.query("""
    select sum(rainfall) as total_rainfall
from sample, (select max(time_stamp) as ts from sample where time_stamp::date = $date) as max_ts
where time_stamp <= max_ts.ts
  and time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))""", params)

    return result[0].total_rainfall