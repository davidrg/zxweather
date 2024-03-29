# coding=utf-8
"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""
import mimetypes
import os
from datetime import date, time, datetime, timedelta

from cache import day_cache_control, rfcformat, cache_control_headers
from database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp, day_exists, get_station_id, \
    get_davis_max_wireless_packets, image_exists, get_image_metadata, get_image, \
    get_image_mime_type, get_day_data_wp, get_image_id, \
    get_image_type_and_ts_info, get_image_details, get_extra_sensors_enabled
import web
import config
import json
from data.util import outdoor_sample_result_to_json, rainfall_sample_result_to_json, indoor_sample_result_to_datatable, indoor_sample_result_to_json, outdoor_sample_result_to_datatable, rainfall_to_datatable

__author__ = 'David Goodwin'


def get_day_records(day, station_id):
    """
    Gets JSON data containing records for the day.
    :param day: The day to get records for
    :type day: date
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: The days records in JSON form
    :rtype: str
    """
    records = get_daily_records(day, station_id)

    data = {
        'date_stamp': str(records.date_stamp),
        'total_rainfall': records.total_rainfall,
        'max_gust_wind_speed': records.max_gust_wind_speed,
        'max_gust_wind_speed_ts': str(records.max_gust_wind_speed_ts),
        'max_average_wind_speed': records.max_average_wind_speed,
        'max_average_wind_speed_ts': str(records.max_average_wind_speed_ts),
        'min_absolute_pressure': records.min_absolute_pressure,
        'min_absolute_pressure_ts': str(records.min_absolute_pressure_ts),
        'max_absolute_pressure': records.max_absolute_pressure,
        'max_absolute_pressure_ts': str(records.max_absolute_pressure_ts),
        'min_sea_level_pressure': records.min_sea_level_pressure,
        'min_sea_level_pressure_ts': str(records.min_sea_level_pressure_ts),
        'max_sea_level_pressure': records.max_sea_level_pressure,
        'max_sea_level_pressure_ts': str(records.max_sea_level_pressure_ts),
        'min_pressure': records.min_pressure,
        'min_pressure_ts': str(records.min_pressure_ts),
        'max_pressure': records.max_pressure,
        'max_pressure_ts': str(records.max_pressure_ts),
        'min_apparent_temperature': records.min_apparent_temperature,
        'min_apparent_temperature_ts': str(records.min_apparent_temperature_ts),
        'max_apparent_temperature': records.max_apparent_temperature,
        'max_apparent_temperature_ts': str(records.max_apparent_temperature_ts),
        'min_wind_chill': records.min_wind_chill,
        'min_wind_chill_ts': str(records.min_wind_chill_ts),
        'max_wind_chill': records.max_wind_chill,
        'max_wind_chill_ts': str(records.max_wind_chill_ts),
        'min_dew_point': records.min_dew_point,
        'min_dew_point_ts': str(records.min_dew_point_ts),
        'max_dew_point': records.max_dew_point,
        'max_dew_point_ts': str(records.max_dew_point_ts),
        'min_temperature': records.min_temperature,
        'min_temperature_ts': str(records.min_temperature_ts),
        'max_temperature': records.max_temperature,
        'max_temperature_ts': str(records.max_temperature_ts),
        'min_humidity': records.min_humidity,
        'min_humidity_ts': str(records.min_humidity_ts),
        'max_humidity': records.max_humidity,
        'max_humidity_ts': str(records.max_humidity_ts),
        }

    # Find the most recent timestamp (used for Last-Modified header)
    age = records.min_absolute_pressure_ts
    if records.max_average_wind_speed_ts is not None and \
                    records.max_average_wind_speed_ts > age:
        age = records.max_average_wind_speed_ts

    if records.min_absolute_pressure_ts is not None and \
                    records.min_absolute_pressure_ts > age:
        age = records.min_absolute_pressure_ts
    if records.max_absolute_pressure_ts is not None and \
                    records.max_absolute_pressure_ts > age:
        age = records.max_absolute_pressure_ts
    if records.min_sea_level_pressure_ts is not None and \
       records.min_sea_level_pressure_ts > age:
        age = records.min_sea_level_pressure_ts
    if records.max_sea_level_pressure_ts is not None and \
       records.max_sea_level_pressure_ts > age:
        age = records.max_sea_level_pressure_ts
    if records.min_apparent_temperature_ts is not None and \
                    records.min_apparent_temperature_ts > age:
        age = records.min_apparent_temperature_ts
    if records.max_apparent_temperature_ts is not None and \
                    records.max_apparent_temperature_ts > age:
        age = records.max_apparent_temperature_ts
    if records.min_wind_chill_ts is not None and \
                    records.min_wind_chill_ts > age:
        age = records.min_wind_chill_ts
    if records.max_wind_chill_ts is not None and \
                    records.max_wind_chill_ts > age:
        age = records.max_wind_chill_ts
    if records.min_dew_point_ts is not None and \
                    records.min_dew_point_ts > age:
        age = records.min_dew_point_ts
    if records.max_dew_point_ts is not None and \
                    records.max_dew_point_ts > age:
        age = records.max_dew_point_ts
    if records.min_temperature_ts is not None and \
                    records.min_temperature_ts > age:
        age = records.min_temperature_ts
    if records.max_temperature_ts is not None and \
                    records.max_temperature_ts > age:
        age = records.max_temperature_ts
    if records.min_humidity_ts is not None and \
                    records.min_humidity_ts > age:
        age = records.min_humidity_ts
    if records.max_humidity_ts is not None and \
                    records.max_humidity_ts > age:
        age = records.max_humidity_ts

    age = datetime.combine(day,age)

    json_data = json.dumps(data)

    day_cache_control(age, day, station_id)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_day_rainfall(day, station_id, use_24hr_range=False):
    """
    Gets JSON data containing total rainfall for the day and the past seven
    days.
    :param day: Date or timestamp
    :type day: date or datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :param use_24hr_range: If a 24-hour time range should be used rather than
    getting the total for the supplied day
    :type use_24hr_range: bool
    """
    rainfall = get_daily_rainfall(day, station_id, use_24hr_range)

    json_data = json.dumps(rainfall)
    day_cache_control(None, day, station_id)
    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_day_dataset(day, data_function, output_function, station_id):
    """
    Gets day-level JSON data using the supplied data function and then
    converts it to JSON using the supplied output function.

    :param day: Day to get data for.
    :param data_function: Function to supply data.
    :param output_function: Function to format JSON output.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data.
    """

    result = data_function(day, station_id)

    data,age = output_function(result)

    day_cache_control(age,day, station_id)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_samples_data(day, station_id):
    """
    Gets the full days samples.
    :param day: Day to get samples for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return:
    """
    params = dict(date=day, station=station_id)
    result = config.db.query("""select s.time_stamp::timestamptz,
               s.temperature,
               s.dew_point,
               s.apparent_temperature,
               s.wind_chill,
               s.relative_humidity,
               s.absolute_pressure,
               s.mean_sea_level_pressure,
               coalesce(s.mean_sea_level_pressure, s.absolute_pressure) as pressure,
               s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap,
               s.average_wind_speed,
               s.gust_wind_speed,
               ds.average_uv_index as uv_index,
               ds.solar_radiation
    from sample s
    inner join sample prev on prev.station_id = s.station_id
                          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
    inner join station st on st.station_id = prev.station_id
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where date(s.time_stamp) = $date
      and s.station_id = $station
    order by s.time_stamp asc"""
                             , params)

    return result

