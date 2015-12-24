# coding=utf-8
"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""
import mimetypes
import os
from datetime import date, datetime, timedelta

from cache import day_cache_control, rfcformat
from database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp, day_exists, get_station_id, \
    get_davis_max_wireless_packets, image_exists, get_image_metadata, get_image, \
    get_image_mime_type
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
    age = records.max_gust_wind_speed_ts
    if records.max_average_wind_speed_ts > age: age = records.max_average_wind_speed_ts
    if records.min_absolute_pressure_ts > age: age = records.min_absolute_pressure_ts
    if records.max_absolute_pressure_ts > age: age = records.max_absolute_pressure_ts
    if records.min_apparent_temperature_ts > age: age = records.min_apparent_temperature_ts
    if records.max_apparent_temperature_ts > age: age = records.max_apparent_temperature_ts
    if records.min_wind_chill_ts > age: age = records.min_wind_chill_ts
    if records.max_wind_chill_ts > age: age = records.max_wind_chill_ts
    if records.min_dew_point_ts > age: age = records.min_dew_point_ts
    if records.max_dew_point_ts > age: age = records.max_dew_point_ts
    if records.min_temperature_ts > age: age = records.min_temperature_ts
    if records.max_temperature_ts > age: age = records.max_temperature_ts
    if records.min_humidity_ts > age: age = records.min_humidity_ts
    if records.max_humidity_ts > age: age = records.max_humidity_ts

    age = datetime.combine(day,age)

    json_data = json.dumps(data)

    day_cache_control(age,day, station_id)
    web.header('Content-Type','application/json')
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
    params = dict(date = day, station=station_id)
    result = config.db.query("""select s.time_stamp::timestamptz,
               s.temperature,
               s.dew_point,
               s.apparent_temperature,
               s.wind_chill,
               s.relative_humidity,
               s.absolute_pressure,
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
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       max(iq.gust_wind_speed) as gust_wind_speed,
       avg(iq.uv_index)::real as uv_index,
       avg(iq.solar_radiation)::real as solar_radiation
from (
        select cur.time_stamp,
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
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
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       max(iq.gust_wind_speed) as gust_wind_speed,
       avg(iq.uv_index)::real as uv_index,
       avg(iq.solar_radiation)::real as solar_radiation
from (
        select cur.time_stamp,
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
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
      and date_trunc('hour', time_stamp) > date_trunc('hour', $time - '1 hour'::interval * 25)
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
    where time_stamp <= $time
      and time_stamp >= ($time - (604800 * '1 second'::interval))
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
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
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

class image:
    """
    Gets an image
    """

    def GET(self, station, year, month, day, source, image_id, mode, extension):
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
        :param image_id: Image id
        :type image_id: int
        :param mode: image mode - full, thumbnail or metadata
        :type mode: str
        :param extension: Image file extension.
        :type extension: str
        :return: The image
        """

        this_date = date(int(year), int(month), int(day))

        image_id = int(image_id)

        if not image_exists(station, this_date, source, image_id):
            raise web.NotFound()

        if mode == "metadata":
            metadata = get_image_metadata(image_id)
            if metadata is None:
                raise web.NotFound()

            result = json.dumps(metadata.metadata)

            web.header('Content-Type', 'application/json')
            web.header('Content-Length', str(len(result)))
            web.header('Expires', rfcformat(metadata.time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(metadata.time_stamp))

            return result
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
            mime_type = None
            time_stamp = None

            cache_file = ""

            if config.cache_thumbnails:
                mime_type_and_ts = get_image_mime_type(image_id)
                mime_type = mime_type_and_ts.mime_type
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

                original = BytesIO(img_info.image_data)

                img = Image.open(original)

                print(config.thumbnail_size)

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

            web.header('Content-Type', mime_type)
            web.header('Content-Length', str(len(thumb_data)))
            web.header('Expires', rfcformat(time_stamp +
                                            timedelta(days=30)))
            web.header('Last-Modified', rfcformat(time_stamp))
            return thumb_data

        raise web.NotFound()
