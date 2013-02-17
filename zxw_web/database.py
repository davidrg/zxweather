# coding=utf-8
"""
Various common functions to get data from the database.
"""

from config import db
from datetime import datetime, date

__author__ = 'David Goodwin'

def get_station_id(station):
    """
    Gets the ID for the specified station code.
    :param station: Station code
    :type station: str or unicode
    :return: The ID for the given station code
    :rtype: int
    """

    # TODO: Cache me

    result = db.query("select station_id from station where code = $code",
                      dict(code=station))
    if len(result):
        return result[0].station_id
    else:
        return None

def get_sample_interval(station_id):
    """
    Gets the sample interval for the specified station.
    :param station_id: ID of the station to get the sample interval for
    :type station_id: int
    :return: Sample interval
    :rtype: int
    """

    # TODO: Cache me. This can be done by get_station_id()

    result = db.query("select sample_interval "
                      "from station "
                      "where station_id = $station",
                      dict(station=station_id))
    if len(result):
        return result[0].sample_interval
    else:
        return None

def get_station_type_code(station_id):
    """
    Gets the hardware type code for the specified station.
    :param station_id: Station to look up the hardware type code for.
    :type station_id: int
    :return:
    """

    # TODO: Cache me. This can be done by get_station_id()

    query = """select st.code
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        where s.station_id = $station"""

    result = db.query(query, dict(station=station_id))
    if len(result):
        return result[0].code
    else:
        return None

def get_live_data_available(station_id):
    """
    Checks to see if live data is available for the specified station.
    :param station_id: The station to check the status of
    :type station_id: integer
    :return: If live data should be available for the station or not
    :rtype boolean:
    """

    # TODO: Cache me. This can be done by get_station_id()

    result = db.query("select live_data_available "
                      "from station "
                      "where station_id = $station",
                      dict(station=station_id))
    if len(result):
        return result[0].live_data_available
    else:
        return None

def day_exists(day, station_id):
    """
    Checks to see if a specific day exists in the database.
    :param day: Day to check for
    :type day: date or datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: True if the day exists.
    """
    # TODO: Cache me. This is called alot.
    if isinstance(day, datetime):
        day = day.date()

    # Tomorrow never exists.
    if day > datetime.now().date():
        return False

    result = db.query("""select 42 from sample
    where date(time_stamp) = $date
    and station_id = $station limit 1""", dict(date=day, station=station_id))
    if len(result):
        return True
    else:
        return False

def in_archive_mode(station_id):
    """
    Checks to see if the station is in archive mode (no current data).
    :param station_id: ID of the station to check
    :return: True if the station is in archive mode (no current data)
    :rtype: bool
    """
    # TODO: Cache me. This is called a lot.

    result = db.query("""select 42 from sample
    where date(time_stamp) = date(NOW()) or date(time_stamp) = date(NOW()) - '1 day'::interval
    and station_id = $station limit 1""", dict(station=station_id))

    if not len(result):
        return True
    else:
        return False

def month_exists(year, month, station_id):
    """
    Checks to see if a specific month exists in the database.
    :param year: Year to check for
    :type year: integer
    :param month: Month to check for
    :type month: integer
    :param station_id: The weather station to work with
    :type station_id: int
    :return: True if the day exists.
    """

    month = date(year, month, 1)

    now = datetime.now().date()

    # Tomorrow never exists.
    if month > date(now.year,now.month,1):
        return False

    result = db.query("""select 42 from sample
    where date_trunc('month',date(time_stamp)) = $date
    and station_id = $station limit 1""", dict(date=month, station=station_id))
    if len(result):
        return True
    else:
        return False

def year_exists(year, station_id):
    """
    Checks to see if a specific year exists in the database.
    :param year: Year to check for
    :type year: integer
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: True if the day exists.
    """

    d = date(year, 1, 1)

    now = datetime.now().date()

    # Tomorrow never exists.
    if d > date(now.year,1,1):
        return False

    result = db.query("""select 42 from sample
    where date_trunc('year',date(time_stamp)) = $date
    and station_id = $station limit 1""", dict(date=d, station=station_id))
    if len(result):
        return True
    else:
        return False

def total_rainfall_in_last_7_days(end_date, station_id):
    """
    Gets the total rainfall for the 7 day period ending at the specified date.
    :param end_date: Final day in the 7 day period
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Total rainfall for the 7 day period.
    """
    params = dict(date=end_date, station=station_id)
    result = db.query("""
    select sum(rainfall) as total_rainfall
from sample, (
    select max(time_stamp) as ts from sample
    where time_stamp::date = $date
    and station_id = $station
) as max_ts
where time_stamp <= max_ts.ts
  and time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
  and station_id = $station""", params)

    return result[0].total_rainfall

def get_daily_records(date, station_id):
    """
    Gets the records for the day. If there is no data for the day, None
    is returned.
    :param date: Day to get data for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: None or record data.
    """
    params = dict(date=date, station=station_id)
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
WHERE date_stamp = $date
and station_id = $station""", params)

    if not len(daily_records):
        return None
    else:
        return daily_records[0]

def get_daily_rainfall(date, station_id):
    """
    Gets rainfall for the specified day and the past seven days
    :param date: Date to get rainfall for
    :type date: date
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: dictionary containing rainfall and 7day rainfall
    :rtype: dict
    """
    params = dict(date=date, station=station_id)
    day_rainfall_record = db.query("""select sum(rainfall) as day_rainfall