def get_24hr_samples_data(time, station_id):
    """
    Gets samples from the 24 hours leading up to the supplied time.
    :param time: Maximum time to get samples for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return:
    """

    # This query is identical to the get_day_samples_data one except for the
    # first condition in the where clause (where we filter by a 24 hr time
    # range instead of a specific date).
    params = dict(time = time, station=station_id)
    result = config.db.query("""select s.time_stamp::timestamptz,
               s.temperature,
               s.dew_point,
               s.apparent_temperature,
               s.wind_chill,
               s.relative_humidity,
               s.absolute_pressure,
               s.mean_sea_level_pressure,
               coalesce(s.mean_sea_level_pressure, s.absolute_pressure) as pressure,
               s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap,
               s.average_wind_speed,
               s.gust_wind_speed,
               ds.average_uv_index as uv_index,
               ds.solar_radiation
    from sample s
    inner join sample prev on prev.station_id = s.station_id
                          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
    inner join station st on st.station_id = prev.station_id
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where (s.time_stamp < $time and s.time_stamp > $time - '1 hour'::interval * 24)
      and s.station_id = $station
    order by s.time_stamp asc"""
        , params)
    return result

def get_7day_samples_data(day, station_id):
    """
    Gets samples for the 7-day period ending on the specified date.
    :param day: End date for the 7-day period
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Query data
    """
    params = dict(date = day, station = station_id)
    result = config.db.query("""select s.time_stamp,
           s.temperature,
           s.dew_point,
           s.apparent_temperature,
           s.wind_chill,
           s.relative_humidity,
           s.absolute_pressure,
           s.mean_sea_level_pressure,
           coalesce(s.mean_sea_level_pressure, s.absolute_pressure) as pressure,
           s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
           CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
              true
           else
              false
           end as gap,
           s.average_wind_speed,
           s.gust_wind_speed,
           ds.average_uv_index as uv_index,
           ds.solar_radiation
    from (select max(time_stamp) as ts from sample where date(time_stamp) = $date and station_id = $station) as max_ts
    inner join sample s on s.time_stamp <= max_ts.ts  -- 604800 seconds in a week.
      and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
    inner join sample prev on prev.station_id = s.station_id and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
    inner join station st on st.station_id = s.station_id
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    where s.station_id = $station
    order by s.time_stamp asc
    """, params)
    return result

