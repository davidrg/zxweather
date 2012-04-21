"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime, timedelta
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

# TODO: round temperatures, etc.

class datatable_json:
    """
    Gets data for a particular day in Googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)
        month = int(month)
        day = int(day)

        # Make sure the day actually exists in the database before we go
        # any further.
        params = dict(date=date(year,month,day))
        recs = db.query("select 42 from sample where date(time_stamp) = $date limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_day_samples_datatable(year,month,day)
        elif dataset == '7day_samples':
            return get_7day_samples_datatable(year,month,day)
        elif dataset == '7day_30m_avg_samples':
            return get_7day_30mavg_samples_datatable(year,month,day)
        elif dataset == 'indoor_samples':
            return get_day_indoor_samples_datatable(year,month,day)
        elif dataset == '7day_indoor_samples':
            return get_7day_indoor_samples_datatable(year,month,day)
        elif dataset == '7day_30m_avg_indoor_samples':
            return get_7day_30mavg_indoor_samples_datatable(year,month,day)
        else:
            raise web.NotFound()

class index:
    def GET(self, station, year, month, day):
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Make sure the day actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year),int(month),int(day)))
        recs = db.query("select 42 from sample where date(time_stamp) = $date limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        return render.daily_data_index()

def daily_cache_control_headers(year,month,day,age):
    """
    Sets cache control headers for a daily data files.
    :param year: Year of the data file
    :type year: int
    :param month: Month of the data file
    :type month: int
    :param day: Day of the data file
    :type day: int
    :param age: Timestamp of the last record in the data file
    :type age: datetime
    """

    now = datetime.now()
    # HTTP-1.1 Cache Control
    if year == now.year and month == now.month and day == now.day:
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
                    {'v': None},
                    {'v': None},
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

    def dthandler(obj):
        if isinstance(obj, datetime) or isinstance(obj,date):
            return obj.isoformat()
        else:
            raise TypeError

    if pretty_print:
        return json.dumps(data, default=dthandler, sort_keys=True, indent=4), data_age
    else:
        return json.dumps(data, default=dthandler), data_age

def get_7day_indoor_samples_datatable(year,month,day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
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

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_30mavg_indoor_samples_datatable(year,month,day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
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
        from sample cur, sample prev
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',$date))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
where date(iq.time_stamp) > (date($date) - '7 days'::interval)
group by iq.quadrant
order by iq.quadrant asc""", params)

    data,age = indoor_sample_result_to_datatable(result)

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_indoor_samples_datatable(year,month,day):
    """
    Gets indoor data for a specific day in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
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

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_samples_datatable(year,month,day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
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

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_7day_30mavg_samples_datatable(year, month, day):
    """
    Gets data for a 7-day period in a Google DataTable compatible format.
    Data is averaged hourly to reduce the sample count.
    :return:
    """
    params = dict(date = date(year,month,day))
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
        from sample cur, sample prev
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',$date))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
where date(iq.time_stamp) > (date($date) - '7 days'::interval)
group by iq.quadrant
order by iq.quadrant asc""", params)

    # 1-hour average query:
    #    """select iq.time_stamp,
    #       avg(iq.temperature) as temperature,
    #       avg(iq.dew_point) as dew_point,
    #       avg(iq.apparent_temperature) as apparent_temperature,
    #       avg(wind_chill) as wind_chill,
    #       avg(relative_humidity)::integer as relative_humidity,
    #       avg(absolute_pressure) as absolute_pressure,
    #       min(prev_sample_time) as prev_sample_time,
    #       bool_or(gap) as gap
    #from (
    #        select date_trunc('hour',cur.time_stamp) as time_stamp,
    #               cur.temperature,
    #               cur.dew_point,
    #               cur.apparent_temperature,
    #               cur.wind_chill,
    #               cur.relative_humidity,
    #               cur.absolute_pressure,
    #               cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
    #               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
    #                  true
    #               else
    #                  false
    #               end as gap
    #        from sample cur, sample prev
    #        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',$date))
    #          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
    #        order by cur.time_stamp asc) as iq
    #where date(iq.time_stamp) >= (date($date) - '7 days'::interval)
    #group by iq.time_stamp
    #order by iq.time_stamp asc
    #    """

    data,age = outdoor_sample_result_to_datatable(result)

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_day_samples_datatable(year,month,day):
    """
    Gets data for a specific day in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
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

    daily_cache_control_headers(year,month,day,age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

