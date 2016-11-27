# coding=utf-8
"""
Various common functions to get data from the database.
"""
import json

from config import db
from datetime import datetime, date
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

    result = db.query("select station_id from station where code = $code",
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

    result = db.query("select code from station where station_id = $station",
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
    from station stn where stn.code = $station
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

    query = """select st.code
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        where s.station_id = $station"""

    result = db.query(query, dict(station=station_id))
    if len(result):
        return result[0].code
    else:
        return None



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
                   dd.solar_radiation"""

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
        select s.code, s.title, s.description, s.sort_order,
           st.code as hw_type_code, st.title as hw_type_name,
           sr.min_ts, sr.max_ts, s.message,
           s.message_timestamp, s.station_config, s.site_title, s.latitude,
           s.longitude, s.altitude
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
            }
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


def get_stations():
    """
    Gets a list of station code,name pairs for all stations in the database.
    :return:
    """
    result = db.query("select code, title from station order by sort_order asc,"
                      " title desc")

    stations = []

    for row in result:
        station = (row.code, row.title)
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

    result = db.query("select image_source_id, code, source_name, description "
                      "from image_source where station_id = $station",
                      dict(station=station_id))
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
      and LOWER(img_typ.code) = LOWER($type_code)
      and LOWER(img_src.code) = LOWER($source_code)
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
           img_typ.code
    from image img
    inner join image_type img_typ on img_typ.image_type_id = img.image_type_id
    where image_id = $image_id
    """

    result = db.query(query, dict(image_id=image_id))
    if len(result):
        return result[0]
    return None


def get_image_source_info(station_id):
    query = """
select lower(x.code) as code,
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
       last_image_type.code as last_image_type_code,
       last_image.title as last_image_title,
       last_image.description as last_image_description,
       last_image.mime_type as last_image_mime_type
from (
    select img_src.code,
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
               where i.image_source_id = img_src.image_source_id
               order by time_stamp desc
               limit 1
           ) as last_image_id,
           img_src.station_id
    from image_source img_src
) as x
inner join image last_image on last_image.image_id = x.last_image_id
inner join image_type last_image_type on last_image_type.image_type_id = last_image.image_type_id
where x.station_id = $station
    """

    result = db.query(query, dict(station=station_id))

    if len(result) == 0:
        return None

    return result

def get_image_source(station_id, source_code):

    result = db.query("""select image_source_id, source_name
from image_source where code = $source and station_id = $station""",
                      dict(source=source_code.upper(), station=station_id))
    if len(result):
        return result[0]
    return None


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
           it.code as type_code,
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


def image_exists(station, image_date, source_code, image_id):
    query = """
    select image_id
    from image i
    inner join image_source src on src.image_source_id = i.image_source_id
    inner join station stn on stn.station_id = src.station_id
    where stn.code = $station_code
      and i.time_stamp::date = $image_date
      and src.code = $source_code
      and i.image_id = $image_id
        """

    result = db.query(query, dict(station_code=station, image_date=image_date,
                                  source_code=source_code.upper(),
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
    query = """
    select i.time_stamp,
           src.code as source,
           i.image_id as id,
           i.mime_type,
           stn.code as station,
           it.code as type_code,
           i.title as title,
           i.description as description
    from image i
    inner join image_source src on src.image_source_id = i.image_source_id
    inner join image_type it on it.image_type_id = i.image_type_id
    inner join station stn on stn.station_id = src.station_id
    where src.image_source_id = $source_id
      and (i.time_stamp::date = $day or $day is null)
    order by i.time_stamp
    """

    result = db.query(query, dict(source_id=source_id, day=day))

    if len(result):
        return result
    return None


def get_most_recent_image_id_for_source(source_id):
    query = """
    select image_id, time_stamp, mime_type
    from image i
    where i.image_source_id = $source_id
    order by time_stamp DESC
    limit 1
    """

    result = db.query(query, dict(source_id=source_id))

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
           ds.high_uv_index
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
# client also uses this dataset for over-the-internet operation
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
       cur.time_stamp::time - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap,
       ds.average_uv_index as uv_index,
       ds.solar_radiation
from sample cur
inner join sample prev on prev.station_id = cur.station_id
        and prev.time_stamp = (
            select max(pm.time_stamp)
            from sample pm
            where pm.time_stamp < cur.time_stamp
            and pm.station_id = cur.station_id)
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(date_trunc('month',cur.time_stamp)) = $date
  and cur.station_id = $station
order by cur.time_stamp asc
        """

    params = dict(station=station_id, date=date(year=year,month=month,day=1))

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
# client also uses this dataset for over-the-internet operation
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
       cur.time_stamp::time - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap,
       ds.average_uv_index as uv_index,
       ds.solar_radiation
from sample cur
inner join sample prev on prev.time_stamp = (
        select max(x.time_stamp)
        from sample x
        where x.time_stamp < cur.time_stamp
        and x.station_id = cur.station_id) and prev.station_id = cur.station_id
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(cur.time_stamp) = $date
  and cur.station_id = $station
order by cur.time_stamp asc"""

    params = dict(station=station_id, date=date)

    return db.query(query, params)