def get_7day_30mavg_samples_data(day, station_id):
    """
    Gets 30-minute averaged sample data over the 7-day period ending on the
    specified day.
    :param day: end date.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Data.
    """
    params = dict(date = day, station = station_id)
    result = config.db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       avg(mean_sea_level_pressure) as mean_sea_level_pressure,
       avg(pressure) as pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       max(iq.gust_wind_speed) as gust_wind_speed,
       avg(iq.uv_index)::real as uv_index,
       avg(iq.solar_radiation)::real as solar_radiation
from (
        select cur.time_stamp,
               --(extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               (extract(epoch from cur.time_stamp::date) + date_part('hour', cur.time_stamp)::int) * 10 + date_part('minute', cur.time_stamp)::int / 30 as quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.mean_sea_level_pressure,
               coalesce(cur.mean_sea_level_pressure, cur.absolute_pressure) as pressure,
               cur.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap,
               cur.average_wind_speed,
               cur.gust_wind_speed,
               ds.average_uv_index as uv_index,
               ds.solar_radiation
        from (select max(time_stamp) as ts from sample where date(time_stamp) = $date and station_id = $station) as max_ts
        inner join sample cur on cur.time_stamp <= max_ts.ts  -- 604800 seconds in a week.
			     and cur.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
        inner join station st on st.station_id = cur.station_id
        inner join sample prev on prev.station_id = cur.station_id and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp and station_id = $station)
        left outer join davis_sample ds on ds.sample_id = cur.sample_id
        where cur.station_id = $station
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)
    return result


def get_168hr_30mavg_samples_data(day, station_id):
    """
    Gets 30-minute averaged sample data over the 7-day period ending on the
    specified time (rather than day).
    :param day: end time stamp.
    :type day: datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Data.
    """
    params = dict(time = day, station = station_id)
    result = config.db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       avg(mean_sea_level_pressure) as mean_sea_level_pressure,
       avg(pressure) as pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       max(iq.gust_wind_speed) as gust_wind_speed,
       avg(iq.uv_index)::real as uv_index,
       avg(iq.solar_radiation)::real as solar_radiation
from (
        select cur.time_stamp,
               --(extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               (extract(epoch from cur.time_stamp::date) + date_part('hour', cur.time_stamp)::int) * 10 + date_part('minute', cur.time_stamp)::int / 30 as quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.mean_sea_level_pressure,
               coalesce(cur.mean_sea_level_pressure, cur.absolute_pressure) as pressure,
               cur.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap,
               cur.average_wind_speed,
               cur.gust_wind_speed,
               ds.average_uv_index as uv_index,
               ds.solar_radiation
        from sample cur
        inner join station st on st.station_id = cur.station_id
        inner join sample prev on prev.station_id = cur.station_id
                              and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp and station_id = $station)
        left outer join davis_sample ds on ds.sample_id = cur.sample_id
        where cur.time_stamp <= $time  -- 604800 seconds in a week.
          and cur.time_stamp >= ($time - (604800 * '1 second'::interval))
          and cur.station_id = $station
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)
    return result


def get_days_hourly_rainfall_data(day, station_id):
    """
    Gets the days hourly rainfall data.
    :param day: Day to get rainfall data for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Rainfall data query result
    """

    params = dict(date = day, station = station_id)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample
    where time_stamp::date = $date
    and station_id = $station
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result

def get_24hr_rainfall_data(time, station_id):
    """
    Gets rainfall data for the last 24 hours.
    :param time: Time to get rainfall data for from.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Rainfall data query result
    """

    params = dict(time = time, station = station_id)

    result = config.db.query("""select time_stamp,
           rainfall
    from sample
    where time_stamp <= $time
      and time_stamp > $time - '1 hour'::interval * 24
    and station_id = $station
    order by time_stamp asc""", params)

    return result

def get_24hr_hourly_rainfall_data(time, station_id):
    """
    Gets the days hourly rainfall data.
    :param time: Time to get rainfall data for from.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Rainfall data query result
    """

    params = dict(time = time, station = station_id)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample
    where date_trunc('hour', time_stamp) <= date_trunc('hour', $time)
      and date_trunc('hour', time_stamp) > date_trunc('hour', $time) - '1 hour'::interval * 25
    and station_id = $station
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result


