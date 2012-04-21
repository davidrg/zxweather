"""
Provides access to zxweather monthly data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime, timedelta
import json
import os
import web
from web.contrib.template import render_jinja
from config import db
import config
from data.util import rfcformat, outdoor_sample_result_to_datatable, datetime_to_js_date, pretty_print

__author__ = 'David Goodwin'

### Data URLs:
# This file provides URLs to access raw data in json format.
#
# /data/{year}/{month}/datatable/samples.json
#       All samples for the month.
# /data/{year}/{month}/datatable/30m_avg_samples.json
#       30 minute averages for the month.
# /data/{year}/{month}/datatable/daily_records.json
#       daily records for the month.<b>

# TODO: handle gaps of an entire day in the daily records output

class datatable_json:
    """
    Gets data for a particular month in Googles DataTable format.
    """

    def GET(self, station, year, month, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)
        month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year),int(month),1))
        recs = db.query("select 42 from sample where date(date_trunc('month',time_stamp)) = $date  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_month_samples_datatable(year,month)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_datatable(year,month)
        elif dataset == 'daily_records':
            return get_daily_records(year,month)
        else:
            raise web.NotFound()

class index:
    def GET(self, station, year, month):
        template_dir = os.path.join(os.path.dirname(__file__),
                                         os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp,
           extract(day from time_stamp) as day_stamp
    from sample
    group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
    order by day_stamp""", dict(year=year, month=month))

        days = []

        for day in month_data:
            day = int(day.day_stamp)
            days.append(day)

        if not len(days):
            raise web.NotFound()

        return render.monthly_data_index(days=days)

def monthly_cache_control_headers(year,month,age):
    """
    Sets cache control headers for a daily data files.
    :param year: Year of the data file
    :type year: int
    :param month: Month of the data file
    :type month: int
    :param age: Timestamp of the last record in the data file
    :type age: datetime
    """

    now = datetime.now()
    # HTTP-1.1 Cache Control
    if year == now.year and month == now.month:
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

def get_daily_records(year,month):
    params = dict(date = date(year,month,01))
    query_data = db.query("""select date_trunc('day', time_stamp)::date as time_stamp,
    max(temperature) as max_temp,
    min(temperature) as min_temp,
    max(relative_humidity) as max_humid,
    min(relative_humidity) as min_humid,
    max(absolute_pressure) as max_pressure,
    min(absolute_pressure) as min_pressure,
    sum(rainfall) as total_rainfall
from sample
where date_trunc('month', time_stamp) = date_trunc('month', $date)
group by date_trunc('day', time_stamp)
order by time_stamp asc""", params)

    cols = [{'id': 'timestamp',
             'label': 'Time Stamp',
             'type': 'date'},
            {'id': 'max_temp',
             'label': 'Maximum Temperature',
             'type': 'number'},
            {'id': 'min_temp',
             'label': 'Minimum Temperature',
             'type': 'number'},
            {'id': 'max_humid',
             'label': 'Maximum Relative Humidity',
             'type': 'number'},
            {'id': 'min_humid',
             'label': 'Minimum Relative Humidity',
             'type': 'number'},
            {'id': 'max_pressure',
             'label': 'Maximum Absolute Pressure',
             'type': 'number'},
            {'id': 'min_pressure',
             'label': 'MinimumAbsolute Pressure',
             'type': 'number'},
            {'id': 'total_rainfall',
             'label': 'Total Rainfall',
             'type': 'number'},
    ]

    rows = []

    # At the end of the following loop, this will contain the timestamp for
    # the most recent record in this data set.
    data_age = None

    for record in query_data:

#        # Handle gaps in the dataset
#        if record.gap:
#            rows.append({'c': [{'v': datetime_to_js_date(record.prev_sample_time)},
#                    {'v': None},
#                    {'v': None},
#                    {'v': None},
#                    {'v': None},
#                    {'v': None},
#                    {'v': None},
#            ]
#            })

        rows.append({'c': [{'v': datetime_to_js_date(record.time_stamp)},
                {'v': record.max_temp},
                {'v': record.min_temp},
                {'v': record.max_humid},
                {'v': record.min_humid},
                {'v': record.max_pressure},
                {'v': record.min_pressure},
                {'v': record.total_rainfall},
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        page = json.dumps(data, sort_keys=True, indent=4)
    else:
        page = json.dumps(data)

    monthly_cache_control_headers(year,month,data_age)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(page)))

    return page


def get_30m_avg_month_samples_datatable(year, month):

    params = dict(date = date(year,month,01))
    query_data = db.query("""select min(iq.time_stamp) as time_stamp,
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
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',now()))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
where date_trunc('month',iq.time_stamp) = date_trunc('month', now())
group by iq.quadrant
order by iq.quadrant asc""", params)

    data, data_age = outdoor_sample_result_to_datatable(query_data)

    monthly_cache_control_headers(year,month,data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_month_samples_datatable(year,month):

    params = dict(date = date(year,month,01))
    query_data = db.query("""select cur.time_stamp,
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
where date(date_trunc('month',cur.time_stamp)) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
order by cur.time_stamp asc""", params)

    data, data_age = outdoor_sample_result_to_datatable(query_data)

    monthly_cache_control_headers(year,month,data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data
