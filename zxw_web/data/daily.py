# coding=utf-8
"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime
from cache import day_cache_control
from database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp, day_exists
import web
import config
import json
from data.util import outdoor_sample_result_to_json, rainfall_sample_result_to_json, indoor_sample_result_to_datatable, indoor_sample_result_to_json, outdoor_sample_result_to_datatable, rainfall_to_datatable

__author__ = 'David Goodwin'


def get_day_records(day):
    """
    Gets JSON data containing records for the day.
    :param day: The day to get records for
    :type day: date
    :return: The days records in JSON form
    :rtype: str
    """
    records = get_daily_records(day)

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

    day_cache_control(age,day)
    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_day_rainfall(day):
    """
    Gets JSON data containing total rainfall for the day and the past seven
    days.
    :param day:
    """
    rainfall = get_daily_rainfall(day)

    age = get_latest_sample_timestamp()
    json_data = json.dumps(rainfall)
    day_cache_control(age,day)
    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_day_dataset(day, data_function, output_function):
    """
    Gets day-level JSON data using the supplied data function and then
    converts it to JSON using the supplied output function.

    :param day: Day to get data for.
    :param data_function: Function to supply data.
    :param output_function: Function to format JSON output.
    :return: JSON data.
    """

    result = data_function(day)

    data,age = output_function(result)

    day_cache_control(age,day)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_samples_data(day):
    """
    Gets the full days samples.
    :param day:
    :return:
    """
    params = dict(date = day)
    result = config.db.query("""select s.time_stamp::timestamptz,
               s.temperature,
               s.dew_point,
               s.apparent_temperature,
               s.wind_chill,
               s.relative_humidity,
               s.absolute_pressure,
               s.time_stamp - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
               CASE WHEN (s.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
                  true
               else
                  false
               end as gap,
               s.average_wind_speed,
               s.gust_wind_speed
    from sample s, sample prev
    where date(s.time_stamp) = $date
      and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)"""
                             , params)

    return result

def get_7day_samples_data(day):
    """
    Gets samples for the 7-day period ending on the specified date.
    :param day: End date for the 7-day period
    :return: Query data
    """
    params = dict(date = day)
    result = config.db.query("""select s.time_stamp,
           s.temperature,
           s.dew_point,
           s.apparent_temperature,
           s.wind_chill,
           s.relative_humidity,
           s.absolute_pressure,
           s.time_stamp - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
           CASE WHEN (s.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
              true
           else
              false
           end as gap,
           s.average_wind_speed,
           s.gust_wind_speed
    from sample s, sample prev,
         (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
    where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
      and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
      and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)
    order by s.time_stamp asc
    """, params)
    return result

def get_7day_30mavg_samples_data(day):
    """
    Gets 30-minute averaged sample data over the 7-day period ending on the
    specified day.
    :param day: end date.
    :return: Data.
    """
    params = dict(date = day)
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
       avg(iq.gust_wind_speed) as gust_wind_speed
