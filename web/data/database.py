"""
Various common functions to get data from the database.
"""

from config import db
from datetime import datetime

__author__ = 'David Goodwin'

def day_exists(date):
    """
    Checks to see if a specific day exists in the database.
    :param date: Day to check for
    :type date: date or datetime
    :return: True if the day exists.
    """

    if isinstance(date, datetime):
        date = date.date()

    # Tomorrow never exists.
    if date > datetime.now().date():
        return False

    result = db.query("""select 42 from sample
    where date(time_stamp) = $date limit 1""", dict(date=date))
    if len(result):
        return True
    else:
        return False