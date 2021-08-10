# coding=utf-8
"""
Various common functions to get data from the database.
"""
import json

from config import db
from datetime import datetime, date, timedelta
import config

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

    result = db.query("select station_id from station where upper(code) = upper($code)",
                      dict(code=station))
    if len(result):
        return result[0].station_id
    else:
        return None


def get_station_code(station):
    """
    Gets the code for the specified station id.
    :param station: Station id
    :type station: int
    :return: The code for the given station code
    :rtype: str
    """

    # TODO: Cache me

    result = db.query("select upper(code) as code from station where station_id = $station",
                      dict(station=station))
    if len(result):
        return result[0].code
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


def get_davis_max_wireless_packets(station_id):
    """
    Gets the number of samples we should have received from the ISS with 100%
    reception. To calculate this we need the ID the station broadcasts on
    which comes from the web interfaces config file.

    :param station_id: Station to get the wireless packet count.
    :return: Packet count or None if not configured.
    """

    station_code = get_station_code(station_id)

    hw_config = get_station_config(station_id)

    broadcast_id = hw_config['broadcast_id']

    if broadcast_id is None:
        # The station broadcast ID hasn't been set for this station. Can't
        # compute the number of samples.
        return None

    # This query is correct for a Vantage Pro2 and a Vantage Vue. The
    # original Vantage Pro uses a different formula.
    query = """select 	(stn.sample_interval::float)
	    /
	((41+$id-1)::float
	   /16.0
	) as max_packets
    from station stn where upper(stn.code) = upper($station)
    """

    result = db.query(query, dict(id=broadcast_id,
                                  station=station_code))

    if len(result):
        return result[0].max_packets
    return None


def get_station_type_code(station_id):
    """
    Gets the hardware type code for the specified station.
    :param station_id: Station to look up the hardware type code for.
    :type station_id: int
    :return:
    """

    # TODO: Cache me. This can be done by get_station_id()

    query = """select upper(st.code) as code
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        where s.station_id = $station"""

    result = db.query(query, dict(station=station_id))
    if len(result):
        return result[0].code
    else:
        return None


def station_archived_status(station_id):
    query = """select archived, archived_message, archived_time 
    from station where station_id = $station_id"""
    result = db.query(query, dict(station_id=station_id))

    return result[0]

def get_station_config(station_id):
    """
    Returns the hardware configuration parameters for the specified station.
    :param station_id: Station to get hardware config for.
    :return: Hardware configuration parameters. Exactly what this is depends
    entirely on the hardware type.
    """
    query = "select station_config from station where station_id = $station"

    result = db.query(query, dict(station=station_id))

    if len(result):
        data = result[0].station_config
        if data is None:
            return None
        return json.loads(data)
    return None


def get_permanent_data_gaps(station_id):
    """
    Returns the set of known permanent gaps in the stations dataset. These are
    marked via the admin-tool and used by the desktop client to know if a
    received samples data file with detected gaps can be cached permanently or
    not.
    """
    query = "select  start_time, end_time, missing_sample_count, label " \
            "from sample_gap where station_id = $station_id"

    result = db.query(query, dict(station_id=station_id))

    return result


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
    where (date(time_stamp) = date(NOW()) or date(time_stamp) = date(NOW()) - '1 day'::interval)
    and station_id = $station limit 1""", dict(station=station_id))

    if not len(result):
        return True
    else:
        return False


def no_data_in_24_hours(station_id):
    """
    Checks to see if there is no data for the last 24 hours
    :param station_id: ID of the station to check
    :return: True if the station is missing data for the last 24 hours
    :rtype: bool
    """

    # TODO: Cache me. This is called a lot.

    result = db.query("""select 42 from sample
    where time_stamp >= (NOW() - '24 hours'::interval)
    and station_id = $station limit 1""", dict(station=station_id))

    return len(result) == 0


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

def get_daily_rainfall(time, station_id, use_24hr_range=False):
    """
    Gets rainfall for the specified day and the past seven days
    :param time: Date or time to get rainfall totals for
    :type time: date or datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :param use_24hr_range: If a 24-hour range from the supplied time should be
        used rather than the exact date
    :type use_24hr_range: bool
    :return: dictionary containing rainfall and 7day rainfall
    :rtype: dict
    """
    params = dict(time=time, station=station_id)
    if not use_24hr_range:
        day_rainfall_record = db.query("""select sum(rainfall) as day_rainfall
    from sample
    where time_stamp::date = $time
    and station_id = $station""", params)
        sevenday_rainfall_record = db.query("""select sum(rainfall) as sevenday_rainfall
from sample s,
  (
     select max(time_stamp) as ts from sample
     where date(time_stamp) = $time::date
     and station_id = $station
  ) as max_ts
where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
  and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
  and s.station_id = $station""", params)
    else:
        day_rainfall_record = db.query("""select sum(rainfall) as day_rainfall
    from sample
    where (time_stamp < $time and time_stamp > $time - '1 hour'::interval * 24)
    and station_id = $station""", params)

        sevenday_rainfall_record = db.query("""select sum(rainfall) as sevenday_rainfall
from sample s
where s.time_stamp <= $time     -- 604800 seconds in a week.
  and s.time_stamp >= ($time - (604800 * '1 second'::interval))
  and s.station_id = $station""", params)

    if len(day_rainfall_record) == 0 or len(sevenday_rainfall_record) is None:
        return {}

    return {
        'rainfall': day_rainfall_record[0].day_rainfall,
        '7day_rainfall': sevenday_rainfall_record[0].sevenday_rainfall,
    }


def get_last_hour_rainfall(station_id):
    params = dict(station=station_id)

    query = """
    select sum(s.rainfall) as total
from sample s
where s.time_stamp > (select max(time_stamp) from sample where station_id = $station) - '1 hour'::interval
  and s.station_id = $station
group by s.station_id
  """

    result = db.query(query, params)

    return result[0].total


def get_day_rainfall(date, station_id):
    """
    Gets rainfall for the specified day and the past seven days
    :param time: Date or time to get rainfall totals for
    :type time: date or datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :param use_24hr_range: If a 24-hour range from the supplied time should be
        used rather than the exact date
    :type use_24hr_range: bool
    :return: dictionary containing rainfall and 7day rainfall
    :rtype: dict
    """
    params = dict(date=date, station=station_id)

    rainfall = db.query("""select s.station_id,
       s.time_stamp::date as date,
       sum(rainfall) as total
from sample s
where s.time_stamp::date = $date
  and s.station_id = $station
group by s.station_id, s.time_stamp::date""", params)

    if rainfall is None or len(rainfall) == 0:
        return 0

    return rainfall[0].total


def get_month_rainfall(year, month, station_id):
    """
    Gets rainfall for the specified month
    """
    params = dict(year=year, month=month, station=station_id)

    rainfall = db.query("""select s.station_id,
       extract('year' from s.time_stamp) as year,
       extract('month' from s.time_stamp) as month,
       sum(rainfall) as total
from sample s
where extract('year' from s.time_stamp) = $year
  and extract('month' from s.time_stamp) = $month
  and s.station_id = $station
group by s.station_id, extract('year' from s.time_stamp),
extract('month' from s.time_stamp)""", params)

    if rainfall is None or len(rainfall) == 0:
        return 0

    return rainfall[0].total


def get_year_rainfall(year, station_id):

    params = dict(year=year,station=station_id)

    # The inner part of this query is the same as for get_month_rainfall
    query = """
    select inr.station_id,
       inr.year,
       sum(inr.total) as total
from (
select s.station_id,
       extract('year' from s.time_stamp) as year,
       extract('month' from s.time_stamp) as month,
       sum(rainfall) as total
from sample s
group by s.station_id, extract('year' from s.time_stamp), extract('month' from s.time_stamp)
) as inr
where inr.station_id = $station
  and inr.year = $year
group by inr.station_id, inr.year
    """

    result = db.query(query, params)
    if result is None or len(result) == 0:
        return 0

    return result[0].total


def get_rain_summary(station_id):
    """
    Gets a summary of rainfall over five relative periods: today, yesterday,
    this week, this month and this year. Any periods that lack rainfall are
    omitted from the results.

    :param station_id: Station to get rainfall summary data for
     :type station_id: int
    """
    params = dict(station=station_id)

    query = """
select periods.period,
       periods.start_time,
       periods.end_time,
       sum(sample.rainfall) as rainfall
from sample
join (
select 'today'      as period, date_trunc('day', NOW())                                                     as start_time, NOW() as end_time union all
select 'yesterday'  as period, date_trunc('day', NOW() - '1 day'::interval)                                 as start_time, date_trunc('day', NOW()) - '1 microsecond'::interval as end_time union all
select 'this_week'  as period, current_date - ((extract(dow from current_date))::text || ' days')::interval as start_time, NOW() as end_time union all
select 'this_month' as period, date_trunc('month', NOW())                                                   as start_time, NOW() as end_time union all
select 'this_year'  as period, date_trunc('year', NOW())                                                    as start_time, NOW() as end_time
    ) as periods on sample.time_stamp between periods.start_time and periods.end_time
where sample.station_id = $station
group by periods.period, periods.start_time, periods.end_time
    """

    result = db.query(query, params)

    if result is None or len(result) == 0:
        return None

    return result


def get_latest_sample_timestamp(station_id):
    """
    Gets the timestamp of the most recent sample in the database. This just
    wraps get_sample_range().
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: datetime
    """
    min_ts, max_ts = get_sample_range(station_id)

    return max_ts


def get_oldest_sample_timestamp(station_id):
    """
    Gets the timestamp of the least recent sample in the database. This just
    wraps get_sample_range().
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: datetime
    """
    min_ts, max_ts = get_sample_range(station_id)

    return min_ts


def get_sample_range(station_id):
    """
    Gets the minimum and maximum sample timestamps in the database for the
    specified station.
    :param station_id: The ID of the weather station to work with
    :return: datetime, datetime
    """
    records = db.query("select min(time_stamp) as min_ts, "
                      "max(time_stamp) as max_ts "
                      "from sample "
                      "where station_id=$station", dict(station=station_id))

    record = records[0]

    return record.min_ts, record.max_ts



# def get_latest_sample(station_id):
#     """
#     Returns the most recent sample for the specified station
#     :param station_id: ID of the station to get the most recent sample for
#     :return: Sample data
#     """
#     query = """
# select s.sample_id,
#        s.time_stamp,
#        s.indoor_relative_humidity as indoor_humidity,
#        s.indoor_temperature,
#        s.relative_humidity as humidity,
#        s.temperature,
#        s.dew_point,
#        s.wind_chill,
#        s.apparent_temperature,
#        s.absolute_pressure as pressure,
#        s.average_wind_speed,
#        s.gust_wind_speed,
#        s.rainfall,
#        ds.solar_radiation,
#        ds.average_uv_index as uv_index
# from sample s
# left outer join davis_sample ds on ds.sample_id = s.sample_id
# where s.station_id = $station_id
#   and s.time_stamp = (select max(time_stamp) from sample where station_id = $station_id)
#     """
#     result = db.query(query, station_id=station_id)[0]
#     if len(result):
#         return result[0]
#     else:
#         return None