def get_24hr_reception(time, station_id):
    """
    Gets reception from the wireless sensors over the last 24 hours. This
    query is specific to Davis weather stations.
    :param time: Maximum time to get samples for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Reception data
    """

    max_packets = get_davis_max_wireless_packets(station_id)

    if max_packets is None:
        return None

    params = dict(time=time, station=station_id, maxpackets=max_packets)

    query = """select s.time_stamp::timestamptz,
       round((ds.wind_sample_count / $maxpackets * 100),1)::float as reception,

       s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
       CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
	  true
       else
	  false
       end as gap

    from sample s, davis_sample ds, sample prev
    inner join station st on st.station_id = prev.station_id
    where (s.time_stamp < $time and s.time_stamp > $time - '1 hour'::interval * 24)
      and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
      and s.station_id = $station
      and prev.station_id = $station
      and ds.sample_id = s.sample_id
    order by s.time_stamp asc
    """

    result = config.db.query(query, params)

    return result


def get_7day_hourly_rainfall_data(day, station_id):
    """
    Gets hourly rainfall data for the 7 day period ending at the specified date.
    :param day: End of the 7 day period
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Rainfall data query result.
    """

    params = dict(date = day, station = station_id)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample, (
        select max(time_stamp) as ts
        from sample
        where time_stamp::date = $date
        and station_id = $station) as max_ts
    where time_stamp <= max_ts.ts
      and time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
      and station_id = $station
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result


def get_168hr_hourly_rainfall_data(day, station_id):
    """
    Gets hourly rainfall data for the 168 hour period ending at the specified
    time (rather than date).
    :param day: End of the 168 hour period
    :type day: datetime
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Rainfall data query result.
    """

    params = dict(time = day, station = station_id)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample
    where date_trunc('hour', time_stamp) <= date_trunc('hour', $time)
      and date_trunc('hour', time_stamp) > date_trunc('hour', $time) - '1 hour'::interval * 169
      and station_id = $station
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result


def get_168hr_reception(time, station_id):
    max_packets = get_davis_max_wireless_packets(station_id)

    if max_packets is None:
        return None

    params = dict(time=time, station=station_id, maxpackets=max_packets)

    query = """select s.time_stamp::timestamptz,
       round((ds.wind_sample_count / $maxpackets * 100),1)::float as reception,

       s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
       CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
	  true
       else
	  false
       end as gap

    from sample s, davis_sample ds, sample prev
    inner join station st on st.station_id = prev.station_id
    where (s.time_stamp < $time and s.time_stamp > $time - (604800 * '1 second'::interval))
      and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
      and s.station_id = $station
      and prev.station_id = $station
      and ds.sample_id = s.sample_id
    order by s.time_stamp asc
    """

    result = config.db.query(query, params)

    return result


def get_day_indoor_samples_data(day, station_id):
    """
    Gets indoor query data for the specific day.
    :param day: Day to get data for
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Query data
    """

    params = dict(date = day, station=station_id)
    result = config.db.query("""select s.time_stamp::timestamptz,
s.indoor_temperature,
s.indoor_relative_humidity,
           s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
           CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
              true
           else
              false
           end as gap
from sample s, sample prev
inner join station st on st.station_id = prev.station_id
where date(s.time_stamp) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp and station_id = $station)
  and s.station_id = $station
  and prev.station_id = $station
order by s.time_stamp asc""", params)

    return result

def get_7day_indoor_samples_data(day, station_id):
    """
    Gets query data for the 7-day indoor samples data set
    :param day: The day to get samples for
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return:
    """

    params = dict(date = day, station = station_id)
    result = config.db.query("""select s.time_stamp,
                   s.indoor_temperature,
                   s.indoor_relative_humidity,
                   s.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
                   CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                      true
                   else
                      false
                   end as gap
            from sample s, sample prev, station st,
                 (select max(time_stamp) as ts from sample where date(time_stamp) = $date and station_id = $station) as max_ts
            where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
              and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
              and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)
              and s.station_id = $station
              and prev.station_id = $station
              and st.station_id = prev.station_id
            order by s.time_stamp asc
            """, params)

    return result

def get_7day_30mavg_indoor_samples_data(day, station_id):
    """
    Gets 30-minute averaged data for the 7-day period ending on the specified
    date.
    :param day: End date for 7-day period
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Query data
    """

    params = dict(date = day, station=station_id)
    result = config.db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.indoor_temperature) as indoor_temperature,
       avg(indoor_relative_humidity)::integer as indoor_relative_humidity,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap
from (
        select cur.time_stamp,
               --(extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               (extract(epoch from cur.time_stamp::date) + date_part('hour', cur.time_stamp)::int) * 10 + date_part('minute', cur.time_stamp)::int / 30 as quadrant,
               cur.indoor_temperature,
               cur.indoor_relative_humidity,
               cur.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap
        from sample cur, sample prev, station st,
             (select max(time_stamp) as ts from sample where date(time_stamp) = $date and station_id = $station) as max_ts
        where cur.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
          and cur.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp and station_id = $station)
          and cur.station_id = $station
          and prev.station_id = $station
          and st.station_id = cur.station_id
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)

    return result