from (
        select cur.time_stamp,
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
                  true
               else
                  false
               end as gap,
               cur.average_wind_speed,
               cur.gust_wind_speed
        from sample cur, sample prev,
             (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
        where cur.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
          and cur.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)
    return result

def get_days_hourly_rainfall_data(day):
    """
    Gets the days hourly rainfall data.
    :param day: Day to get rainfall data for.
    :return: Rainfall data query result
    """

    params = dict(date = day)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample
    where time_stamp::date = $date
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result

def get_7day_hourly_rainfall_data(day):
    """
    Gets hourly rainfall data for the 7 day period ending at the specified date.
    :param day: End of the 7 day period
    :return: Rainfall data query result.
    """

    params = dict(date = day)

    result = config.db.query("""select date_trunc('hour',time_stamp) as time_stamp,
           sum(rainfall) as rainfall
    from sample, (select max(time_stamp) as ts from sample where time_stamp::date = $date) as max_ts
    where time_stamp <= max_ts.ts
      and time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
    group by date_trunc('hour',time_stamp)
    order by date_trunc('hour',time_stamp) asc""", params)

    return result

def get_day_indoor_samples_data(day):
    """
    Gets indoor query data for the specific day.
    :param day: Day to get data for
    :return: Query data
    """

    params = dict(date = day)
    result = config.db.query("""select s.time_stamp::timestamptz,
s.indoor_temperature,
s.indoor_relative_humidity,
           s.time_stamp - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
           CASE WHEN (s.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
              true
           else
              false
           end as gap
from sample s, sample prev
where date(s.time_stamp) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)""", params)

    return result

def get_7day_indoor_samples_data(day):
    """
    Gets query data for the 7-day indoor samples data set
    :param day:
    :return:
    """

    params = dict(date = day)
    result = config.db.query("""select s.time_stamp,
                   s.indoor_temperature,
                   s.indoor_relative_humidity,
                   s.time_stamp - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
                   CASE WHEN (s.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
                      true
                   else
                      false
                   end as gap
            from sample s, sample prev,
                 (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
            where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
              and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
              and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)
            order by s.time_stamp asc
            """, params)

    return result

def get_7day_30mavg_indoor_samples_data(day):
    """
    Gets 30-minute averaged data for the 7-day period ending on the specified
    date.
    :param day: End date for 7-day period
    :return: Query data
    """

    params = dict(date = day)
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
               cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
                  true
               else
                  false
               end as gap
        from sample cur, sample prev,
             (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
        where cur.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
          and cur.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)

    return result

# Data sources available at the day level.
datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': lambda day: get_day_dataset(day, get_day_samples_data, outdoor_sample_result_to_json)
    },
    '7day_samples':{
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day: get_day_dataset(day,get_7day_samples_data,outdoor_sample_result_to_json)
    },
    '7day_30m_avg_samples':{
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day: get_day_dataset(day,get_7day_30mavg_samples_data,outdoor_sample_result_to_json)
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
        'func': lambda day: get_day_dataset(day,get_days_hourly_rainfall_data,rainfall_sample_result_to_json)
    },
    '7day_hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the past seven days.',
        'func': lambda day: get_day_dataset(day,get_7day_hourly_rainfall_data,rainfall_sample_result_to_json)
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': lambda day: get_day_dataset(day,get_day_indoor_samples_data,indoor_sample_result_to_json)
    },
    '7day_indoor_samples': {
        'desc': 'Every indoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day: get_day_dataset(day,get_7day_indoor_samples_data,indoor_sample_result_to_json)
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': lambda day: get_day_dataset(day,get_7day_30mavg_indoor_samples_data,indoor_sample_result_to_json)
    },
}

# These are all the available datasources ('files') for the day datatable
# route.
datatable_datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': lambda day: get_day_dataset(day, get_day_samples_data, outdoor_sample_result_to_datatable)
    },
    '7day_samples': {
        'desc': 'Every outdoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day: get_day_dataset(day,get_7day_samples_data,outdoor_sample_result_to_datatable)
    },
    '7day_30m_avg_samples': {
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': lambda day: get_day_dataset(day,get_7day_30mavg_samples_data,outdoor_sample_result_to_datatable)
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': lambda day: get_day_dataset(day,get_day_indoor_samples_data,indoor_sample_result_to_datatable)
    },
    '7day_indoor_samples': {
        'desc': 'Every indoor sample over the past seven days. Should be around 2016 records.',
        'func': lambda day: get_day_dataset(day,get_7day_indoor_samples_data,indoor_sample_result_to_datatable)
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': lambda day: get_day_dataset(day,get_7day_30mavg_indoor_samples_data,indoor_sample_result_to_datatable)
    },
    'hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the day',
        'func': lambda day: get_day_dataset(day,get_days_hourly_rainfall_data,rainfall_to_datatable)
    },
    '7day_hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the past seven days.',
        'func': lambda day: get_day_dataset(day,get_7day_hourly_rainfall_data,rainfall_to_datatable)
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
    if station != config.default_station_name:
        raise web.NotFound()
    print dataset
    # Make sure the day actually exists in the database before we go
    # any further.
    if not day_exists(day):
        raise web.NotFound()

    if dataset in datasource_dict:
        if datasource_dict[dataset]['func'] is None:
            raise web.NotFound()
        return datasource_dict[dataset]['func'](day)
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