def get_images_in_last_minute(station_id):
    query = """
select i.image_id,
       i.time_stamp,
       i.title,
       i.description,
       it.code as type_code,
       i.mime_type,
       src.code as source_code
from image i
inner join image_type it on it.image_type_id = i.image_type_id
inner join image_source src on src.image_source_id = i.image_source_id
where src.station_id = $station
  and time_stamp >= NOW() - '1 minute'::interval
    """

    result = db.query(query, dict(station=station_id))
    return result


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
                   ld.download_timestamp::date as date_stamp,
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
                   dd.forecast_rule_id,
                   dd.uv_index,
                   dd.solar_radiation,
                   dd.leaf_wetness_1,
                   dd.leaf_wetness_2,
                   dd.leaf_temperature_1,
                   dd.leaf_temperature_2,
                   dd.soil_moisture_1,
                   dd.soil_moisture_2,
                   dd.soil_moisture_3, 
                   dd.soil_moisture_4,
                   dd.soil_temperature_1,
                   dd.soil_temperature_2,
                   dd.soil_temperature_3,
                   dd.soil_temperature_4,
                   dd.extra_humidity_1,
                   dd.extra_humidity_2,
                   dd.extra_temperature_1,
                   dd.extra_temperature_2,
                   dd.extra_temperature_3"""

            ext_joins = """
            inner join davis_live_data dd on dd.station_id = ld.station_id """

        query = base_query.format(ext_columns=ext_columns, ext_joins=ext_joins)

        result = db.query(query, dict(station=station_id))
        if len(result):
            current_data = result[0]
            current_data_ts = current_data.time_stamp
        else:
            return None, None, hw_type
    #elif hw_type == 'DAVIS':
        # Live data for Davis weather stations can't be faked by taking the
        # most recent sample. This is because most of the davis-specific
        # fields are only available in live data.
    #    return None, None, hw_type
    else:
        # Fetch the latest data for today
        query = """
                select s.time_stamp::time as time_stamp,
                    s.relative_humidity,
                    s.temperature,
                    s.dew_point,
                    s.wind_chill,
                    s.apparent_temperature,
                    s.absolute_pressure,
                    s.average_wind_speed,
                    s.wind_direction,
                    extract('epoch' from (now() - s.download_timestamp)) as age
                    {ext_columns}
                from sample s {ext_joins}
                where date(s.time_stamp) = $date
                and s.station_id = $station
                order by s.time_stamp desc
                limit 1"""

        ext_columns = ""
        ext_joins = ""

        if hw_type == 'DAVIS':
            ext_columns = """,
            0 as bar_trend,
            ds.high_rain_rate as rain_rate,
            0 as storm_rain,
            null as current_storm_start_date,
            0 as transmitter_battery,
            0 as console_battery_voltage,
            0 as forecast_icon,
            ds.forecast_rule_id,
            ds.average_uv_index as uv_index,
            ds.solar_radiation
            """
            ext_joins = """
            inner join davis_sample ds on ds.sample_id = s.sample_id
            """

        query = query.format(ext_columns=ext_columns, ext_joins=ext_joins)

        current_data = db.query(query, params)[0]
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


def get_full_station_info(include_coordinates=True):
    """
    Gets full information for all stations in the database - code, name,
    description, hardware type and date ranges.
    :param include_coordinates: If station coordinates should be included
    :type include_coordinates: bool
    :return:
    """

    result = db.query("""
        select upper(s.code) as code, s.title, s.description, s.sort_order,
           upper(st.code) as hw_type_code, st.title as hw_type_name,
           sr.min_ts, sr.max_ts, s.message,
           s.message_timestamp, s.station_config, s.site_title, s.latitude,
           s.longitude, s.altitude, s.sample_interval, s.archived, s.archived_message, s.archived_time
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        left outer join (
            select station_id, min(time_stamp) min_ts, max(time_stamp) max_ts
            from sample
            group by station_id) as sr on sr.station_id = s.station_id
    """)

    stations = []
    for record in result:

        hw_config = None

        if record.station_config is not None:
            hw_config = json.loads(record.station_config)

        station = {
            'code': record.code,
            'name': record.title,
            'desc': record.description,
            'order': record.sort_order,
            'msg': record.message,
            'msg_ts': None,
            'hw_type': {
                'code': record.hw_type_code,
                'name': record.hw_type_name
            },
            'range': {
                'min': None,
                'max': None
            },
            'hw_config': hw_config,
            'site_title': record.site_title,
            'coordinates': {
                'latitude': None,
                'longitude': None,
                'altitude': record.altitude
            },
            'interval': record.sample_interval,
            'is_archived': record.archived
        }

        if record.archived:
            station['archived'] = {
                "time": record.archived_time.isoformat(),
                "message": record.archived_message
            }

        if include_coordinates:
            station['coordinates']['latitude'] = record.latitude
            station['coordinates']['longitude'] = record.longitude

        if record.message_timestamp is not None:
            station['msg_ts'] = record.message_timestamp.isoformat()
        if record.min_ts is not None:
            station['range']['min'] = record.min_ts.isoformat()
        if record.max_ts is not None:
            station['range']['max'] = record.max_ts.isoformat()

        stations.append(station)
    return stations


def get_extra_sensors_enabled(station_id):
    """
    Returns true if the specified station has any extra sensors enabled at all.
    An extra sensor is one of: Leaf Wetness 1-2, Leaf Temperature 1-2,
    Soil Moisture 1-4, Soil Temperature 1-4, Extra Humidity 1-2,
    Extra Temperature 1-3

    :param station_id: Station ID
    :type station_id: int
    :return: True if an extra sensor is configured, false otherwise.
    :rtype: bool
    """
    result = db.query("select station_config from station "
                      "where station_id = $station and station_config is not null",
                      dict(station=station_id))

    if len(result) == 0:
        return False

    hw_config = json.loads(result[0].station_config)

    if "sensor_config" not in hw_config:
        return False

    extra_sensor_names = [
        "leaf_wetness_1", "leaf_wetness_2",
        "leaf_temperature_1", "leaf_temperature_2",
        "soil_moisture_1", "soil_moisture_2", "soil_moisture_3",
        "soil_moisture_4",
        "soil_temperature_1", "soil_temperature_2", "soil_temperature_3",
        "soil_temperature_4",
        "extra_humidity_1", "extra_humidity_2",
        "extra_temperature_1", "extra_temperature_2", "extra_temperature_3"
    ]

    sensor_config = hw_config["sensor_config"]

    for sensor in extra_sensor_names:
        if sensor in sensor_config:
            if "enabled" in sensor_config[sensor] and sensor_config[sensor]["enabled"]:
                return True

    return False




def get_stations():
    """
    Gets a list of station code,name pairs for all stations in the database.
    :return:
    """
    result = db.query("select upper(code) as code, title, archived "
                      "from station order by archived, sort_order asc, title desc")

    stations = []

    for row in result:
        station = (row.code, row.title, row.archived)
        stations.append(station)

    return stations


def get_station_message(station_id):
    """
    Gets the message (if any) for the specified station.
    :param station_id:
    :return:
    """

    result = db.query("select message, "
                      "    extract(epoch from message_timestamp)::integer as ts"
                      "  from station "
                      " where station_id = $station",
                      dict(station=station_id))[0]


    return result.message, result.ts

def get_site_name(station_id):
    """
    Returns the site name
    :param station_code: Station code to get the site name for
    """

    # TODO: Cache me

    if station_id is None:
        return config.site_name

    result = db.query("select site_title from station "
                      "where station_id = $station and site_title is not null",
                      dict(station=station_id))

    if len(result) == 0:
        return config.site_name

    return result[0].site_title


def get_image_sources_for_station(station_id):
    """
    Returns a list of all image sources for the specified station ID.

    :param station_id: Station to get image sources for
    :type station_id: int
    :return: List of image sources
    """

    items = [
        x.upper()
        for x in map(str.strip, config.image_type_sort.split(","))
        if x != ""
    ]

    sort_list = "{" + ",".join(items) + "}"

    qry = """
select src.image_source_id, upper(src.code) as code, src.source_name,
        src.description, ( select i.image_id
           from image as i
           inner join (
                select it2.image_type_id,
                       upper(it2.code) as code,
                       coalesce(array_position(($sort_list)::character varying[], upper(it2.code)::character varying), 1000) as sort_order
                from image_type it2
           ) as it on it.image_type_id = i.image_type_id
           where i.image_source_id = src.image_source_id
           order by i.time_stamp desc, it.sort_order asc, i.title desc
           limit 1
       ) as last_image_id
from image_source src where src.station_id = $station"""

    # Ugh. At least this can disappear when support for Postgres 9.1-9.4 does.
    if not config.array_position_available:
        qry = qry.replace("array_position", "array_idx")

    result = db.query(qry, dict(station=station_id, sort_list=sort_list,))
    if len(result) == 0:
        return None

    return result


def get_image_id(source, type, time_stamp):
    query = """
    select img.image_id
    from image img
    inner join image_source img_src on img.image_source_id = img_src.image_source_id
    inner join image_type img_typ on img.image_type_id = img_typ.image_type_id
    where date_trunc('second', img.time_stamp) = $ts
      and upper(img_typ.code) = upper($type_code)
      and upper(img_src.code) = upper($source_code)
    """

    result = db.query(query, dict(ts=time_stamp, type_code=type,
                                  source_code=source))
    if len(result):
        return result[0].image_id
    return None


def get_image_type_and_ts_info(image_id):
    """
    Gets the image type and timestamp for the specified image. This is used
    to redirect from an old data route to the new one.

    :param image_id: ID to get the image data for
    """

    query = """
    select img.time_stamp,
           upper(img_typ.code) as code
    from image img
    inner join image_type img_typ on img_typ.image_type_id = img.image_type_id
    where image_id = $image_id
    """

    result = db.query(query, dict(image_id=image_id))
    if len(result):
        return result[0]
    return None


def get_image_source_info(station_id):
    items = [
        x.upper()
        for x in map(str.strip, config.image_type_sort.split(","))
        if x != ""
    ]

    sort_list = "{" + ",".join(items) + "}"

    query = """
 select upper(x.code) as code,
       x.source_name,
       x.description,
       x.first_image,
       last_image.time_stamp as last_image,
       -- An image source is considered active if it has images taken
       -- within the last 24 hours.
       case when NOW() - coalesce(last_image.time_stamp,
                                  NOW() - '1 week'::interval)
                 > '24 hours'::interval
            then FALSE
            else TRUE
            end as is_active,
       x.last_image_id,
       last_image.time_stamp as last_image_time_stamp,
       upper(last_image_type.code) as last_image_type_code,
       last_image.title as last_image_title,
       last_image.description as last_image_description,
       last_image.mime_type as last_image_mime_type,
       last_image.metadata is not null as last_image_has_metadata
from (
    select upper(img_src.code) as code,
           img_src.source_name,
           img_src.description,
           (
               select min(i.time_stamp) as min_ts
               from image as i
               where i.image_source_id = img_src.image_source_id
           ) as first_image,
           (
               select i.image_id
               from image as i
               inner join (
                    select it2.image_type_id,
                           upper(it2.code) as code,
                           coalesce(array_position(($sort_list)::character varying[], upper(it2.code)::character varying), 1000) as sort_order
                    from image_type it2
               ) as it on it.image_type_id = i.image_type_id
               where i.image_source_id = img_src.image_source_id
               order by i.time_stamp desc, it.sort_order asc, i.title desc
               limit 1
           ) as last_image_id,
           img_src.station_id
    from image_source img_src
) as x
inner join image last_image on last_image.image_id = x.last_image_id
inner join image_type last_image_type on last_image_type.image_type_id = last_image.image_type_id
where x.station_id = $station
    """

    # Ugh. At least this can disappear when support for Postgres 9.1-9.4 does.
    if not config.array_position_available:
        query = query.replace("array_position", "array_idx")

    result = db.query(query, dict(station=station_id, sort_list=sort_list))

    if len(result) == 0:
        return None

    return result


def get_image_sources_by_date(station_id):
    """
    Gets a list of dates that have images available. For each date a list of
    image sources with images for that date .

    :param station_id: ID of the station to look up image dates for
     :type station_id: int
    :return:
    """
    query = """
select inr.date_stamp as date_stamp,
		-- string_agg(inr.mime_type, ',') as mime_types,
		string_agg(upper(inr.src_code), ',') as image_source_codes
from (
	select distinct
			img.time_stamp::date as date_stamp,
			-- img.mime_type,
			upper(img_src.code) as src_code,
			img_src.source_name as src_name
	from image img
	inner join image_source img_src on img_src.image_source_id = img.image_source_id
	where img_src.station_id = $station) as inr
group by inr.date_stamp
    """

    result = db.query(query, dict(station=station_id))

    if len(result) == 0:
        return None

    return result


def get_image_source(station_id, source_code):

    result = db.query("""select image_source_id, source_name