# Data sources available at the day level.
datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': lambda day, station_id: get_day_dataset(day, get_day_samples_data, outdoor_sample_result_to_json, station_id)
    },
    '7day_samples':{
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_samples_data,outdoor_sample_result_to_json, station_id)
    },
    '7day_30m_avg_samples':{
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_30mavg_samples_data,outdoor_sample_result_to_json, station_id)
    },
    'records': {
        'desc': 'Records for the day.',
        'func': get_day_records
    },
    'rainfall': {
        'desc': 'Rainfall totals for the day and the past seven days.',
        'func': get_day_rainfall
    },
    'hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the day',
        'func': lambda day, station_id: get_day_dataset(day,get_days_hourly_rainfall_data,rainfall_sample_result_to_json, station_id)
    },
    '7day_hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the past seven days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_hourly_rainfall_data,rainfall_sample_result_to_json, station_id)
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': lambda day, station_id: get_day_dataset(day,get_day_indoor_samples_data,indoor_sample_result_to_json, station_id)
    },
    '7day_indoor_samples': {
        'desc': 'Every indoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_indoor_samples_data,indoor_sample_result_to_json, station_id)
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_30mavg_indoor_samples_data,indoor_sample_result_to_json, station_id)
    },
}

# These are all the available datasources ('files') for the day datatable
# route.
datatable_datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': lambda day, station_id: get_day_dataset(day, get_day_samples_data, outdoor_sample_result_to_datatable, station_id)
    },
    '7day_samples': {
        'desc': 'Every outdoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_samples_data,outdoor_sample_result_to_datatable, station_id)
    },
    '7day_30m_avg_samples': {
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_30mavg_samples_data,outdoor_sample_result_to_datatable, station_id)
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': lambda day, station_id: get_day_dataset(day,get_day_indoor_samples_data,indoor_sample_result_to_datatable, station_id)
    },
    '7day_indoor_samples': {
        'desc': 'Every indoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_indoor_samples_data,indoor_sample_result_to_datatable, station_id)
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_30mavg_indoor_samples_data,indoor_sample_result_to_datatable, station_id)
    },
    'hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the day',
        'func': lambda day, station_id: get_day_dataset(day,get_days_hourly_rainfall_data,rainfall_to_datatable, station_id)
    },
    '7day_hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the past seven days.',
        'func': lambda day, station_id: get_day_dataset(day,get_7day_hourly_rainfall_data,rainfall_to_datatable, station_id)
    },
}

def datasource_dispatch(station, datasource_dict, dataset, day):
    """
    Dispatches a request for a data source.
    :param station: Station to use
    :type station: str
    :param datasource_dict: Dict of datasources that are valid at this point.
    :type datasource_dict: dict
    :param dataset: dataset that was requested
    :type dataset: str
    :param day: Day that data was requested for
    :type day: date
    :return: Response data
    :raise: web.NotFound() if request is invalid
    """

    station_id = get_station_id(station)
    if station_id is None:
        raise web.NotFound()

    # Make sure the day actually exists in the database before we go
    # any further.
    if not day_exists(day, station_id):
        raise web.NotFound()

    if dataset in datasource_dict:
        if datasource_dict[dataset]['func'] is None:
            raise web.NotFound()
        return datasource_dict[dataset]['func'](day, station_id)
    else:
        raise web.NotFound()

def json_dispatch(station, dataset, day):
    """
    Dispatches a request for generic JSON data.
    :param station: Station to get data for.
    :param dataset: Data set to get.
    :param day: Day to get data for.
    :return: JSON data.
    """

    return datasource_dispatch(station,
                               datasources,
                               dataset,
                               day)

class data_json:
    """
    Gets data for a particular day in Googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        """
        Gets plain (non-datatable) JSON data sources.
        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param dataset: The dataset (filename) to retrieve
        :type dataset: str
        :return: the JSON dataset requested
        :rtype: str
        :raise: web.NotFound if an invalid request is made.
        """
        this_date = date(int(year),int(month),int(day))
        return json_dispatch(station, dataset,this_date)

def dt_json_dispatch(station, dataset, date):
    """
    Dispatches a Day-level DataTable format JSON data set request.
    :param station: Station to get data for.
    :param dataset: Data set to get
    :param date: Date to get data for.
    :return: JSON data.
    """
    return datasource_dispatch(station,
                               datatable_datasources,
                               dataset,
                               date)

class dt_json:
    """
    Gets data for a particular day in Googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        """
        Handles requests for per-day JSON data sources in Googles datatable
        format.
        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param dataset: The dataset (filename) to retrieve
        :type dataset: str
        :return: JSON Data for whatever dataset was requested.
        :raise: web.NotFound if an invalid request is made.
        """
        this_date = date(int(year),int(month),int(day))
        return dt_json_dispatch(station, dataset, this_date)


