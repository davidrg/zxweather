"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime, timedelta
from data.database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp
import os
import web
from web.contrib.template import render_jinja
from config import db
import config
import json
from data.util import rfcformat, datetime_to_js_date, outdoor_sample_result_to_datatable, pretty_print

__author__ = 'David Goodwin'

### Data URLs:
# This file provides URLs to access raw data in json format.
#
# /data/{year}/{month}/{day}/datatable/samples.json
#       All samples for the day.
# /data/{year}/{month}/{day}/datatable/7day_samples.json
#       All samples for the past 7 days.
# /data/{year}/{month}/{day}/datatable/7day_30m_avg_samples.json
#       30 minute averages for the past 7 days.
# /data/{year}/{month}/{day}/datatable/indoor_samples.json
#       All indoor samples.
# /data/{year}/{month}/{day}/datatable/7day_indoor_samples.json
#       All samples for the past 7 days of indoor weather.
# /data/{year}/{month}/{day}/datatable/7day_30m_avg_indoor_samples.json
#       30 minute averages for the past 7 days of indoor weather.

class datatable_json:
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
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)
        month = int(month)
        day = int(day)

        this_date = date(year,month,day)

        # Make sure the day actually exists in the database before we go
        # any further.
        params = dict(date=this_date)
        recs = db.query("select 42 from sample where date(time_stamp) = $date limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_day_samples_datatable(this_date)
        elif dataset == '7day_samples':
            return get_7day_samples_datatable(this_date)
        elif dataset == '7day_30m_avg_samples':
            return get_7day_30mavg_samples_datatable(this_date)
        elif dataset == 'indoor_samples':
            return get_day_indoor_samples_datatable(this_date)
        elif dataset == '7day_indoor_samples':
            return get_7day_indoor_samples_datatable(this_date)
        elif dataset == '7day_30m_avg_indoor_samples':
            return get_7day_30mavg_indoor_samples_datatable(this_date)
        elif dataset == 'hourly_rainfall':
            return get_days_hourly_rain_datatable(this_date)
        elif dataset == '7day_hourly_rainfall':
            return get_7day_hourly_rain_datatable(this_date)
        else:
            raise web.NotFound()

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
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)
        month = int(month)
        day = int(day)

        this_date = date(year,month,day)

        # Make sure the day actually exists in the database before we go
        # any further.
        params = dict(date=this_date)
        recs = db.query("select 42 from sample where date(time_stamp) = $date limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()


        if dataset == 'records':
            return get_day_records(this_date)
        if dataset == 'rainfall':
            return get_day_rainfall(this_date)
        else:
            raise web.NotFound()

class index:
    """
    Provides an index of available daily data sources
    """
    def GET(self, station, year, month, day):
        """
        Returns an index page containing a list of json files available for
        the day.
        :param station: Station to get data for
        :type station: string
        :param year: Year to get data for
        :type year: string
        :param month: Month to get data for
        :type month: string
        :param day: Day to get data for.
        :type day: string
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Make sure the day actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year),int(month),int(day)))
        recs = db.query("select 42 from sample where date(time_stamp) = $date limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.daily_data_index()

def daily_cache_control_headers(day,age):
    """
    Sets cache control headers for a daily data files.
    :param day: Day of the data file
    :type day: date
    :param age: Timestamp of the last record in the data file
    :type age: datetime
    """

    now = datetime.now()
    # HTTP-1.1 Cache Control
    if day.year == now.year and day.month == now.month and day.day == now.day:
        # We should be getting a new sample every sample_interval seconds if
        # the requested day is today.
        web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
        web.header('Expires',
                   rfcformat(age + timedelta(0, config.sample_interval)))
    else:
        # Old data. Never expires.
        web.header('Expires',
                   rfcformat(now + timedelta(60, 0))) # Age + 60 days
    web.header('Last-Modified', rfcformat(age))

def indoor_sample_result_to_datatable(query_data):
    """
    Converts a query on the sample table to DataTable-compatible JSON.

    Expected columns are time_stamp, indoor_temperature,
    indoor_relative_humidity, prev_sample_time, gap.

    :param query_data: Result from the query
    :return: Query data in JSON format.
    """
    cols = [{'id': 'timestamp',
             'label': 'Time Stamp',
             'type': 'datetime'},
            {'id': 'temperature',
             'label': 'Indoor Temperature',
             'type': 'number'},
            {'id': 'humidity',
             'label': 'Indoor Humidity',
             'type': 'number'},
    ]

    rows = []

    # At the end of the following loop, this will contain the timestamp for
    # the most recent record in this data set.
    data_age = None

    for record in query_data:


        # Handle gaps in the dataset
        if record.gap:
            rows.append({'c': [{'v': datetime_to_js_date(record.prev_sample_time)},
                    {'v': None},
                    {'v': None},
            ]
            })

        rows.append({'c': [{'v': datetime_to_js_date(record.time_stamp)},
                {'v': record.indoor_temperature},
                {'v': record.indoor_relative_humidity},
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        return json.dumps(data, sort_keys=True, indent=4), data_age
    else:
        return json.dumps(data), data_age

def get_7day_indoor_samples_datatable(day):
    """
    :param day: Day to get data for
    :type day: date
    :return: JSON data in Googles datatable format containing indoor samples
    for the past seven days.
    """
    params = dict(date = day)
    result = db.query("""select s.time_stamp,
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

    data,age = indoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_30mavg_indoor_samples_datatable(day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    :param day: Day to get data for
    :type day: date
    :return: JSON in Googles Datatable format containing 30 minute averaged
    data for the past seven days of indoor samples.
    """
    params = dict(date = day)
    result = db.query("""select min(iq.time_stamp) as time_stamp,
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

    data,age = indoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_indoor_samples_datatable(day):
    """
    Gets indoor data for a specific day in a Google DataTable compatible format.
    :param day: Day to get data for
    :type day: date
    :return: JSON data (in googles datatable format) containing indoor samples
             for the day
    :rtype: str
    """
    params = dict(date = day)
    result = db.query("""select s.time_stamp::timestamptz,
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

    data,age = indoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_samples_datatable(day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    :param day: Day to get data for
    :type day: date
    :return: JSON data containing the past seven days of samples
    :rtype: str
    """
    params = dict(date = day)
    result = db.query("""select s.time_stamp,
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
           end as gap
    from sample s, sample prev,
         (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
    where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
      and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
      and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)
    order by s.time_stamp asc
    """, params)

    data,age = outdoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_30mavg_samples_datatable(day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    Data is averaged hourly to reduce the sample count.
    :param day: Day to get data for
    :type day: date
    :return: JSON data containing 30 minute averaged sample data for the past
             seven days
    :rtype: str
    """
    params = dict(date = day)
    result = db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap
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
               end as gap
        from sample cur, sample prev,
             (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
        where cur.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
          and cur.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
group by iq.quadrant
order by iq.quadrant asc""", params)

    data,age = outdoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_samples_datatable(day):
    """
    Gets data for a specific day in a Google DataTable compatible format.
    :param day: Day to get data for
    :type day: date
    :return: JSON data containing samples for the day.
    :rtype: str
    """
    params = dict(date = day)
    result = db.query("""select s.time_stamp::timestamptz,
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
           end as gap
from sample s, sample prev
where date(s.time_stamp) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < s.time_stamp)""", params)

    data,age = outdoor_sample_result_to_datatable(result)

    daily_cache_control_headers(day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def rainfall_to_datatable(query_result):
    """
    Converts a rainfall data query result into Googles DataTable JSON format.
    :param query_result: Result of the rainfall query.
    :return: JSON data.
    """
    cols = [{'id': 'timestamp',
             'label': 'Time Stamp',
             'type': 'datetime'},
            {'id': 'rainfall',
             'label': 'Rainfall',
             'type': 'number'},
    ]

    rows = []

    # At the end of the following loop, this will contain the timestamp for
    # the most recent record in this data set.
    data_age = None

    for record in query_result:

        rows.append({'c': [{'v': datetime_to_js_date(record.time_stamp)},
                {'v': record.rainfall},
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        return json.dumps(data, sort_keys=True, indent=4), data_age
    else:
        return json.dumps(data), data_age

def get_days_hourly_rain_datatable(data_date):
    """
    Gets total rainfall for each hour during the specified day.
    :param data_date: Date to get data for
    :type data_date: datetime.date
    """

    params = dict(date = data_date)

    result = db.query("""select date_trunc('hour',time_stamp) as time_stamp,
       sum(rainfall) as rainfall
from sample
where time_stamp::date = $date
group by date_trunc('hour',time_stamp)
order by date_trunc('hour',time_stamp) asc""", params)

    json_data, data_age = rainfall_to_datatable(result)

    daily_cache_control_headers(data_date, data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_7day_hourly_rain_datatable(data_date):
    """
    Gets total rainfall for each hour during the past 7 days.
    :param data_date: Date to get data for
    :type data_date: datetime.date
    """

    params = dict(date = data_date)

    result = db.query("""select date_trunc('hour',time_stamp) as time_stamp,
       sum(rainfall) as rainfall
from sample, (select max(time_stamp) as ts from sample where time_stamp::date = $date) as max_ts
where time_stamp <= max_ts.ts
  and time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
group by date_trunc('hour',time_stamp)
order by date_trunc('hour',time_stamp) asc""", params)

    json_data, data_age = rainfall_to_datatable(result)

    daily_cache_control_headers(data_date, data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

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

    daily_cache_control_headers(day, age)
    return json.dumps(data)

def get_day_rainfall(day):
    """
    Gets JSON data containing total rainfall for the day and the past seven
    days.
    :param day:
    """
    rainfall = get_daily_rainfall(day)

    age = get_latest_sample_timestamp()
    daily_cache_control_headers(day,age)
    return json.dumps(rainfall)