from image_source where upper(code) = upper($source) and station_id = $station""",
                      dict(source=source_code, station=station_id))
    if len(result):
        return result[0]
    return None


def get_image_source_date_counts(station_id):
    """
    Similar to image sources by date but gives the number of images for each
    source on each date.
    :param station_id: Station ID to get data for
    """
    count_query = """
    select img.time_stamp::date as date_stamp,
           upper(img_src.code) as src_code,
           count(*) as date_source_count
    from image img
    inner join image_source img_src on img.image_source_id = img_src.image_source_id
    where img_src.station_id = $station
    group by img.time_stamp::date, upper(img_src.code)
    order by img.time_stamp::date, upper(img_src.code)
    """

    result = db.query(count_query, dict(station=station_id))

    if len(result) == 0:
        return None

    dates = dict()
    for row in result:
        date_stamp = row.date_stamp.isoformat()
        if date_stamp not in dates:
            dates[date_stamp] = dict()
        dates[date_stamp][row.src_code] = row.date_source_count

    return dates


def get_day_images_for_source(source_id, image_date=None):
    """
    Returns a list of all images for the specified image source and date.
    :param source_id: Image source
    :type source_id: int
    :param image_date: Date to fetch images for
    :type image_date: date
    :return: A list of available images
    """

    query = """
    select i.image_id as id,
           i.time_stamp,
           i.mime_type,
           it.type_name as type_name,
           upper(it.code) as type_code,
           i.title,
           i.description,
           case when i.metadata is null or i.metadata = '' then False else True end as has_metadata
    from image i
    inner join image_type it on it.image_type_id = i.image_type_id
    where i.image_source_id = $source_id
      and (i.time_stamp::date = $date or $date is null)
    order by i.time_stamp asc
    """

    result = db.query(query, dict(source_id=source_id, date=image_date))
    return result


def get_latest_image_ts_for_source(source_id, image_date=None):
    query = """
    select max(time_stamp) as ts
    from image i
    where i.image_source_id = $source_id
      and (i.time_stamp::date = $date or $date is null)
    """

    result = db.query(query, dict(source_id=source_id, date=image_date))

    if result is None:
        return None
    return result[0].ts


def get_most_recent_image_ts_for_source_and_date(source_id, image_date):
    """
    Returns the timestamp for the most recent image on the specified date for
    the specified source. Used for cache control purposes.

    :param source_id: Image source
    :type source_id: int
    :param image_date: Date to find the most recent timestamp for
    :type image_date: date
    :return: Time of most recent image for the date and source
    """

    query = """
    select max(i.time_stamp) as ts
    from image i
    inner join image_type it on it.image_type_id = i.image_type_id
    where i.image_source_id = $source_id
      and i.time_stamp::date = $date
    """

    result = db.query(query, dict(source_id=source_id, date=image_date))
    return result[0].ts


def image_exists(station, image_date, source_code, image_id):
    query = """
    select image_id
    from image i
    inner join image_source src on src.image_source_id = i.image_source_id
    inner join station stn on stn.station_id = src.station_id
    where upper(stn.code) = upper($station_code)
      and i.time_stamp::date = $image_date
      and upper(src.code) = upper($source_code)
      and i.image_id = $image_id
        """

    result = db.query(query, dict(station_code=station, image_date=image_date,
                                  source_code=source_code,
                                  image_id=image_id))
    if len(result):
        return True
    return False


def get_image_metadata(image_id):
    result = db.query("select metadata, time_stamp "
                      "from image where image_id = $id",
                      dict(id=image_id))
    if len(result):
        return result[0]
    return None


def get_image_details(image_id):
    result = db.query("select title, description, time_stamp "
                      "from image where image_id = $id",
                      dict(id=image_id))
    if len(result):
        return result[0]
    return None


def get_image(image_id):
    result = db.query("select image_data, mime_type, time_stamp "
                      "from image where image_id = $id",
                      dict(id=image_id))
    if len(result):
        return result[0]
    return None


def get_image_mime_type(image_id):
    result = db.query("select mime_type, time_stamp "
                      "from image where image_id = $id",
                      dict(id=image_id))
    if len(result):
        return result[0]
    return None


def get_images_for_source(source_id, day=None):

    items = [
        x.upper()
        for x in map(str.strip, config.image_type_sort.split(","))
        if x != ""
    ]

    sort_list = "{" + ",".join(items) + "}"

    query = """
    select i.time_stamp,
           upper(src.code) as source,
           i.image_id as id,
           i.mime_type,
           upper(stn.code) as station,
           upper(it.code) as type_code,
           i.title as title,
           i.description as description,
           it.sort_order as sort_order
    from image i
    inner join image_source src on src.image_source_id = i.image_source_id
    inner join (
        select image_type_id,
               upper(code) as code,
               coalesce(array_position(($sort_list)::character varying[], upper(code)::character varying), 1000) as sort_order
        from image_type
    ) as it on it.image_type_id = i.image_type_id
    inner join station stn on stn.station_id = src.station_id
    where src.image_source_id = $source_id
      and (i.time_stamp::date = $day or $day is null)
    order by i.time_stamp, it.sort_order, i.title
    """

    # Ugh. At least this can disappear when support for Postgres 9.1-9.4 does.
    if not config.array_position_available:
        query = query.replace("array_position", "array_idx")

    result = db.query(query, dict(source_id=source_id, day=day,
                                  sort_list=sort_list))

    if len(result):
        return result
    return None


def get_most_recent_image_id_for_source(source_id):
    items = [
        x.upper()
        for x in map(str.strip, config.image_type_sort.split(","))
        if x != ""
    ]

    sort_list = "{" + ",".join(items) + "}"

    query = """
    select image_id, time_stamp, mime_type
    from image i
    inner join (
        select image_type_id,
               upper(code) as code,
               coalesce(array_position(($sort_list)::character varying[], upper(code)::character varying), 1000) as sort_order
        from image_type
    ) as it on it.image_type_id = i.image_type_id
    where i.image_source_id = $source_id
      and i.mime_type ilike 'image/%'
    order by i.time_stamp desc, it.sort_order asc, i.title desc
    limit 1
    """

    # Ugh. At least this can disappear when support for Postgres 9.1-9.4 does.
    if not config.array_position_available:
        query = query.replace("array_position", "array_idx")

    result = db.query(query, dict(source_id=source_id, sort_list=sort_list))

    if len(result):
        return result[0]
    return None


def get_latest_sample(station_id):
    query = """
    select s.time_stamp,
           s.indoor_relative_humidity,
           s.indoor_temperature,
           s.relative_humidity,
           s.temperature,
           s.dew_point,
           s.wind_chill,
           s.apparent_temperature,
           s.absolute_pressure,
           s.average_wind_speed,
           s.gust_wind_speed,
           s.wind_direction,
           s.rainfall,
           case when s.temperature is null then true else false end as lost_sensor_contact,
           ds.high_temperature,
           ds.low_temperature,
           ds.high_rain_rate,
           ds.solar_radiation,
           ds.wind_sample_count,
           ds.gust_wind_direction,
           ds.average_uv_index,
           ds.high_solar_radiation,
           ds.high_uv_index,
           s.sample_id
    from sample s
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where s.station_id = $station
    order by s.time_stamp desc
    limit 1
    """

    result = db.query(query, dict(station=station_id))
    if len(result):
        return result[0]
    return None


def get_day_evapotranspiration(station_id):

    query = """
    select s.time_stamp::date,
    sum(ds.evapotranspiration) as evapotranspiration
    from davis_sample ds
    inner join sample s on s.sample_id = ds.sample_id
    where s.station_id = $station
    group by time_stamp::date
    order by time_stamp::date desc
    limit 1
    """

    result = db.query(query, dict(station=station_id))
    if len(result):
        return result[0].evapotranspiration
    return 0


def get_current_3h_trends(station_id):
    query = """
select s.station_id,
       (s.indoor_relative_humidity - s3h.indoor_relative_humidity) / 3 as indoor_humidity_trend,
       (s.indoor_temperature - s3h.indoor_temperature) / 3 as indoor_temperature_trend,
       (s.relative_humidity - s3h.relative_humidity) /3 as humidity_trend,
       (s.temperature - s3h.temperature) / 3 as temperature_trend,
       (s.dew_point - s3h.dew_point) / 3 as dew_point_trend,
       (s.wind_chill - s3h.wind_chill) / 3 as wind_chill_trend,
       (s.apparent_temperature - s3h.apparent_temperature) / 3 as apparent_temperature_trend,
       (s.absolute_pressure - s3h.absolute_pressure) / 3 as absolute_pressure_trend
from sample s
left outer join sample s3h on s3h.station_id = s.station_id and s3h.time_stamp = s.time_stamp - '3 hours'::interval
where s.station_id = $station
order by s.time_stamp desc
limit 1
        """

    result = db.query(query, dict(station=station_id))

    if len(result):
        return result[0]
    return None


def get_10m_avg_bearing_max_gust(station_id):
    query = """
        select avg(s.wind_direction) as avg_bearing,
       max(s.gust_wind_speed) as max_gust
from sample s
where s.station_id = $station
  and s.time_stamp > (select max(time_stamp) from sample where station_id = $station) - '10 minutes'::interval
limit 10"""

    return db.query(query, dict(station=station_id))[0]


def get_day_wind_run(date, station_id):
    query = """
        select s.time_stamp::date, sum(300*average_wind_speed) as wind_run
from sample s
where s.station_id = $station
  and s.time_stamp::date = $date
group by s.time_stamp::date
    """

    result = db.query(query, dict(station=station_id, date=date))

    if result is None or len(result) == 0:
        return 0

    return result[0].wind_run


def get_cumulus_dayfile_data(station_id):
    """
    This returns the data required for the Cumulus dayfile.txt data file. Its
     a fairly expensive query to run as it effectively returns aggregate data
     for the entire weather stations history. The results are effectively the
     daily records plus additional records for davis stations plus a few other
     similar details (average temperature, total wind run, total
     evapotranspiration, etc). It excludes data for the current day.
    """
    query = """
with full_davis_sample as (
select s.time_stamp,
       s.station_id,
       ds.gust_wind_direction,
       ds.high_rain_rate,
       ds.evapotranspiration,
       ds.solar_radiation,
       ds.average_uv_index
from sample s
inner join davis_sample ds on ds.sample_id = s.sample_id
)
select dr.date_stamp,
       dr.station_id,
       dr.max_gust_wind_speed,
       coalesce(ds_hgb.gust_wind_direction, 0) as max_gust_wind_speed_direction,
       dr.max_gust_wind_speed_ts,
       dr.min_temperature,
       dr.min_temperature_ts,
       dr.max_temperature,
       dr.max_temperature_ts,
       dr.min_absolute_pressure,
       dr.min_absolute_pressure_ts,
       dr.max_absolute_pressure,
       dr.max_absolute_pressure_ts,
       coalesce(ds_rrr.max_rain_rate, 0) as max_rain_rate,
       ds_rrr.max_rain_rate_ts,
       dr.total_rainfall,
       day_avg.average_temperature,
       day_wind_run.wind_run,
       dr.max_average_wind_speed,
       dr.max_average_wind_speed_ts,
       dr.min_humidity,
       dr.min_humidity_ts,
       dr.max_humidity,
       dr.max_humidity_ts,
       coalesce(day_et.total, 0) as evapotranspiration,
       -- total hours of sunshine
       -- high heat index
       -- time of high heat index
       dr.max_apparent_temperature,
       dr.max_apparent_temperature_ts,
       dr.min_apparent_temperature,
       dr.min_apparent_temperature_ts,
       coalesce(dhrr.max_hour_rain, 0) as max_hour_rain,
       dhrr.max_hour_rain_ts,
       dr.min_wind_chill,
       dr.min_wind_chill_ts,
       dr.max_dew_point,
       dr.max_dew_point_ts,
       dr.min_dew_point,
       dr.min_dew_point_ts,
       day_avg.average_wind_direction,
       coalesce(ds_uv.max_uv_index, 0) as max_uv_index,
       ds_uv.max_uv_index_ts,
       coalesce(ds_sr.max_solar_radiation, 0) as max_solar_radiation,
       ds_sr.max_solar_radiation_ts
from daily_records dr
left outer join full_davis_sample ds_hgb on ds_hgb.station_id = dr.station_id and ds_hgb.time_stamp = dr.max_gust_wind_speed_ts
left outer join (
   select ds.time_stamp::date as date_stamp,
       ds.station_id,
       ds.high_rain_rate as max_rain_rate,
       max(ds.time_stamp) as max_rain_rate_ts
    from full_davis_sample ds
    inner join (select time_stamp::date as date_stamp, station_id, max(high_rain_rate) as max_rain_rate
    from full_davis_sample
    group by time_stamp::date, station_id) as mx on mx.date_stamp = ds.time_stamp::date and mx.station_id = ds.station_id and mx.max_rain_rate = ds.high_rain_rate
    group by ds.high_rain_rate, ds.station_id, ds.time_stamp::date
    order by ds.time_stamp::date, ds.station_id
) as ds_rrr on ds_rrr.station_id = dr.station_id and ds_rrr.date_stamp = dr.date_stamp
left outer join (
   select ds.time_stamp::date as date_stamp,
       ds.station_id,
       ds.average_uv_index as max_uv_index,
       max(ds.time_stamp) as max_uv_index_ts
    from full_davis_sample ds
    inner join (
        select time_stamp::date as date_stamp,
               station_id,
               max(average_uv_index) as max_uv_index
        from full_davis_sample
        group by time_stamp::date, station_id
    ) as mx on mx.date_stamp = ds.time_stamp::date and mx.station_id = ds.station_id and mx.max_uv_index = ds.average_uv_index
    group by ds.average_uv_index, ds.station_id, ds.time_stamp::date
    order by ds.time_stamp::date, ds.station_id
) as ds_uv on ds_uv.station_id = dr.station_id and ds_uv.date_stamp = dr.date_stamp
left outer join (
 select ds.time_stamp::date as date_stamp,
       ds.station_id,
       ds.solar_radiation as max_solar_radiation,
       max(ds.time_stamp) as max_solar_radiation_ts
    from full_davis_sample ds
    inner join (
        select time_stamp::date as date_stamp,
               station_id,
               max(solar_radiation) as max_solar_radiation
        from full_davis_sample
        group by time_stamp::date, station_id
    ) as mx on mx.date_stamp = ds.time_stamp::date and mx.station_id = ds.station_id and mx.max_solar_radiation = ds.solar_radiation
    group by ds.solar_radiation, ds.station_id, ds.time_stamp::date
    order by ds.time_stamp::date, ds.station_id
) as ds_sr on ds_sr.station_id = dr.station_id and ds_sr.date_stamp = dr.date_stamp
inner join (
    select s.time_stamp::date as date_stamp,
           s.station_id,
           avg(s.temperature) as average_temperature,
           avg(s.wind_direction) as average_wind_direction
      from sample s
      group by s.time_stamp::date, s.station_id
) as day_avg on day_avg.date_stamp = dr.date_stamp and day_avg.station_id = dr.station_id
inner join (
    select s.time_stamp::date as date_stamp,
           s.station_id,
           sum(300*average_wind_speed) as wind_run
    from sample s
    group by s.time_stamp::date, s.station_id
) as day_wind_run on day_wind_run.date_stamp = dr.date_stamp and day_wind_run.station_id = dr.station_id
left outer join (
  select time_stamp::date as date_stamp,
         station_id,
         sum(evapotranspiration) as total
  from full_davis_sample
  group by time_stamp::date, station_id
) as day_et on day_et.date_stamp = dr.date_stamp and day_et.station_id = dr.station_id
left outer join (
  -- So I can't find any description of what 'Hourly Rain' actually is in Cumulus.
  -- I'm just assuming its the total rainfall for each hour. And as such this will
  -- give the hour that had the most:
    with hourly_rain as (
        select time_stamp::date as date_stamp,
               date_trunc('hour', time_stamp) as day_hour,
               station_id,
               sum(rainfall) as total
        from sample
        group by time_stamp::date, date_trunc('hour', time_stamp), station_id
    )
    select hr.date_stamp,
           hr.station_id,
           hr.total as max_hour_rain,
           max(hr.day_hour) as max_hour_rain_ts
    from hourly_rain hr
    inner join(
        select date_stamp,
               station_id,
               max(total) as max_total
        from hourly_rain
        group by date_stamp, station_id
    ) as mhr on mhr.date_stamp = hr.date_stamp and mhr.station_id = hr.station_id and hr.total = mhr.max_total
    where total > 0
    group by hr.date_stamp, hr.station_id, hr.total
) dhrr on dhrr.date_stamp = dr.date_stamp and dhrr.station_id = dr.station_id
where dr.station_id = $station
  -- Cumulus adds a new entry to this file at midnight. So we'll exclude the
  -- entry for the current date.
  and dr.date_stamp <> NOW()::Date
order by dr.date_stamp
    """

    params = dict(station=station_id)

    result = db.query(query, params)

    return result


# Query used by weather_plot for the month data set. Copied here as the desktop
# client also uses this dataset for over-the-internet operation. This version
# has gap detection removed as the desktop client figures this out itself
def get_month_data_wp(year, month, station_id):
    query = """
