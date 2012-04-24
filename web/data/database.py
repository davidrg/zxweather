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