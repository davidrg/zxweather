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

def get_daily_records(date):
    """
    Gets the records for the day. If there is no data for the day, None
    is returned.
    :param date: Day to get data for.
    :return: None or record data.
    """
    params = dict(date=date)
    daily_records = db.query("""SELECT date_stamp, total_rainfall, max_gust_wind_speed, max_gust_wind_speed_ts::time,
   max_average_wind_speed, max_average_wind_speed_ts::time, min_absolute_pressure,
   min_absolute_pressure_ts::time, max_absolute_pressure, max_absolute_pressure_ts::time,
   min_apparent_temperature, min_apparent_temperature_ts::time, max_apparent_temperature,
   max_apparent_temperature_ts::time, min_wind_chill, min_wind_chill_ts::time,
   max_wind_chill, max_wind_chill_ts::time, min_dew_point, min_dew_point_ts::time,
   max_dew_point, max_dew_point_ts::time, min_temperature, min_temperature_ts::time,
   max_temperature, max_temperature_ts::time, min_humidity, min_humidity_ts::time,
   max_humidity, max_humidity_ts::time
FROM daily_records
WHERE date_stamp = $date""", params)

    if not len(daily_records):
        return None
    else:
        return daily_records[0]

def get_daily_rainfall(date):
    """
    Gets rainfall for the specified day and the past seven days
    :param date: Date to get rainfall for
    :type date: date
    :return: dictionary containing rainfall and 7day rainfall
    :rtype: dict
    """
    params = dict(date=date)
    day_rainfall_record = db.query("""select sum(rainfall) as day_rainfall
from sample
where time_stamp::date = $date""", params)
    sevenday_rainfall_record = db.query("""select sum(rainfall) as sevenday_rainfall
from sample s,
     (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
  and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))""", params)

    if len(day_rainfall_record) == 0 or len(sevenday_rainfall_record) == None:
        return {}

    return {
        'rainfall': day_rainfall_record[0].day_rainfall,
        '7day_rainfall': sevenday_rainfall_record[0].sevenday_rainfall,
    }

def get_latest_sample_timestamp():
    """
    Gets the timestamp of the most recent sample in the database
    :return: datetime
    """
    record = db.query("select max(time_stamp) as time_stamp from sample")
    return record[0].time_stamp