select cur.time_stamp,
       round(cur.temperature::numeric, 2) as temperature,
       round(cur.dew_point::numeric, 1) as dew_point,
       round(cur.apparent_temperature::numeric, 1) as apparent_temperature,
       round(cur.wind_chill::numeric,1) as wind_chill,
       cur.relative_humidity,
       round(cur.absolute_pressure::numeric,2) as absolute_pressure,
       round(cur.indoor_temperature::numeric,2) as indoor_temperature,
       cur.indoor_relative_humidity,
       round(cur.rainfall::numeric, 1) as rainfall,
       round(cur.average_wind_speed::numeric,2) as average_wind_speed,
       round(cur.gust_wind_speed::numeric,2) as gust_wind_speed,
       cur.wind_direction,
       cur.time_stamp - (s.sample_interval * '1 second'::interval) as prev_sample_time,
       ds.average_uv_index as uv_index,
       ds.solar_radiation,
       
       case when $broadcast_id is null then null
       --                                  |----- Max wireless packets calculation -----------------------|
       else round((ds.wind_sample_count / ((s.sample_interval::float) /((41+$broadcast_id-1)::float /16.0 )) * 100)::numeric,1)::float 
       end as reception,
       round(ds.high_temperature::numeric, 2) as high_temperature,
       round(ds.low_temperature::numeric, 2) as low_temperature,
       round(ds.high_rain_rate::numeric, 2) as high_rain_rate,
       ds.gust_wind_direction,
       ds.evapotranspiration,
       ds.high_solar_radiation,
       ds.high_uv_index,
       ds.forecast_rule_id,
       
       -- Extra sensors
       ds.soil_moisture_1,
       ds.soil_moisture_2,
       ds.soil_moisture_3,
       ds.soil_moisture_4,
       round(ds.soil_temperature_1::numeric, 2) as soil_temperature_1,
       round(ds.soil_temperature_2::numeric, 2) as soil_temperature_2,
       round(ds.soil_temperature_3::numeric, 2) as soil_temperature_3,
       round(ds.soil_temperature_4::numeric, 2) as soil_temperature_4,
       ds.leaf_wetness_1,
       ds.leaf_wetness_2,
       round(ds.leaf_temperature_1::numeric, 2) as leaf_temperature_1,
       round(ds.leaf_temperature_2::numeric, 2) as leaf_temperature_2,
       ds.extra_humidity_1,
       ds.extra_humidity_2,
       round(ds.extra_temperature_1::numeric, 2) as extra_temperature_1,
       round(ds.extra_temperature_2::numeric, 2) as extra_temperature_2,
       round(ds.extra_temperature_3::numeric, 2) as extra_temperature_3
from sample cur
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(date_trunc('month',cur.time_stamp)) = $date
  and cur.station_id = $station
order by cur.time_stamp asc
        """

    # TODO: Integrate this get_station_config call into the query when our
    # minimum PostgreSQL version supports JSON
    station_config = get_station_config(station_id)
    broadcast_id = None
    if station_config is not None and 'broadcast_id' in station_config:
        broadcast_id = station_config['broadcast_id']

    params = dict(station=station_id, date=date(year=year,month=month,day=1),
                  broadcast_id=broadcast_id)

    return db.query(query, params)


def get_month_data_wp_age(year, month, station_id):
    query = """
select max(cur.time_stamp) as max_ts
from sample cur
where date(date_trunc('month',cur.time_stamp)) = $date
  and cur.station_id = $station
        """

    params = dict(station=station_id, date=date(year=year,month=month,day=1))

    result = db.query(query, params)
    if result is None or len(result) == 0:
        return None

    return result[0].max_ts

# Query used by weather_plot for the day data set. Copied here as the desktop
# client also uses this dataset for over-the-internet operation. This version
# excludes gap detection as the desktop client figures that out on its own.
def get_day_data_wp(date, station_id):
    query = """select cur.time_stamp::time as time,
       cur.time_stamp,
       round(cur.temperature::numeric,2) as temperature,
       round(cur.dew_point::numeric, 1) as dew_point,
       round(cur.apparent_temperature::numeric, 1) as apparent_temperature,
       round(cur.wind_chill::numeric,1) as wind_chill,
       cur.relative_humidity,
       round(cur.absolute_pressure::numeric,2) as absolute_pressure,
       round(cur.indoor_temperature::numeric,2) as indoor_temperature,
       cur.indoor_relative_humidity,
       round(cur.rainfall::numeric, 1) as rainfall,
       round(cur.average_wind_speed::numeric,2) as average_wind_speed,
       round(cur.gust_wind_speed::numeric,2) as gust_wind_speed,
       cur.wind_direction,
       cur.time_stamp - (s.sample_interval * '1 second'::interval) as prev_sample_time,
       ds.average_uv_index as uv_index,
       ds.solar_radiation,
       
       case when $broadcast_id is null then null
       --                                  |----- Max wireless packets calculation -----------------------|
       else round((ds.wind_sample_count / ((s.sample_interval::float) /((41+$broadcast_id-1)::float /16.0 )) * 100)::numeric,1)::float 
       end as reception,
       round(ds.high_temperature::numeric, 2) as high_temperature,
       round(ds.low_temperature::numeric, 2) as low_temperature,
       round(ds.high_rain_rate::numeric, 2) as high_rain_rate,
       ds.gust_wind_direction,
       ds.evapotranspiration,
       ds.high_solar_radiation,
       ds.high_uv_index,
       ds.forecast_rule_id,
       
       -- Extra sensors
       ds.soil_moisture_1,
       ds.soil_moisture_2,
       ds.soil_moisture_3,
       ds.soil_moisture_4,
       round(ds.soil_temperature_1::numeric, 2) as soil_temperature_1,
       round(ds.soil_temperature_2::numeric, 2) as soil_temperature_2,
       round(ds.soil_temperature_3::numeric, 2) as soil_temperature_3,
       round(ds.soil_temperature_4::numeric, 2) as soil_temperature_4,
       ds.leaf_wetness_1,
       ds.leaf_wetness_2,
       round(ds.leaf_temperature_1::numeric, 2) as leaf_temperature_1,
       round(ds.leaf_temperature_2::numeric, 2) as leaf_temperature_2,
       ds.extra_humidity_1,
       ds.extra_humidity_2,
       round(ds.extra_temperature_1::numeric, 2) as extra_temperature_1,
       round(ds.extra_temperature_2::numeric, 2) as extra_temperature_2,
       round(ds.extra_temperature_3::numeric, 2) as extra_temperature_3
from sample cur
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(cur.time_stamp) = $date
  and cur.station_id = $station
order by cur.time_stamp asc"""

    # TODO: Integrate this get_station_config call into the query when our
    # minimum PostgreSQL version supports JSON
    broadcast_id = get_station_config(station_id)['broadcast_id']

    params = dict(station=station_id, date=date, broadcast_id=broadcast_id)

    return db.query(query, params)


def get_noaa_year_data(station_code, year, include_criteria=True):

    if station_code not in config.report_settings or \
            "noaa_year" not in config.report_settings[station_code]:
        return None

    params = config.report_settings[station_code]["noaa_year"]

    stations = get_full_station_info(not config.hide_coordinates)

    station_info = None
    for x in stations:
        if x['code'].lower() == station_code.lower():
            station_info = x
            break

    if station_info is None:
        return None

    params.update({
        "start": date(year=year, month=1, day=1),
        "end": date(year=year, month=12, day=31),
        "atDate": date(year=year, month=1, day=1),
        "title": station_info['name'],
        "altitude": station_info['coordinates']['altitude'],
        "latitude": station_info['coordinates']['latitude'],
        "longitude": station_info['coordinates']['longitude'],
    })

    # Changes from desktop version are limited to query parameter form
    # ($parameter instead of :parameter)
    yearly_query = """