class image_old:
    """
    Handles the old URL scheme that includes the local image id. This is just
    to handle any existing bookmarked links, etc.
    """

    def GET(self, station, year, month, day, source, image_id, mode, extension):

        # The URL to get here is:
        # /data/station_code/year/month/day/images/source/image_id/mode.extension
        # We need to redirect to here:
        # /data/station_code/year/month/day/images/source/image_time/type_mode.extension
        # which can be done relatively:
        # ../image_time/type_mode.extension
        #
        # To build this URL we need:
        #  -> Timestamp
        #  -> Image type
        # We have the rest of the details from the existing URL.

        details = get_image_type_and_ts_info(image_id)
        if details is None:
            raise web.NotFound()

        url = "/data/{station}/{year}/{month}/{day}/images/{src}/{h}_{m}_{s}/" \
              "{typ}_{mode}.{ext}".format(
                station=station,
                year=year,
                month=month,
                day=day,
                src=source,
                h=details.time_stamp.hour,
                m=details.time_stamp.minute,
                s=details.time_stamp.second,
                typ=details.code.lower(),
                mode=mode,
                ext=extension
        )

        # Permanent (301) redirect
        raise web.redirect(url)


def trim_cache_directory(directory, max_size):
    """
    Reduces the specified cache directory to the specified size

    :param directory: Directory to trim
    :param max_size: Maximum size (in megabytes) for the cache directory
    """

    if max_size is None:
        return

    # from timeit import default_timer as timer
    # start = timer()

    max_bytes = max_size * 1000 * 1000

    cache_files = [os.path.join(directory, f) for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]

    dir_size = sum(os.path.getsize(f) for f in cache_files)

    if dir_size < max_bytes:
        # Cache is under maximum size. Nothing to do.
        # end = timer()
        # print("Cache is under max size in {0} seconds".format(end - start))
        return

    # Sort the list of files to decide what to delete first.
    if config.cache_expiry_access_time:
        # If the filesystem is mounted noatime this will behave the same as sorting
        # by modified/created time.
        cache_files.sort(key=os.path.getatime)
    else:
        # If the file hasn't been modified mtime should be the same as ctime.
        cache_files.sort(key=os.path.getmtime)

    for f in cache_files:
        f_size = os.path.getsize(f)
        os.remove(f)
        dir_size = dir_size - f_size

        if dir_size < max_bytes:
            break  # Finished downloading files for now.

    # end = timer()
    # print("Completed cache cleanup in {0} seconds".format(end-start))