from sample
where time_stamp::date = $date
and station_id = $station""", params)

    sevenday_rainfall_record = db.query("""select sum(rainfall) as sevenday_rainfall
from sample s,
  (
     select max(time_stamp) as ts from sample
     where date(time_stamp) = $date
     and station_id = $station
  ) as max_ts
where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
  and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
  and s.station_id = $station""", params)

    if len(day_rainfall_record) == 0 or len(sevenday_rainfall_record) is None:
        return {}

    return {
        'rainfall': day_rainfall_record[0].day_rainfall,
        '7day_rainfall': sevenday_rainfall_record[0].sevenday_rainfall,
    }

def get_latest_sample_timestamp(station_id):
    """
    Gets the timestamp of the most recent sample in the database
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: datetime
    """
    record = db.query("""select max(time_stamp) as time_stamp
    from sample where station_id = $station""", dict(station=station_id))
    return record[0].time_stamp

def get_oldest_sample_timestamp(station_id):
    """
    Gets the timestamp of the least recent sample in the database
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: datetime
    """
    record = db.query("""select min(time_stamp) as time_stamp
        from sample where station_id = $station""", dict(station=station_id))
    return record[0].time_stamp

def get_live_data(station_id):
    """
    Gets live data from the database. If config.live_data_available is set
    then the data will come from the live data table and will be at most
    48 seconds old. If it is not set then the data returned will be the
    most recent sample from the sample table.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Timestamp for the data and the actual data.
    """

    now = datetime.now()
    params = dict(date=date(now.year, now.month, now.day), station=station_id)

    hw_type = get_station_type_code(station_id)

    if get_live_data_available(station_id):
        # No need to filter or anything - live_data only contains one record.
        base_query = """
            select ld.download_timestamp::time as time_stamp,
                   ld.relative_humidity,
                   ld.temperature,
                   ld.dew_point,
                   ld.wind_chill,
                   ld.apparent_temperature,
                   ld.absolute_pressure,
                   ld.average_wind_speed,
                   ld.wind_direction,
                   extract('epoch' from (now() - ld.download_timestamp)) as age
                   {ext_columns}
            from live_data ld{ext_joins}
            where ld.station_id = $station"""

        ext_columns = ''
        ext_joins = ''

        if hw_type == 'DAVIS':
            ext_columns = """,
                   dd.bar_trend,
                   dd.rain_rate,
                   dd.storm_rain,
                   dd.current_storm_start_date,
                   dd.transmitter_battery,
                   dd.console_battery_voltage,
                   dd.forecast_icon,
                   dd.forecast_rule_id """

            ext_joins = """
            inner join davis_live_data dd on dd.station_id = ld.station_id """

        query = base_query.format(ext_columns=ext_columns, ext_joins=ext_joins)

        result = db.query(query, dict(station=station_id))
        if len(result):
            current_data = result[0]
            current_data_ts = current_data.time_stamp
        else:
            return None, None, hw_type
    elif hw_type == 'DAVIS':
        # Live data for Davis weather stations can't be faked by taking the
        # most recent sample. This is because most of the davis-specific
        # fields are only available in live data.
        return None, None, hw_type
    else:
        # Fetch the latest data for today
        current_data = db.query("""select time_stamp::time as time_stamp, relative_humidity,
                    temperature,dew_point, wind_chill, apparent_temperature,
                    absolute_pressure, average_wind_speed,
                    wind_direction,
                    extract('epoch' from (now() - download_timestamp)) as age
                from sample
                where date(time_stamp) = $date
                and station_id = $station
                order by time_stamp desc
                limit 1""", params)[0]
        current_data_ts = current_data.time_stamp

    return current_data_ts, current_data, hw_type

def get_years(station_id):
    """
    Gets a list of years in the database.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: A list of years with data in the database
    :rtype: [integer]
    """
    years_result = db.query(
        """select distinct extract(year from time_stamp) as year
        from sample
        where station_id = $station
        order by year desc""", dict(station=station_id))
    years = []
    for record in years_result:
        years.append(int(record.year))

    return years

def get_live_indoor_data(station_id):
    """
    Gets live indoor data from the database. If config.live_data_available
    is set then the data will come from the live data table and will be at
    most 48 seconds old. If it is not set then the data returned will be
    the most recent sample from the sample table.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Timestamp for the data and the actual data.
    """
    now = datetime.now()
    params = dict(date=date(now.year, now.month, now.day), station=station_id)

    if get_live_data_available(station_id):
        current_data = db.query("""select download_timestamp::time as time_stamp,
            indoor_relative_humidity,
            indoor_temperature
            from live_data
            where station_id = $station""", dict(station=station_id))[0]
    else:
        # Fetch the latest data for today
        current_data = db.query("""select time_stamp::time as time_stamp,
            indoor_relative_humidity,
            indoor_temperature
        from sample
        where date(time_stamp) = $date
        and station_id = $station
        order by time_stamp desc
        limit 1""",params)[0]

    return current_data

def get_station_name(station_id):
    """
    Gets the name of the station
    :param station_id: The ID of the station to get data for
    :type station_id: int
    :return: Station name
    :rtype: str
    """

    result = db.query("select title "
                      "from station "
                      "where station_id = $station",
        dict(station=station_id))
    if len(result):
        return result[0].title
    else:
        return None