WITH parameters AS (
   SELECT
     $heatBase::float    	            AS heat_base,
     $coolBase::float    	            AS cool_base,
     $start::date                       AS start_date,
     $end::date                         AS end_date,
   	 $fahrenheit::boolean               AS fahrenheit, -- instead of celsius
   	 $kmh::boolean                      AS kmh,	-- instead of m/s
   	 $mph::boolean                      AS mph,	-- instead of m/s
   	 $inches::boolean                   AS inches, -- instead of mm
     $stationCode::varchar(5)           AS stationCode,
     32::integer                        AS max_high_temp,
     0::integer                         AS max_low_temp,
     0::integer                         AS min_high_temp,
     -18::integer                       AS min_low_temp,
     0.2::float                         AS rain_02,
     2.0::float                         AS rain_2,
     20.0::float                        AS rain_20
 ), compass_points AS (
  SELECT
    column1 AS idx,
    column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
), normal_readings as (
  select
    column1 as month,
    column2 as temp,
    column3 as rain
  from (values
      (1, $normJan::FLOAT, $normJanRain::FLOAT),
      (2, $normFeb::FLOAT, $normFebRain::FLOAT),
      (3, $normMar::FLOAT, $normMarRain::FLOAT),
      (4, $normApr::FLOAT, $normAprRain::FLOAT),
      (5, $normMay::FLOAT, $normMayRain::FLOAT),
      (6, $normJun::FLOAT, $normJunRain::FLOAT),
      (7, $normJul::FLOAT, $normJulRain::FLOAT),
      (8, $normAug::FLOAT, $normAugRain::FLOAT),
      (9, $normSep::FLOAT, $normSepRain::FLOAT),
      (10, $normOct::FLOAT, $normOctRain::FLOAT),
      (11, $normNov::FLOAT, $normNovRain::FLOAT),
      (12, $normDec::FLOAT, $normDecRain::FLOAT)
    ) as norm
), daily_aggregates as (
  select
      -- Day info
      aggregates.date                                         AS date,
      aggregates.station_id                                   AS station_id,
      -- Day Counts
      case when aggregates.tot_rain >= parameters.rain_02
        then 1 else 0 end                                     AS rain_over_02,
      case when aggregates.tot_rain >= parameters.rain_2
        then 1 else 0 end                                     AS rain_over_2,
      case when aggregates.tot_rain >= parameters.rain_20
        then 1 else 0 end                                     AS rain_over_20,
      case when aggregates.max_temp >= parameters.max_high_temp
        then 1 else 0 end                                     AS max_high_temp,
      case when aggregates.max_temp <= parameters.max_low_temp
        then 1 else 0 end                                     AS max_low_temp,
      case when aggregates.min_temp <= parameters.min_high_temp
        then 1 else 0 end                                     AS min_high_temp,
      case when aggregates.min_temp <= parameters.min_low_temp
        then 1 else 0 end                                     AS min_low_temp,
      -- day aggregates
      aggregates.max_temp                                     AS max_temp,
      aggregates.min_temp                                     AS min_temp,
      aggregates.avg_temp                                     AS avg_temp,
      aggregates.tot_rain                                     AS tot_rain,
      aggregates.avg_wind                                     AS avg_wind,
      aggregates.max_wind                                     AS max_wind,
      aggregates.tot_cool_degree_days                         AS tot_cool_degree_days,
      aggregates.tot_heat_degree_days                         AS tot_heat_degree_days
    from (
      select   -- Various aggregates per day
        s.time_stamp::date                          AS date,
        s.station_id                                AS station_id,
        round(max(temperature)::numeric,1)          AS max_temp,
        round(min(temperature)::numeric,1)          AS min_temp,
        round(avg(temperature)::numeric,1)          AS avg_temp,
        round(sum(rainfall)::numeric,1)             AS tot_rain,
        round(avg(average_wind_speed)::numeric,1)   AS avg_wind,
        round(max(gust_wind_speed)::numeric,1)      AS max_wind,
        sum(cool_degree_days)                       AS tot_cool_degree_days,
        sum(heat_degree_days)                       AS tot_heat_degree_days
      from (
        select
          time_stamp,
          s.station_id,
          temperature,
          rainfall,
          average_wind_speed,
          gust_wind_speed,
          case when temperature > p.cool_base
            then temperature - p.cool_base
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as heat_degree_days
        from parameters p, sample s
        inner join station stn on stn.station_id = s.station_id
        where stn.code = p.stationCode
      ) s, parameters
      where s.time_stamp::date between parameters.start_date::date and parameters.end_date::date
      group by s.time_stamp::date, s.station_id
    ) as aggregates, parameters
  group by
    aggregates.date, aggregates.tot_rain, aggregates.max_temp, aggregates.min_temp, aggregates.station_id,
    aggregates.avg_temp, aggregates.avg_wind, aggregates.max_wind, aggregates.tot_cool_degree_days,
    aggregates.tot_heat_degree_days, parameters.rain_02, parameters.rain_2, parameters.rain_20,
    parameters.max_high_temp, parameters.max_low_temp, parameters.min_high_temp, parameters.min_low_temp
),
monthly_aggregates as (
  select
    date_trunc('month', agg.date)::date as date,
    agg.station_id as station_id,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.max_temp)                   as max_avg_temp,
    avg(agg.min_temp)                   as min_avg_temp,
    avg(agg.avg_temp)                   as avg_temp,
    avg(agg.avg_temp) - norm.temp       as dep_norm_temp,
    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,
    sum(agg.tot_rain) - norm.rain       as dep_norm_rain,
    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from daily_aggregates as agg
  inner join normal_readings norm on norm.month = extract(month from agg.date)
  group by date_trunc('month', agg.date)::date, agg.station_id, norm.temp, norm.rain
), yearly_aggregates as (
  select
    agg.station_id                      as station_id,
    date_trunc('year', agg.date)::date  as year,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.max_avg_temp)               as max_avg_temp,
    avg(agg.min_avg_temp)               as min_avg_temp,
    avg(agg.avg_temp)                   as avg_temp,
    avg(agg.dep_norm_temp)              as dep_norm_temp,
    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,
    sum(agg.dep_norm_rain)              as dep_norm_rain,
    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from monthly_aggregates as agg
  group by date_trunc('year', agg.date)::date, agg.station_id
),dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      date_trunc('year', time_stamp)::date as year,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where date_trunc('year', s.time_stamp)::date between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY date_trunc('year', s.time_stamp) :: DATE, station_id, wind_direction
  ), max_count AS (
    SELECT
      station_id,
      year,
      max(count) AS max_count
    FROM wind_directions AS counts
    GROUP BY station_id, year
  )
  SELECT
    d.station_id,
    d.year,
    min(wind_direction) AS wind_direction
  FROM wind_directions d
    INNER JOIN max_count mc ON mc.station_id = d.station_id AND mc.year = d.year AND mc.max_count = d.count
  GROUP BY d.station_id, d.year
)
select
  d.year                                                    as year,

  -- Temperature
  lpad(round(d.mean_max::numeric, 1)::varchar, 5, ' ')      as mean_max,
  lpad(round(d.mean_min::numeric, 1)::varchar, 5, ' ')      as mean_min,
  lpad(round(d.mean::numeric, 1)::varchar, 5, ' ')          as mean,
  lpad(round(d.dep_norm_temp::numeric, 1)::varchar, 5, ' ') as dep_norm_temp,
  lpad(round(d.heat_deg_days::numeric, 0)::varchar, 5, ' ') as heat_dd,
  lpad(round(d.cool_deg_days::numeric, 0)::varchar, 5, ' ') as cool_dd,
  lpad(round(d.hi_temp::numeric, 1)::varchar, 5, ' ')       as hi_temp,
  lpad(d.hi_temp_date::varchar, 3, ' ')                     as hi_temp_date,
  lpad(round(d.low::numeric, 1)::varchar, 5, ' ')           as low_temp,
  lpad(d.low_date::varchar, 3, ' ')                         as low_temp_date,
  lpad(d.max_high::varchar, 3, ' ')                         as max_high,
  lpad(d.max_low::varchar, 3, ' ')                          as max_low,
  lpad(d.min_high::varchar, 3, ' ')                         as min_high,
  lpad(d.min_low::varchar, 3, ' ')                          as min_low,

  -- Rain
  lpad(case when p.inches
    then round(d.tot_rain::numeric, 2)
    else round(d.tot_rain::numeric, 1)
    end::varchar, 7, ' ')                                   as tot_rain,
  lpad(case when p.inches
    then round(d.dep_norm_rain::numeric, 2)
    else round(d.dep_norm_rain::numeric, 1)
    end::varchar, 6, ' ')                                   as dep_norm_rain,
  lpad(case when p.inches
    then round(d.max_obs_rain::numeric, 2)
    else round(d.max_obs_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as max_obs_rain,
  lpad(d.max_obs_rain_day::varchar, 3, ' ')                 as max_obs_rain_day,
  lpad(d.rain_02::varchar, 3, ' ')                          as rain_02,
  lpad(d.rain_2::varchar, 3, ' ')                           as rain_2,
  lpad(d.rain_20::varchar, 3, ' ')                          as rain_20,

  -- Wind
  lpad(round(d.avg_wind::numeric, 1)::varchar, 4, ' ')      as avg_wind,
  lpad(round(d.max_wind::numeric, 1)::varchar, 4, ' ')      as hi_wind,
  lpad(d.high_wind_day::varchar, 3, ' ')                    as high_wind_day,
  lpad(d.dom_dir::varchar, 3, ' ')                          as dom_dir

from (  -- This is unit-converted report data, d:
  select
    -- Common Columns
    to_char(data.year, 'YY')                                                              as year,

    -- Temperature Table
    case when p.fahrenheit then data.max_avg_temp  * 1.8 + 32 else data.max_avg_temp  end as mean_max,
    case when p.fahrenheit then data.min_avg_temp  * 1.8 + 32 else data.min_avg_temp  end as mean_min,
    case when p.fahrenheit then data.avg_temp      * 1.8 + 32 else data.avg_temp      end as mean,
    case when p.fahrenheit then data.dep_temp_norm * 1.8 + 32 else data.dep_temp_norm end as dep_norm_temp,
    case when p.fahrenheit
      then data.tot_heat_degree_days * 1.8 + 32 else data.tot_heat_degree_days        end as heat_deg_days,
    case when p.fahrenheit
      then data.tot_cool_degree_days * 1.8 + 32 else data.tot_cool_degree_days        end as cool_deg_days,
    case when p.fahrenheit then data.max_temp      * 1.8 + 32 else data.max_temp      end as hi_temp,
    to_char(data.max_temp_date, 'MON')                                                    as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    to_char(data.min_temp_date, 'MON')                                                    as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.dep_rain_norm * 1.0/25.4 else data.dep_rain_norm     end as dep_norm_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    to_char(data.max_rain_date, 'MON')                                                    as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round((data.avg_wind * 3.6)::numeric,1)
         when p.mph then round((data.avg_wind * 2.23694)::numeric,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round((data.max_wind * 3.6)::numeric, 1)
         when p.mph then round((data.max_wind * 2.23694)::numeric, 1)
         else            data.max_wind                                                end as max_wind,
    to_char(data.max_wind_date, 'MON')                                                    as high_wind_day,
    data.dom_wind_direction                                                               as dom_dir

  from (
    select
      match.year,
      match.station_id,

      -- Rain counts
      match.rain_over_02,
      match.rain_over_2,
      match.rain_over_20,

      -- Temperature counts
      match.max_high_temp,
      match.max_low_temp,
      match.min_high_temp,
      match.min_low_temp,

      -- Monthly aggregates - temperature
      match.max_avg_temp,
      match.min_avg_temp,
      match.avg_temp,
      match.dep_temp_norm,
      match.tot_cool_degree_days,
      match.tot_heat_degree_days,
      match.max_temp,
      match.min_temp,
      max(match.max_temp_date) as max_temp_date,
      max(match.min_temp_date) as min_temp_date,

      -- Monthly aggregates - rain
      match.tot_rain,
      match.dep_rain_norm,
      match.max_rain,
      max(match.max_rain_date) as max_rain_date,

      -- Monthly aggregates - wind
      match.avg_wind,
      match.max_wind,
      max(match.max_wind_date) as max_wind_date,
      match.dom_wind_direction
    from (
      select
        yearly.year                    as year,
        yearly.station_id              as station_id,

        -- Rain counts
        yearly.rain_over_02            as rain_over_02,
        yearly.rain_over_2             as rain_over_2,
        yearly.rain_over_20            as rain_over_20,

        -- Temperature counts
        yearly.max_high_temp           as max_high_temp,
        yearly.max_low_temp            as max_low_temp,
        yearly.min_high_temp           as min_high_temp,
        yearly.min_low_temp            as min_low_temp,

        -- Monthly aggregates - temperature
        yearly.max_avg_temp            as max_avg_temp,
        yearly.min_avg_temp            as min_avg_temp,
        yearly.avg_temp                as avg_temp,
        yearly.dep_norm_temp           as dep_temp_norm,
        yearly.tot_cool_degree_days    as tot_cool_degree_days,
        yearly.tot_heat_degree_days    as tot_heat_degree_days,
        yearly.max_temp                as max_temp,
        case when yearly.max_temp = monthly.max_temp
          then monthly.date else null end as max_temp_date,
        yearly.min_temp                as min_temp,
        case when yearly.min_temp = monthly.min_temp
          then monthly.date else null end as min_temp_date,

        -- Monthly aggregates - rainfall
        yearly.tot_rain                as tot_rain,
        yearly.dep_norm_rain           as dep_rain_norm,
        yearly.max_rain                as max_rain,
        case when yearly.max_rain = monthly.tot_rain
          then monthly.date else null end as max_rain_date,

        -- Monthly aggregates - wind
        yearly.avg_wind                as avg_wind,
        yearly.max_wind                as max_wind,
        case when yearly.max_wind = monthly.max_wind
          then monthly.date else null end as max_wind_date,
        point.point                     as dom_wind_direction

      from yearly_aggregates yearly
      inner join monthly_aggregates monthly ON
          monthly.station_id = yearly.station_id
          and (yearly.max_temp = monthly.max_temp
               or yearly.min_temp = monthly.min_temp
               or yearly.max_rain = monthly.tot_rain
               or yearly.max_wind = monthly.max_wind)
      left outer join dominant_wind_directions dwd on dwd.station_id = yearly.station_id and dwd.year = yearly.year
      LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
    ) as match
    group by
      match.year, match.station_id, match.rain_over_02, match.rain_over_2, match.rain_over_20,
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp, match.max_avg_temp,
      match.min_avg_temp, match.avg_temp, match.dep_temp_norm, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.dep_rain_norm, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.year, match.station_id
  ) as data, parameters p
) as d
cross join parameters p;
    """

    monthly_query = """
WITH parameters AS (
   SELECT
     $heatBase::float    	            AS heat_base,
     $coolBase::float                   AS cool_base,
     $start::date                       AS start_date,
     $end::date                         AS end_date,
   	 $fahrenheit::boolean               AS fahrenheit, -- instead of celsius
   	 $kmh::boolean                      AS kmh,	-- instead of m/s
   	 $mph::boolean                      AS mph,	-- instead of m/s
   	 $inches::boolean                   AS inches, -- instead of mm
     $stationCode::varchar(5)           AS stationCode,
     32::integer                        AS max_high_temp,
     0::integer                         AS max_low_temp,
     0::integer                         AS min_high_temp,
     -18::integer                       AS min_low_temp,
     0.2::float                         AS rain_02,
     2.0::float                         AS rain_2,
     20.0::float                        AS rain_20
 ), compass_points AS (
  SELECT
    column1 AS idx,
    column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
), normal_readings as (
  select
    column1 as month,
    column2 as temp,
    column3 as rain
  from (values
      (1, $normJan::FLOAT, $normJanRain::FLOAT),
      (2, $normFeb::FLOAT, $normFebRain::FLOAT),
      (3, $normMar::FLOAT, $normMarRain::FLOAT),
      (4, $normApr::FLOAT, $normAprRain::FLOAT),
      (5, $normMay::FLOAT, $normMayRain::FLOAT),
      (6, $normJun::FLOAT, $normJunRain::FLOAT),
      (7, $normJul::FLOAT, $normJulRain::FLOAT),
      (8, $normAug::FLOAT, $normAugRain::FLOAT),
      (9, $normSep::FLOAT, $normSepRain::FLOAT),
      (10, $normOct::FLOAT, $normOctRain::FLOAT),
      (11, $normNov::FLOAT, $normNovRain::FLOAT),
      (12, $normDec::FLOAT, $normDecRain::FLOAT)
    ) as norm
), daily_aggregates as (
  select
      -- Day info
      aggregates.date                                         AS date,
      aggregates.station_id                                   AS station_id,
      -- Day Counts
      case when aggregates.tot_rain >= parameters.rain_02
        then 1 else 0 end                                     AS rain_over_02,
      case when aggregates.tot_rain >= parameters.rain_2
        then 1 else 0 end                                     AS rain_over_2,
      case when aggregates.tot_rain >= parameters.rain_20
        then 1 else 0 end                                     AS rain_over_20,
      case when aggregates.max_temp >= parameters.max_high_temp
        then 1 else 0 end                                     AS max_high_temp,
      case when aggregates.max_temp <= parameters.max_low_temp
        then 1 else 0 end                                     AS max_low_temp,
      case when aggregates.min_temp <= parameters.min_high_temp
        then 1 else 0 end                                     AS min_high_temp,
      case when aggregates.min_temp <= parameters.min_low_temp
        then 1 else 0 end                                     AS min_low_temp,
      -- day aggregates
      aggregates.max_temp                                     AS max_temp,
      aggregates.min_temp                                     AS min_temp,
      aggregates.avg_temp                                     AS avg_temp,
      aggregates.tot_rain                                     AS tot_rain,
      aggregates.avg_wind                                     AS avg_wind,
      aggregates.max_wind                                     AS max_wind,
      aggregates.tot_cool_degree_days                         AS tot_cool_degree_days,
      aggregates.tot_heat_degree_days                         AS tot_heat_degree_days
    from (
      select   -- Various aggregates per day
        s.time_stamp::date                          AS date,
        s.station_id                                AS station_id,
        round(max(temperature)::numeric,1)          AS max_temp,
        round(min(temperature)::numeric,1)          AS min_temp,
        round(avg(temperature)::numeric,1)          AS avg_temp,
        round(sum(rainfall)::numeric,1)             AS tot_rain,
        round(avg(average_wind_speed)::numeric,1)   AS avg_wind,
        round(max(gust_wind_speed)::numeric,1)      AS max_wind,
        sum(cool_degree_days)                       AS tot_cool_degree_days,
        sum(heat_degree_days)                       AS tot_heat_degree_days
      from (
        select
          time_stamp,
          s.station_id,
          temperature,
          rainfall,
          average_wind_speed,
          gust_wind_speed,
          case when temperature > p.cool_base
            then temperature - p.cool_base
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as heat_degree_days
        from parameters p, sample s
        inner join station stn on stn.station_id = s.station_id
        where stn.code = p.stationCode
      ) s, parameters
      where s.time_stamp::date between parameters.start_date::date and parameters.end_date::date
      group by s.time_stamp::date, s.station_id
    ) as aggregates, parameters
  group by
    aggregates.date, aggregates.tot_rain, aggregates.max_temp, aggregates.min_temp, aggregates.station_id,
    aggregates.avg_temp, aggregates.avg_wind, aggregates.max_wind, aggregates.tot_cool_degree_days,
    aggregates.tot_heat_degree_days, parameters.rain_02, parameters.rain_2, parameters.rain_20,
    parameters.max_high_temp, parameters.max_low_temp, parameters.min_high_temp, parameters.min_low_temp
),
monthly_aggregates as (
  select
    date_trunc('month', agg.date)::date as date,
    agg.station_id as station_id,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.max_temp)                   as max_avg_temp,
    avg(agg.min_temp)                   as min_avg_temp,
    avg(agg.avg_temp)                   as avg_temp,

    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,

    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from daily_aggregates as agg
  group by date_trunc('month', agg.date)::date, agg.station_id
), dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      date_trunc('month', time_stamp)::date as month,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where date_trunc('month', s.time_stamp)::date between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY date_trunc('month', s.time_stamp) :: DATE, station_id, wind_direction
  ), max_count AS (
    SELECT
      station_id,
      month,
      max(count) AS max_count
    FROM wind_directions AS counts
    GROUP BY station_id, month
  )
  SELECT
    d.station_id,
    d.month,
    min(wind_direction) AS wind_direction
  FROM wind_directions d
    INNER JOIN max_count mc ON mc.station_id = d.station_id AND mc.month = d.month AND mc.max_count = d.count
  GROUP BY d.station_id, d.month
), range as (
  SELECT
    to_char(start_date, 'YY') as year,
    generate_series(1, 12, 1) as month
  from parameters
)
select
  r.year                                                    as year,
  lpad(r.month::varchar, 2, ' ')                            as month,

  -- Temperature
  lpad(round(d.mean_max::numeric, 1)::varchar, 5, ' ')      as mean_max,
  lpad(round(d.mean_min::numeric, 1)::varchar, 5, ' ')      as mean_min,
  lpad(round(d.mean::numeric, 1)::varchar, 5, ' ')          as mean,
  lpad(round(d.dep_norm_temp::numeric, 1)::varchar, 5, ' ') as dep_norm_temp,
  lpad(round(d.heat_deg_days::numeric, 0)::varchar, 5, ' ') as heat_dd,
  lpad(round(d.cool_deg_days::numeric, 0)::varchar, 5, ' ') as cool_dd,
  lpad(round(d.hi_temp::numeric, 1)::varchar, 5, ' ')       as hi_temp,
  lpad(d.hi_temp_date::varchar, 2, ' ')                     as hi_temp_date,
  lpad(round(d.low::numeric, 1)::varchar, 5, ' ')           as low_temp,
  lpad(d.low_date::varchar, 2, ' ')                         as low_temp_date,
  lpad(d.max_high::varchar, 2, ' ')                         as max_high,
  lpad(d.max_low::varchar, 2, ' ')                          as max_low,
  lpad(d.min_high::varchar, 2, ' ')                         as min_high,
  lpad(d.min_low::varchar, 2, ' ')                          as min_low,

  -- Rain
  lpad(case when p.inches
    then round(d.tot_rain::numeric, 2)
    else round(d.tot_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as tot_rain,
  lpad(case when p.inches
    then round(d.dep_norm_rain::numeric, 2)
    else round(d.dep_norm_rain::numeric, 1)
    end::varchar, 6, ' ')                                   as dep_norm_rain,
  lpad(case when p.inches
    then round(d.max_obs_rain::numeric, 2)
    else round(d.max_obs_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as max_obs_rain,
  lpad(d.max_obs_rain_day::varchar, 3, ' ')                 as max_obs_rain_day,
  lpad(d.rain_02::varchar, 3, ' ')                          as rain_02,
  lpad(d.rain_2::varchar, 3, ' ')                           as rain_2,
  lpad(d.rain_20::varchar, 3, ' ')                          as rain_20,

  -- Wind
  lpad(round(d.avg_wind::numeric, 1)::varchar, 4, ' ')      as avg_wind,
  lpad(round(d.max_wind::numeric, 1)::varchar, 4, ' ')      as hi_wind,
  lpad(d.high_wind_day::varchar, 2, ' ')                    as high_wind_day,
  lpad(d.dom_dir::varchar, 3, ' ')                          as dom_dir

from range r
left outer join (  -- This is unit-converted report data, d:
  select
    -- Common Columns
    to_char(data.date, 'YY')                                                              as year,
    extract(month from data.date)                                                         as month,

    -- Temperature Table
    case when p.fahrenheit then data.max_avg_temp  * 1.8 + 32 else data.max_avg_temp  end as mean_max,
    case when p.fahrenheit then data.min_avg_temp  * 1.8 + 32 else data.min_avg_temp  end as mean_min,
    case when p.fahrenheit then data.avg_temp      * 1.8 + 32 else data.avg_temp      end as mean,
    case when p.fahrenheit then data.dep_temp_norm * 1.8 + 32 else data.dep_temp_norm end as dep_norm_temp,
    case when p.fahrenheit
      then data.tot_heat_degree_days * 1.8 + 32 else data.tot_heat_degree_days        end as heat_deg_days,
    case when p.fahrenheit
      then data.tot_cool_degree_days * 1.8 + 32 else data.tot_cool_degree_days        end as cool_deg_days,
    case when p.fahrenheit then data.max_temp      * 1.8 + 32 else data.max_temp      end as hi_temp,
    extract(day from data.max_temp_date)                                                  as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    extract(day from data.min_temp_date)                                                  as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.dep_rain_norm * 1.0/25.4 else data.dep_rain_norm     end as dep_norm_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    extract(day from data.max_rain_date)                                                  as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round((data.avg_wind * 3.6)::numeric,1)
         when p.mph then round((data.avg_wind * 2.23694)::numeric,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round((data.max_wind * 3.6)::numeric,1)
         when p.mph then round((data.max_wind * 2.23694)::numeric,1)
         else            data.max_wind                                                end as max_wind,
    extract(day from data.max_wind_date)                                                  as high_wind_day,
    data.dom_wind_direction                                                               as dom_dir

  from (
    select
      match.date,
      match.station_id,

      -- Rain counts
      match.rain_over_02,
      match.rain_over_2,
      match.rain_over_20,

      -- Temperature counts
      match.max_high_temp,
      match.max_low_temp,
      match.min_high_temp,
      match.min_low_temp,

      -- Monthly aggregates - temperature
      match.max_avg_temp,
      match.min_avg_temp,
      match.avg_temp,
      match.dep_temp_norm,
      match.tot_cool_degree_days,
      match.tot_heat_degree_days,
      match.max_temp,
      match.min_temp,
      max(match.max_temp_date) as max_temp_date,
      max(match.min_temp_date) as min_temp_date,

      -- Monthly aggregates - rain
      match.tot_rain,
      match.dep_rain_norm,
      match.max_rain,
      max(match.max_rain_date) as max_rain_date,

      -- Monthly aggregates - wind
      match.avg_wind,
      match.max_wind,
      max(match.max_wind_date) as max_wind_date,
      match.dom_wind_direction
    from (
      select
        monthly.date as date,
        monthly.station_id as station_id,

        -- Rain counts
        monthly.rain_over_02 as rain_over_02,
        monthly.rain_over_2 as rain_over_2,
        monthly.rain_over_20 as rain_over_20,

        -- Temperature counts
        monthly.max_high_temp as max_high_temp,
        monthly.max_low_temp as max_low_temp,
        monthly.min_high_temp as min_high_temp,
        monthly.min_low_temp as min_low_temp,

        -- Monthly aggregates - temperature
        monthly.max_avg_temp as max_avg_temp,
        monthly.min_avg_temp as min_avg_temp,
        monthly.avg_temp as avg_temp,
        monthly.avg_temp - norm.temp as dep_temp_norm,
        monthly.tot_cool_degree_days as tot_cool_degree_days,
        monthly.tot_heat_degree_days as tot_heat_degree_days,
        monthly.max_temp as max_temp,
        case when monthly.max_temp = daily.max_temp
          then daily.date else null end as max_temp_date,
        monthly.min_temp as min_temp,
        case when monthly.min_temp = daily.min_temp
          then daily.date else null end as min_temp_date,

        -- Monthly aggregates - rainfall
        monthly.tot_rain as tot_rain,
        monthly.tot_rain - norm.rain as dep_rain_norm,
        monthly.max_rain as max_rain,
        case when monthly.max_rain = daily.tot_rain
          then daily.date else null end as max_rain_date,

        -- Monthly aggregates - wind
        monthly.avg_wind as avg_wind,
        monthly.max_wind as max_wind,
        case when monthly.max_wind = daily.max_wind
          then daily.date else null end as max_wind_date,
        point.point as dom_wind_direction

      from monthly_aggregates monthly
      inner join normal_readings norm on norm.month = extract(month from monthly.date)
      inner join daily_aggregates daily ON
          daily.station_id = monthly.station_id
        and ( monthly.max_temp = daily.max_temp
          or monthly.min_temp = daily.min_temp
          or monthly.max_rain = daily.tot_rain
          or monthly.max_wind = daily.max_wind)
      left outer join dominant_wind_directions dwd on dwd.station_id = monthly.station_id and dwd.month = monthly.date
      LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
    ) as match
    group by
      match.date, match.station_id, match.rain_over_02, match.rain_over_2, match.rain_over_20,
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp, match.max_avg_temp,
      match.min_avg_temp, match.avg_temp, match.dep_temp_norm, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.dep_rain_norm, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.date, match.station_id
  ) as data, parameters p
) as d on d.year = r.year and d.month = r.month
cross join parameters p

    """

    # This is a bit ugly. Really we'd do all this work in Python but its easier
    # to do it this way to keep
    criteria_query = """
    WITH parameters AS (
  SELECT
	$celsius::boolean		    AS celsius,
	$fahrenheit::boolean    AS fahrenheit,
	$kmh::boolean			      AS kmh,
	$mph::boolean			      AS mph,
	$inches::boolean		    AS inches,
	$atDate::date      		  AS date,
    $title::varchar		      AS title,
    $city::varchar 			    AS city,
    $state::varchar   		  AS state,
    $altitude::FLOAT        AS altitude,
    $latitude::FLOAT        AS latitude,
    $longitude::FLOAT       AS longitude,
	$altFeet::BOOLEAN       AS altitude_feet,
	$coolBase::FLOAT		    AS cool_base,
	$heatBase::FLOAT		    AS heat_base,
	32::integer             AS max_high_temp,
  0::integer              AS max_low_temp,
  0::integer              AS min_high_temp,
  -18::integer            AS min_low_temp,
  0.2::numeric            AS rain_02,
  2.0::numeric            AS rain_2,
  20.0::numeric           AS rain_20
),
latitude_dms as (
  select floor(abs(latitude)) as degrees,
         floor(60 * (abs(latitude) - floor(abs(latitude)))) as minutes,
         3600 * (abs(latitude) - floor(abs(latitude))) - 60 * floor(60 * (abs(latitude) - floor(abs(latitude)))) as seconds,
         case when latitude < 0 then 'S' else 'N' end as direction
  from parameters
),
longitude_dms as (
  select floor(abs(longitude)) as degrees,
         floor(60 * (abs(longitude) - floor(abs(longitude)))) as minutes,
         3600 * (abs(longitude) - floor(abs(longitude))) - 60 * floor(60 * (abs(longitude) - floor(abs(longitude)))) as seconds,
         case when longitude < 0 then 'W' else 'E' end as direction
  from parameters
)
select
  to_char(p.date, 'MON')                  AS month,
  extract(year from p.date)               AS year,
  p.title                                 AS title,
  p.city                                  AS city,
  p.state                                 AS state,
  lpad(round((
    CASE WHEN p.altitude_feet THEN p.altitude * 3.28084
         ELSE p.altitude
    END)::NUMERIC, 0)::varchar, 5, ' ')   AS altitude,
  CASE WHEN p.altitude_feet THEN 'ft'
       ELSE 'm '
    END                                   AS altitude_units,
  lpad(lat.degrees || '° ' || lat.minutes || ''' ' || round(lat.seconds::numeric, 0) || '" ' || lat.direction, 14, ' ')      AS latitude,
  lpad(long.degrees || '° ' || long.minutes || ''' ' || round(long.seconds::numeric, 0) || '" ' || long.direction, 14, ' ')  AS longitude,
  CASE WHEN p.celsius THEN '°C'
       ELSE '°F' END                      AS temperature_units,
  CASE WHEN p.inches THEN 'in'
       ELSE 'mm' END                      AS rain_units,
  CASE WHEN p.kmh THEN 'km/h'
       WHEN p.mph THEN 'mph'
       ELSE 'm/s' END                     AS wind_units,
  lpad(round((
	CASE WHEN p.celsius THEN p.cool_base
	     ELSE p.cool_base * 1.8 + 32
	END)::numeric, 1)::varchar, 4, ' ')   AS cool_base,
  lpad(round((
    CASE WHEN p.celsius THEN p.heat_base
         ELSE p.heat_base * 1.8 + 32
	END)::numeric, 1)::varchar, 4, ' ')   AS heat_base,
  -- >=90 <=32 <=32 <=0
  -- >=32 <=0  <=0  <=-18
  '>=' || rpad(case when p.fahrenheit then (p.max_high_temp * 1.8 + 32)::integer
   else p.max_high_temp end::varchar, 2, ' ') as max_high_temp,
  '<=' || rpad(case when p.fahrenheit then (p.max_low_temp * 1.8 + 32)::integer
   else p.max_low_temp end::varchar, 2, ' ') as max_low_temp,
  '<=' || rpad(case when p.fahrenheit then (p.min_high_temp * 1.8 + 32)::integer
   else p.min_high_temp end::varchar, 2, ' ') as min_high_temp,
  '<=' || case when p.fahrenheit then (p.min_low_temp * 1.8 + 32)::integer
   else p.min_low_temp end::varchar as min_low_temp,
  lpad(case when p.inches then to_char(round(p.rain_02  * 1.0/25.4, 2), 'FM9.99') else to_char(p.rain_02, 'FM9.9') end::varchar, 3, ' ') as rain_02,
  lpad(case when p.inches then to_char(round(p.rain_2   * 1.0/25.4, 1), 'FM9.9') else round(p.rain_2, 0)::varchar end::varchar, 2, ' ') as rain_2,
  lpad(case when p.inches then round(p.rain_20  * 1.0/25.4, 0)  else round(p.rain_20, 0) end::varchar, 2, ' ') as rain_20
from parameters p, latitude_dms as lat, longitude_dms as long;
"""

    yearly_data = db.query(yearly_query, params)[0]
    monthly_data = list(db.query(monthly_query, params))
    criteria_data = None
    if include_criteria:
        criteria_data = db.query(criteria_query, params)[0]

    return monthly_data, yearly_data, criteria_data


def get_noaa_month_data(station_code, year, month, include_criteria=True):
    if station_code not in config.report_settings or \
            "noaa_year" not in config.report_settings[station_code]:
        return None

    params = config.report_settings[station_code]["noaa_year"]

    stations = get_full_station_info(not config.hide_coordinates)

    station_info = None
    for x in stations:
        if x['code'].lower() == station_code.lower():
            station_info = x
            break

    if station_info is None:
        return None

    params.update({
        "start": date(year=year, month=month, day=1),
        "end": date(year=year, month=month+1, day=1) - timedelta(days=1),
        "atDate": date(year=year, month=month, day=1),
        "title": station_info['name'],
        "altitude": station_info['coordinates']['altitude'],
        "latitude": station_info['coordinates']['latitude'],
        "longitude": station_info['coordinates']['longitude'],
    })

    month_query = """
    WITH parameters AS (
   SELECT
     $heatBase::float    	              AS heat_base,
     $coolBase::float    	              AS cool_base,
     $start::date                       AS start_date,
     $end::date                         AS end_date,
   	 $fahrenheit::boolean	              AS fahrenheit, -- instead of celsius
   	 $kmh::boolean			                AS kmh,	-- instead of m/s
   	 $mph::boolean			                AS mph,	-- instead of m/s
   	 $inches::boolean		                AS inches, -- instead of mm
     $stationCode::varchar(5)           AS stationCode,
     32::integer                        AS max_high_temp,
     0::integer                         AS max_low_temp,
     0::integer                         AS min_high_temp,
     -18::integer                       AS min_low_temp,
     0.2::float                         AS rain_02,
     2.0::float                         AS rain_2,
     20.0::float                        AS rain_20
 ), compass_points AS (
  SELECT
	column1 AS idx,
	column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
), daily_aggregates as (
  select
      -- Day info
      aggregates.date                                         AS date,
      aggregates.station_id                                   AS station_id,
      -- Day Counts
      case when aggregates.tot_rain >= parameters.rain_02
        then 1 else 0 end                                     AS rain_over_02,
      case when aggregates.tot_rain >= parameters.rain_2
        then 1 else 0 end                                     AS rain_over_2,
      case when aggregates.tot_rain >= parameters.rain_20
        then 1 else 0 end                                     AS rain_over_20,
      case when aggregates.max_temp >= parameters.max_high_temp
        then 1 else 0 end                                     AS max_high_temp,
      case when aggregates.max_temp <= parameters.max_low_temp
        then 1 else 0 end                                     AS max_low_temp,
      case when aggregates.min_temp <= parameters.min_high_temp
        then 1 else 0 end                                     AS min_high_temp,
      case when aggregates.min_temp <= parameters.min_low_temp
        then 1 else 0 end                                     AS min_low_temp,
      -- day aggregates
      aggregates.max_temp                                     AS max_temp,
      aggregates.min_temp                                     AS min_temp,
      aggregates.avg_temp                                     AS avg_temp,
      aggregates.tot_rain                                     AS tot_rain,
      aggregates.avg_wind                                     AS avg_wind,
      aggregates.max_wind                                     AS max_wind,
      aggregates.tot_cool_degree_days                         AS tot_cool_degree_days,
      aggregates.tot_heat_degree_days                         AS tot_heat_degree_days
    from (
      select   -- Various aggregates per day
        s.time_stamp::date                          AS date,
        s.station_id                                AS station_id,
        round(max(temperature)::numeric,1)          AS max_temp,
        round(min(temperature)::numeric,1)          AS min_temp,
        round(avg(temperature)::numeric,1)          AS avg_temp,
        round(sum(rainfall)::numeric,1)             AS tot_rain,
        round(avg(average_wind_speed)::numeric,1)   AS avg_wind,
        round(max(gust_wind_speed)::numeric,1)      AS max_wind,
        sum(cool_degree_days)                       AS tot_cool_degree_days,
        sum(heat_degree_days)                       AS tot_heat_degree_days
      from (
        select
          time_stamp,
          s.station_id,
          temperature,
          rainfall,
          average_wind_speed,
          gust_wind_speed,
          case when temperature > p.cool_base
            then temperature - p.cool_base
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as cool_degree_days,
          case when temperature < p.heat_base
            then p.heat_base - temperature
          else 0 end * (stn.sample_interval :: NUMERIC / 86400.0) as heat_degree_days
        from parameters p, sample s
        inner join station stn on stn.station_id = s.station_id
        where stn.code = p.stationCode
      ) s, parameters
      where s.time_stamp::date between parameters.start_date::date and parameters.end_date::date
      group by s.time_stamp::date, s.station_id
    ) as aggregates, parameters
  group by
    aggregates.date, aggregates.tot_rain, aggregates.max_temp, aggregates.min_temp, aggregates.station_id,
    aggregates.avg_temp, aggregates.avg_wind, aggregates.max_wind, aggregates.tot_cool_degree_days,
    aggregates.tot_heat_degree_days, parameters.rain_02, parameters.rain_2, parameters.rain_20,
    parameters.max_high_temp, parameters.max_low_temp, parameters.min_high_temp, parameters.min_low_temp
),
monthly_aggregates as (
  select
    date_trunc('month', agg.date)::date as date,
    agg.station_id as station_id,

    -- Monthly counts
    sum(agg.rain_over_02)               as rain_over_02,
    sum(agg.rain_over_2)                as rain_over_2,
    sum(agg.rain_over_20)               as rain_over_20,
    sum(agg.max_high_temp)              as max_high_temp,
    sum(agg.max_low_temp)               as max_low_temp,
    sum(agg.min_high_temp)              as min_high_temp,
    sum(agg.min_low_temp)               as min_low_temp,

    -- Monthly aggregates - temperature
    avg(agg.avg_temp)                   as avg_temp,

    sum(agg.tot_cool_degree_days)       as tot_cool_degree_days,
    sum(agg.tot_heat_degree_days)       as tot_heat_degree_days,
    max(agg.max_temp)                   as max_temp,
    min(agg.min_temp)                   as min_temp,

    -- Monthly aggregates - rainfall
    sum(agg.tot_rain)                   as tot_rain,

    max(agg.tot_rain)                   as max_rain,

    -- Monthly aggregates - wind
    avg(agg.avg_wind)                   as avg_wind,
    max(agg.max_wind)                   as max_wind

  from daily_aggregates as agg
  group by date_trunc('month', agg.date)::date, agg.station_id
), dominant_wind_directions as (
  WITH wind_directions AS (
    SELECT
      station_id,
      date_trunc('month', time_stamp)::date as month,
      wind_direction,
      count(wind_direction) AS count
    FROM sample s, parameters p
    where date_trunc('month', s.time_stamp)::date between p.start_date and p.end_date and s.wind_direction is not null
    GROUP BY date_trunc('month', s.time_stamp) :: DATE, station_id, wind_direction
  ), max_count AS (
    SELECT
      station_id,
      month,
      max(count) AS max_count
    FROM wind_directions AS counts
    GROUP BY station_id, month
  )
  SELECT
    d.station_id,
    d.month,
    min(wind_direction) AS wind_direction
  FROM wind_directions d
    INNER JOIN max_count mc ON mc.station_id = d.station_id AND mc.month = d.month AND mc.max_count = d.count
  GROUP BY d.station_id, d.month
)
select
  -- Temperature
  lpad(round(d.mean::numeric, 1)::varchar, 5, ' ')          as mean,
  lpad(round(d.heat_deg_days::numeric, 1)::varchar, 5, ' ') as heat_dd,
  lpad(round(d.cool_deg_days::numeric, 1)::varchar, 5, ' ') as cool_dd,
  lpad(round(d.hi_temp::numeric, 1)::varchar, 5, ' ')       as hi_temp,
  lpad(d.hi_temp_date::varchar, 2, ' ')                     as hi_temp_date,
  lpad(round(d.low::numeric, 1)::varchar, 5, ' ')           as low_temp,
  lpad(d.low_date::varchar, 2, ' ')                         as low_temp_date,
  lpad(d.max_high::varchar, 2, ' ')                         as max_high,
  lpad(d.max_low::varchar, 2, ' ')                          as max_low,
  lpad(d.min_high::varchar, 2, ' ')                         as min_high,
  lpad(d.min_low::varchar, 2, ' ')                          as min_low,

  -- Rain
  lpad(case when p.inches
    then round(d.tot_rain::numeric, 2)
    else round(d.tot_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as tot_rain,
  lpad(case when p.inches
    then round(d.max_obs_rain::numeric, 2)
    else round(d.max_obs_rain::numeric, 1)
    end::varchar, 5, ' ')                                   as max_obs_rain,
  d.max_obs_rain_day::varchar                               as max_obs_rain_day,
  lpad(d.rain_02::varchar, 2, ' ')                          as rain_02,
  lpad(d.rain_2::varchar, 2, ' ')                           as rain_2,
  lpad(d.rain_20::varchar, 2, ' ')                          as rain_20,

  -- Wind
  lpad(round(d.avg_wind::numeric, 1)::varchar, 4, ' ')      as avg_wind,
  lpad(round(d.max_wind::numeric, 1)::varchar, 4, ' ')      as hi_wind,
  lpad(d.high_wind_day::varchar, 2, ' ')                    as high_wind_day,
  lpad(d.dom_dir::varchar, 3, ' ')                          as dom_dir

from (  -- This is unit-converted report data, d:
  select
    -- Temperature Table
    case when p.fahrenheit then data.avg_temp      * 1.8 + 32 else data.avg_temp      end as mean,
    case when p.fahrenheit
      then data.tot_heat_degree_days * 1.8 + 32 else data.tot_heat_degree_days        end as heat_deg_days,
    case when p.fahrenheit
      then data.tot_cool_degree_days * 1.8 + 32 else data.tot_cool_degree_days        end as cool_deg_days,
    case when p.fahrenheit then data.max_temp      * 1.8 + 32 else data.max_temp      end as hi_temp,
    extract(day from data.max_temp_date)                                                  as hi_temp_date,
    case when p.fahrenheit then data.min_temp      * 1.8 + 32 else data.min_temp      end as low,
    extract(day from data.min_temp_date)                                                  as low_date,
    data.max_high_temp                                                                    as max_high,
    data.max_low_temp                                                                     as max_low,
    data.min_high_temp                                                                    as min_high,
    data.min_low_temp                                                                     as min_low,

    -- Rain table
    case when p.inches then data.tot_rain      * 1.0/25.4 else data.tot_rain          end as tot_rain,
    case when p.inches then data.max_rain      * 1.0/25.4 else data.max_rain          end as max_obs_rain,
    data.max_rain_date::date                                                              as max_obs_rain_day,
    data.rain_over_02                                                                     as rain_02,
    data.rain_over_2                                                                      as rain_2,
    data.rain_over_20                                                                     as rain_20,

    -- Wind table
    case when p.kmh then round((data.avg_wind * 3.6)::numeric,1)
         when p.mph then round((data.avg_wind * 2.23694)::numeric,1)
         else            data.avg_wind                                                end as avg_wind,
    case when p.kmh then round((data.max_wind * 3.6)::numeric,1)
         when p.mph then round((data.max_wind * 2.23694)::numeric,1)
         else            data.max_wind                                                end as max_wind,
    extract(day from data.max_wind_date)                                                  as high_wind_day,
    data.dom_wind_direction                                                               as dom_dir

  from (
    select
      match.date,
      match.station_id,

      -- Rain counts
      match.rain_over_02,
      match.rain_over_2,
      match.rain_over_20,

      -- Temperature counts
      match.max_high_temp,
      match.max_low_temp,
      match.min_high_temp,
      match.min_low_temp,

      -- Monthly aggregates - temperature
      match.avg_temp,
      match.tot_cool_degree_days,
      match.tot_heat_degree_days,
      match.max_temp,
      match.min_temp,
      max(match.max_temp_date) as max_temp_date,
      max(match.min_temp_date) as min_temp_date,

      -- Monthly aggregates - rain
      match.tot_rain,
      match.max_rain,
      max(match.max_rain_date) as max_rain_date,

      -- Monthly aggregates - wind
      match.avg_wind,
      match.max_wind,
      max(match.max_wind_date) as max_wind_date,
      match.dom_wind_direction
    from (
      select
        monthly.date as date,
        monthly.station_id as station_id,

        -- Rain counts
        monthly.rain_over_02 as rain_over_02,
        monthly.rain_over_2 as rain_over_2,
        monthly.rain_over_20 as rain_over_20,

        -- Temperature counts
        monthly.max_high_temp as max_high_temp,
        monthly.max_low_temp as max_low_temp,
        monthly.min_high_temp as min_high_temp,
        monthly.min_low_temp as min_low_temp,

        -- Monthly aggregates - temperature
        monthly.avg_temp as avg_temp,
        monthly.tot_cool_degree_days as tot_cool_degree_days,
        monthly.tot_heat_degree_days as tot_heat_degree_days,
        monthly.max_temp as max_temp,
        case when monthly.max_temp = daily.max_temp
          then daily.date else null end as max_temp_date,
        monthly.min_temp as min_temp,
        case when monthly.min_temp = daily.min_temp
          then daily.date else null end as min_temp_date,

        -- Monthly aggregates - rainfall
        monthly.tot_rain as tot_rain,
        monthly.max_rain as max_rain,
        case when monthly.max_rain = daily.tot_rain
          then daily.date else null end as max_rain_date,

        -- Monthly aggregates - wind
        monthly.avg_wind as avg_wind,
        monthly.max_wind as max_wind,
        case when monthly.max_wind = daily.max_wind
          then daily.date else null end as max_wind_date,
        point.point as dom_wind_direction

      from monthly_aggregates monthly
      inner join daily_aggregates daily ON
          daily.station_id = monthly.station_id
        and ( monthly.max_temp = daily.max_temp
          or monthly.min_temp = daily.min_temp
          or monthly.max_rain = daily.tot_rain
          or monthly.max_wind = daily.max_wind)
      left outer join dominant_wind_directions dwd on dwd.station_id = monthly.station_id and dwd.month = monthly.date
      LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
    ) as match
    group by
      match.date, match.station_id, match.rain_over_02, match.rain_over_2, match.rain_over_20,
      match.max_high_temp, match.max_low_temp, match.min_high_temp, match.min_low_temp,
      match.avg_temp, match.tot_cool_degree_days,
      match.tot_heat_degree_days, match.max_temp, match.tot_rain, match.max_rain,
      match.avg_wind, match.max_wind, match.dom_wind_direction, match.min_temp
    order by match.date, match.station_id
  ) as data, parameters p
) as d
cross join parameters p
"""

    day_query = """
WITH parameters AS (
  SELECT
    $heatBase::float    	AS heat_base,
    $coolBase::float    	AS cool_base,
    $start::date  			AS start_date,
    $end::date  			AS end_date,
	$fahrenheit::boolean	AS fahrenheit, -- instead of celsius
	$kmh::boolean			AS kmh,	-- instead of m/s
	$mph::boolean			AS mph,	-- instead of m/s
	$inches::boolean		AS inches -- instead of mm
), compass_points AS (
  SELECT
	column1 AS idx,
	column2 AS point
  FROM (VALUES
    (0, 'N'), (1, 'NNE'), (2, 'NE'), (3, 'ENE'), (4, 'E'), (5, 'ESE'), (6, 'SE'), (7, 'SSE'), (8, 'S'), (9, 'SSW'),
    (10, 'SW'), (11, 'WSW'), (12, 'W'), (13, 'WNW'), (14, 'NW'), (15, 'NNW')) AS t
)
SELECT
  lpad(extract(day from dates.date)::varchar, 2, ' ')					AS day,
  lpad(round((
	case when p.fahrenheit then records.avg_temp * 1.8 + 32
		 else records.avg_temp
	end)::numeric, 1)::varchar, 4, ' ')    								AS mean_temp,
  lpad(round((
	case when p.fahrenheit then records.high_temp * 1.8 + 32
		 else records.high_temp
	end)::numeric, 1)::varchar, 4, ' ')									AS high_temp,
  lpad(to_char(records.high_temp_time, 'fmHH24:MI'), 5, ' ')			AS high_temp_time,
  lpad(round((
	case when p.fahrenheit then records.low_temp * 1.8 + 32
		 else records.low_temp
	end)::numeric, 1)::varchar, 4, ' ')									AS low_temp,
  lpad(to_char(records.low_temp_time, 'fmHH24:MI'), 5, ' ')				AS low_temp_time,
  lpad(round((
	case when p.fahrenheit then records.heat_degree_days * 1.8 + 32
		 else records.heat_degree_days
	end)::numeric, 1)::varchar, 4, ' ')									AS heat_degree_days,
  lpad(round((
	case when p.fahrenheit then records.cool_degree_days * 1.8 + 32
		 else records.cool_degree_days
	end)::numeric, 1)::varchar, 4, ' ')									AS cool_degree_days,
  lpad((
	case when p.inches then round((records.rainfall * 1.0/25.4)::NUMERIC, 2)
		 else round(records.rainfall::NUMERIC, 1)
	end)::varchar, 4, ' ')												AS rain,
  lpad(round(
	case when p.mph then records.avg_wind * 2.23694
		 when p.kmh then records.avg_wind * 3.6
		 else records.avg_wind
	end::NUMERIC, 1)::varchar, 4, ' ')									AS avg_wind_speed,
  lpad(round((
	case when p.mph then records.high_wind * 2.23694
		 when p.kmh then records.high_wind * 3.6
		 else records.high_wind
	end)::NUMERIC, 1)::varchar, 4, ' ')									AS high_wind,
  lpad(to_char(records.high_wind_time, 'fmHH24:MI'), 5, ' ' )			AS high_wind_time,
  lpad(point.point::varchar, 3, ' ')									AS dom_wind_dir
from parameters p, (
  SELECT
    generate_series(start_date, end_date, '1 day' :: INTERVAL) :: DATE as date,
    (select station_id from station where lower(code) = lower(($stationCode)::varchar(5))) as station_id
  from parameters
) as dates
LEFT OUTER JOIN (
  SELECT
    data.date_stamp                       AS date,
    data.station_id,
    data.max_gust_wind_speed              AS high_wind,
    max(data.max_gust_wind_speed_ts)      AS high_wind_time,
    data.min_temperature                  AS low_temp,
    max(data.min_temperature_ts)          AS low_temp_time,
    data.max_temperature                  AS high_temp,
    max(data.max_temperature_ts)          AS high_temp_time,
    data.avg_temperature                  AS avg_temp,
    data.avg_wind_speed                   AS avg_wind,
    data.rainfall                         AS rainfall,
    data.heat_degree_days                 AS heat_degree_days,
    data.cool_degree_days                 AS cool_degree_days
  FROM (SELECT
          dr.date_stamp,
          dr.station_id,
          dr.max_gust_wind_speed,
          dr.avg_temperature,
          dr.avg_wind_speed,
          dr.rainfall,
          dr.heat_degree_days,
          dr.cool_degree_days,
          CASE WHEN (s.gust_wind_speed = dr.max_gust_wind_speed)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS max_gust_wind_speed_ts,
          dr.max_temperature,
          CASE WHEN (s.temperature = dr.max_temperature)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS max_temperature_ts,
          dr.min_temperature,
          CASE WHEN (s.temperature = dr.min_temperature)
            THEN s.time_stamp
          ELSE NULL :: TIMESTAMP WITH TIME ZONE END AS min_temperature_ts
        FROM (sample s
          JOIN (SELECT
                  (s.time_stamp) :: DATE                 AS date_stamp,
                  s.station_id,
                  max(s.gust_wind_speed)                 AS max_gust_wind_speed,
                  min(s.temperature)                     AS min_temperature,
                  max(s.temperature)                     AS max_temperature,
                  avg(s.temperature)                     AS avg_temperature,
                  avg(s.average_wind_speed)              AS avg_wind_speed,
                  sum(s.rainfall)                        AS rainfall,
                  sum(s.heat_dd)                         AS heat_degree_days,
                  sum(s.cool_dd)                         AS cool_degree_days
                FROM (
                  SELECT
                    x.time_stamp,
                    x.station_id,
                    x.temperature,
                    x.average_wind_speed,
                    x.gust_wind_speed,
                    x.rainfall,
                    CASE WHEN x.temperature > p.cool_base
                      THEN x.temperature - p.cool_base
                    ELSE 0
                    END * (stn.sample_interval :: NUMERIC / 86400.0) AS cool_dd,
                    CASE WHEN x.temperature < p.heat_base
                      THEN p.heat_base - x.temperature
                    ELSE 0
                    END * (stn.sample_interval :: NUMERIC / 86400.0) AS heat_dd
                  from parameters p, sample x
                  inner join station stn on stn.station_id = x.station_id
                  where x.time_stamp::date between p.start_date and p.end_date
                ) s, parameters p
                where s.time_stamp::date between p.start_date and p.end_date
                GROUP BY s.time_stamp :: DATE, s.station_id
               ) dr ON (s.time_stamp :: DATE = dr.date_stamp and s.station_id = dr.station_id))
        WHERE s.gust_wind_speed = dr.max_gust_wind_speed OR
                ((s.temperature = dr.max_temperature) OR (s.temperature = dr.min_temperature))
       ) data
  GROUP BY data.date_stamp, data.station_id, data.max_gust_wind_speed, data.max_temperature, data.min_temperature,
    data.avg_wind_speed, data.avg_temperature, data.rainfall, data.heat_degree_days, data.cool_degree_days
) as records on records.date = dates.date and records.station_id = dates.station_id
LEFT OUTER JOIN (
    WITH wind_directions AS (
        SELECT
          time_stamp :: DATE    AS date,
          station_id,
          wind_direction,
          count(wind_direction) AS count
        FROM sample s, parameters p
        where s.time_stamp::date between p.start_date and p.end_date and s.wind_direction is not null
        GROUP BY time_stamp :: DATE, station_id, wind_direction
    ),
        max_count AS (
          SELECT
            date,
            station_id,
            max(count) AS max_count
          FROM wind_directions AS counts
          GROUP BY date, station_id
      )
    SELECT
      d.date,
      d.station_id,
      min(wind_direction) AS wind_direction
    FROM wind_directions d
      INNER JOIN max_count mc ON mc.date = d.date AND mc.station_id = d.station_id AND mc.max_count = d.count
    GROUP BY d.date, d.station_id
    ) as dwd on dwd.station_id = records.station_id and dwd.date = records.date
LEFT OUTER JOIN compass_points point ON point.idx = (((dwd.wind_direction * 100) + 1125) % 36000) / 2250
order by dates.date asc
;
    """

    criteria_query = """
WITH parameters AS (
  SELECT
	$celsius::boolean		AS celsius,
	$fahrenheit::boolean    AS fahrenheit,
	$kmh::boolean			AS kmh,
	$mph::boolean			AS mph,
	$inches::boolean		AS inches,
	$atDate::date      		AS date,
    $title::varchar		    AS title,
    $city::varchar 			AS city,
    $state::varchar   		AS state,
    $altitude::FLOAT        AS altitude,
    $latitude::FLOAT        AS latitude,
    $longitude::FLOAT       AS longitude,
	$altFeet::BOOLEAN       AS altitude_feet,
	$coolBase::FLOAT		AS cool_base,
	$heatBase::FLOAT		AS heat_base,
	32::INTEGER             AS max_high_temp,
    0::INTEGER              AS max_low_temp,
    0::INTEGER              AS min_high_temp,
    -18::INTEGER            AS min_low_temp,
    0.2                     AS rain_02,
    2.0                     AS rain_2,
    20.0                    AS rain_20
),
latitude_dms as (
  select floor(abs(latitude)) as degrees,
         floor(60 * (abs(latitude) - floor(abs(latitude)))) as minutes,
         3600 * (abs(latitude) - floor(abs(latitude))) - 60 * floor(60 * (abs(latitude) - floor(abs(latitude)))) as seconds,
         case when latitude < 0 then 'S' else 'N' end as direction
  from parameters
),
longitude_dms as (
  select floor(abs(longitude)) as degrees,
         floor(60 * (abs(longitude) - floor(abs(longitude)))) as minutes,
         3600 * (abs(longitude) - floor(abs(longitude))) - 60 * floor(60 * (abs(longitude) - floor(abs(longitude)))) as seconds,
         case when longitude < 0 then 'W' else 'E' end as direction
  from parameters
), ranges AS (
  select
    case when p.fahrenheit then (p.max_high_temp * 1.8 + 32)::integer
         else p.max_high_temp end as max_high_temp,
    case when p.fahrenheit then (p.max_low_temp * 1.8 + 32)::integer
         else p.max_low_temp end as max_low_temp,
    case when p.fahrenheit then (p.min_high_temp * 1.8 + 32)::integer
         else p.min_high_temp end as min_high_temp,
    case when p.fahrenheit then (p.min_low_temp * 1.8 + 32)::integer
         else p.min_low_temp end as min_low_temp
  from parameters p
)
select
  to_char(p.date, 'MON')                  AS month,
  extract(year from p.date)::integer      AS year,
  p.title                                 AS title,
  p.city                                  AS city,
  p.state                                 AS state,
  lpad(round((
    CASE WHEN p.altitude_feet THEN p.altitude * 3.28084
         ELSE p.altitude
    END)::NUMERIC, 0)::varchar, 5, ' ')   AS altitude,
  CASE WHEN p.altitude_feet THEN 'ft'
       ELSE 'm '
    END                                   AS altitude_units,
  lpad(lat.degrees || '° ' || lat.minutes || ''' ' || round(lat.seconds::numeric, 0) || '" ' || lat.direction, 14, ' ')      AS latitude,
  lpad(long.degrees || '° ' || long.minutes || ''' ' || round(long.seconds::numeric, 0) || '" ' || long.direction, 14, ' ')  AS longitude,
  CASE WHEN p.celsius THEN '°C'
       ELSE '°F' END                      AS temperature_units,
  CASE WHEN p.inches THEN 'in'
       ELSE 'mm' END                      AS rain_units,
  CASE WHEN p.kmh THEN 'km/h'
       WHEN p.mph THEN 'mph'
       ELSE 'm/s' END                     AS wind_units,
  lpad(round((
	CASE WHEN p.celsius THEN p.cool_base
	     ELSE p.cool_base * 1.8 + 32 
	END)::numeric, 1)::varchar, 5, ' ')   AS cool_base,
  lpad(round((
    CASE WHEN p.celsius THEN p.heat_base
         ELSE p.heat_base * 1.8 + 32 
	END)::numeric, 1)::varchar, 5, ' ')   AS heat_base,
  lpad(round(r.max_high_temp::numeric, 1)::varchar, 5, ' ') as max_high_temp,
  lpad(round(r.max_low_temp::numeric, 1)::varchar, 5, ' ') as max_low_temp,
  lpad(round(r.min_high_temp::numeric, 1)::varchar, 5, ' ') as min_high_temp,
  lpad(round(r.min_low_temp::numeric, 1)::varchar, 5, ' ') as min_low_temp,
  case when p.inches then to_char(round(p.rain_02  * 1.0/25.4, 2), 'FM9.99') || ' in' else to_char(p.rain_02, 'FM9.9') || ' mm' end as rain_02_value,
  case when p.inches then to_char(round(p.rain_2   * 1.0/25.4, 1), 'FM9.9') || ' in' else round(p.rain_2, 0)   || ' mm' end as rain_2_value,
  case when p.inches then round(p.rain_20  * 1.0/25.4, 0) || ' in' else round(p.rain_20, 0)  || ' mm' end as rain_20_value
from parameters p, latitude_dms as lat, longitude_dms as long, ranges as r;
    """

    month_data = db.query(month_query, params)[0]
    daily_data = list(db.query(day_query, params))
    criteria_data = None
    if include_criteria:
        criteria_data = db.query(criteria_query, params)[0]

    return month_data, daily_data, criteria_data