class image:
    """
    Gets an image
    """

    def GET(self, station, year, month, day, source, image_time, type_mode,
            extension):
        """
        Fetches an image from the database.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param source: Image source code
        :type source: str
        :param image_time: Image time string (eg 15_10_00)
        :type image_time: str
        :param type_mode: image type (eg, cam) and mode - full, thumbnail or metadata
        :type type_mode: str
        :param extension: Image file extension.
        :type extension: str
        :return: The image
        """

        this_date = date(int(year), int(month), int(day))

        type_mode_bits = type_mode.split('_')
        image_type = type_mode_bits[0].lower()
        mode = type_mode_bits[1]

        t = datetime.strptime(image_time, "%H_%M_%S")
        time_stamp = datetime(this_date.year, this_date.month, this_date.day,
                              t.hour, t.minute, t.second)

        image_id = get_image_id(source, image_type, time_stamp)

        if not image_exists(station, this_date, source, image_id):
            raise web.NotFound()

        if mode == "metadata":
            metadata = get_image_metadata(image_id)
            if metadata is None:
                raise web.NotFound()

            result = metadata.metadata

            web.header('Content-Type', 'application/json')
            web.header('Content-Length', str(len(result)))
            web.header('Expires', rfcformat(metadata.time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(metadata.time_stamp))

            return result
        elif mode == "description":

            image = get_image_details(image_id)

            data = {"title": image.title,
                    "description": image.description}

            if data["title"] is None or data["title"] == "":
                data["title"] = time_stamp.time().strftime("%H:%M")

            result = json.dumps(data)

            web.header('Content-Type', 'application/json')
            web.header('Content-Length', str(len(result)))
            web.header('Expires', rfcformat(image.time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(image.time_stamp))

            return result
        elif config.cache_videos and mode == "full" and \
                        extension.lower() == "mp4":
            is_cached = False
            video_data = None
            cache_file = "{0}{1}.{2}".format(
                config.video_cache_directory, image_id, extension
            )
            mime_type = None

            if os.path.isfile(cache_file):
                with open(cache_file, "r+b") as f:
                    video_data = f.read()
                is_cached = True

                mime_type_and_ts = get_image_mime_type(image_id)
                mime_type = mime_type_and_ts.mime_type

            if not is_cached:
                # Not in cache. Load it from the database.
                img = get_image(image_id)
                if img is None or img.image_data is None or img.mime_type is None:
                    raise web.NotFound()
                video_data = img.image_data

                with open(cache_file, 'w+b') as f:
                    f.write(video_data)

                trim_cache_directory(config.video_cache_directory, config.max_video_cache_size)

                mime_type = img.mime_type

            web.header('Content-Type', mime_type)
            web.header('Content-Length', str(len(video_data)))
            web.header('Expires', rfcformat(time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(time_stamp))
            return video_data

        elif mode == "full":
            img = get_image(image_id)
            if img is None or img.image_data is None or img.mime_type is None:
                raise web.NotFound()
            web.header('Content-Type', img.mime_type)
            web.header('Content-Length', str(len(img.image_data)))
            web.header('Expires', rfcformat(img.time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(img.time_stamp))
            return img.image_data
        elif mode == "thumbnail":
            is_cached = False

            thumb_data = None
            time_stamp = None

            cache_file = ""

            mime_type_and_ts = get_image_mime_type(image_id)
            mime_type = mime_type_and_ts.mime_type

            # Videos can't be thumbnailed
            if mime_type.startswith("video/"):
                raise web.NotFound()

            if config.cache_thumbnails:
                time_stamp = mime_type_and_ts.time_stamp

                ext = mimetypes.guess_extension(mime_type)
                if ext == ".jpe":
                    ext = ".jpeg"

                cache_file = "{0}{1}{2}".format(
                    config.cache_directory, image_id, ext
                )

                if os.path.isfile(cache_file):
                    with open(cache_file, "r+b") as f:
                        thumb_data = f.read()
                    is_cached = True

            if not is_cached:
                img_info = get_image(image_id)
                if img_info is None or img_info.image_data is None or img_info.mime_type is None:
                    raise web.NotFound()

                mime_type = img_info.mime_type
                time_stamp = img_info.time_stamp

                # Thumbnail the image. This needs Pillow (or perhaps PIL)
                from io import BytesIO
                from PIL import Image

                original = BytesIO(bytes(img_info.image_data))

                img = Image.open(original)

                img.thumbnail(config.thumbnail_size, Image.ANTIALIAS)

                ext = mimetypes.guess_extension(mime_type)
                if ext == ".jpe":
                    ext = ".jpeg"
                ext = ext[1:]

                out = BytesIO()
                img.save(out, format=ext)
                thumb_data = out.getvalue()
                out.close()
                original.close()

                if config.cache_thumbnails:
                    # Cache the image
                    with open(cache_file, 'w+b') as f:
                        f.write(thumb_data)

                    trim_cache_directory(config.cache_directory, config.max_thumbnail_cache_size)

            web.header('Content-Type', mime_type)
            web.header('Content-Length', str(len(thumb_data)))
            web.header('Expires', rfcformat(time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(time_stamp))
            return thumb_data

        raise web.NotFound()

class data_ascii:
    """
    Gets data for a particular day in plain text (.txt) format
    """
    def GET(self, station, year, month, day, dataset):
        """
        Gets plain (non-datatable) JSON data sources.
        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param dataset: The dataset (filename) to retrieve
        :type dataset: str
        :return: the JSON dataset requested
        :rtype: str
        :raise: web.NotFound if an invalid request is made.
        """
        this_date = date(int(year), int(month), int(day))

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        # Make sure the day actually exists in the database before we go
        # any further.
        if not day_exists(this_date, station_id):
            raise web.NotFound()

        # No plain-text formats currently supported.
        raise web.NotFound()

        # if dataset == "samples":
        #     data, age = get_day_samples_tab_delimited(this_date, station_id)
        #     cache_control_headers(station_id, age, int(year), int(month),
        #                           int(day))
        # else:
        #     raise web.NotFound()

        # web.header('Content-Type', "text/plain")
        # return data


class data_dat:
    """
    Gets data for a particular day in plain text (.dat) format
    """
    def GET(self, station, year, month, day, dataset):
        """
        Gets plain (non-datatable) JSON data sources.
        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param dataset: The dataset (filename) to retrieve
        :type dataset: str
        :return: the JSON dataset requested
        :rtype: str
        :raise: web.NotFound if an invalid request is made.
        """
        this_date = date(int(year), int(month), int(day))

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        # Make sure the day actually exists in the database before we go
        # any further.
        if not day_exists(this_date, station_id):
            raise web.NotFound()

        if dataset == "samples":
            data, age = get_day_samples_tab_delimited(this_date, station_id)
            cache_control_headers(station_id, age, int(year), int(month),
                                  int(day))
        else:
            raise web.NotFound()

        web.header('Content-Type', "text/plain")
        return data


def get_day_samples_tab_delimited(this_date, station_id):
    weather_data = get_day_data_wp(this_date, station_id)

    extra_sensors = get_extra_sensors_enabled(station_id)

    # Davis only columns are:
    #   uv index (Vantage Pro2 Plus only)
    #   solar radiation (Vantage Pro2 Plus only)
    #   reception (wireless stations only)
    #   high_temperature
    #   low_temperature
    #   high_rain_rate
    #   gust_wind_direction
    #   evapotranspiration (Vantage Pro2 Plus only)
    #   high_solar_radiation (Vantage Pro2 Plus only)
    #   high_uv_index (Vantage Pro2 Plus only)
    #   forecast_rule_id
    #   soil_moisture_1, soil_moisture_2, soil_moisture_3, soil_moisture_4
    #   soil_temperature_1, soil_temperature_2, soil_temperature_3, soil_temperature_4
    #   leaf_wetness_1, leaf_wetness_2
    #   extra_humidity_1, extra_humidity_2
    #   extra_temperature_1, extra_tempreature_2, extra_temperature_3

    file_data = '# timestamp\ttemperature\tdew point\tapparent temperature\t' \
                'wind chill\trelative humidity\tabsolute pressure\t' \
                'mean sea level pressure\tindoor temperature\t' \
                'indoor relative humidity\trainfall\taverage wind speed\t' \
                'gust wind speed\twind direction\tuv index\tsolar radiation\t' \
                'reception\thigh_temp\tlow_temp\thigh_rain_rate\t' \
                'gust_direction\tevapotranspiration\thigh_solar_radiation\t' \
                'high_uv_index\tforecast_rule_id'

    if extra_sensors:
        file_data += '\tsoil_moisture_1\tsoil_moisture_2\tsoil_moisture_3\t' \
                     'soil_moisture_4\tsoil_temperature_1\t' \
                     'soil_temperature_2\tsoil_temperature_3\t' \
                     'soil_temperature_4\tleaf_wetness_1\tleaf_wetness_2\t' \
                     'leaf_temperature_1\tleaf_temperature_2\t' \
                     'extra_humidity_1\textra_humidity_2\t' \
                     'extra_temperature_1\textra_temperature_2\t' \
                     'extra_temperature_3'
    file_data += "\n"

    max_ts = None

    format_string = '{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t' \
                    '{11}\t{12}\t{13}\t{14}\t{15}\t{16}\t{17}\t{18}\t{19}\t' \
                    '{20}\t{21}\t{22}\t{23}\t{24}'

    if extra_sensors:
        format_string += "\t{25}\t{26}\t{27}\t{28}\t{29}\t{30}\t{31}\t{32}\t" \
                         "{33}\t{34}\t{35}\t{36}\t{37}\t{38}\t{39}\t{40}\t{41}"

    format_string += "\n"

    def nullable_str(val):
        if val is None:
            return '?'
        return str(val)

    for record in weather_data:

        if max_ts is None:
            max_ts = record.time_stamp

        if record.time_stamp > max_ts:
            max_ts = record.time_stamp

        row = [
            str(record.time),
            nullable_str(record.temperature),
            nullable_str(record.dew_point),
            nullable_str(record.apparent_temperature),
            nullable_str(record.wind_chill),
            nullable_str(record.relative_humidity),
            nullable_str(record.absolute_pressure),
            nullable_str(record.mean_sea_level_pressure),
            nullable_str(record.indoor_temperature),
            nullable_str(record.indoor_relative_humidity),
            nullable_str(record.rainfall),
            nullable_str(record.average_wind_speed),
            nullable_str(record.gust_wind_speed),
            nullable_str(record.wind_direction),
            nullable_str(record.uv_index),
            nullable_str(record.solar_radiation),
            nullable_str(record.reception),
            nullable_str(record.high_temperature),
            nullable_str(record.low_temperature),
            nullable_str(record.high_rain_rate),
            nullable_str(record.gust_wind_direction),
            nullable_str(record.evapotranspiration),
            nullable_str(record.high_solar_radiation),
            nullable_str(record.high_uv_index),
            nullable_str(record.forecast_rule_id)
        ]

        if extra_sensors:
            row = row + [
                nullable_str(record.soil_moisture_1),
                nullable_str(record.soil_moisture_2),
                nullable_str(record.soil_moisture_3),
                nullable_str(record.soil_moisture_4),
                nullable_str(record.soil_temperature_1),
                nullable_str(record.soil_temperature_2),
                nullable_str(record.soil_temperature_3),
                nullable_str(record.soil_temperature_4),
                nullable_str(record.leaf_wetness_1),
                nullable_str(record.leaf_wetness_2),
                nullable_str(record.leaf_temperature_1),
                nullable_str(record.leaf_temperature_2),
                nullable_str(record.extra_humidity_1),
                nullable_str(record.extra_humidity_2),
                nullable_str(record.extra_temperature_1),
                nullable_str(record.extra_temperature_2),
                nullable_str(record.extra_temperature_3),
            ]

        file_data += format_string.format(*row)

    return file_data, max_ts
