"""
Provides access to zxweather yearly data over HTTP in a number of formats.
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
# /data/{year}/datatable/daily_records.json
#       daily records for the year.

# TODO: round temperatures, etc.

class datatable_json:
    """
    Gets data for a particular month in Googles DataTable format.
    """

    def GET(self, station, year, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)

        # Make sure the year actually exists in the database before we go
        # any further.
        params = dict(year=year)
        recs = db.query("select 42 from sample where extract(year from time_stamp) = $year  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'daily_records':
            return get_daily_records(year)
        else:
            raise web.NotFound()

class index:
    """ Year level data sources"""
    def GET(self, station, year):
        """
        Gets a list of data sources available at the year level.
        :param station: Station to get data for
        :type station: string
        :param year: Year to get data for
        :type year: string
        :return: html view data.
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    group by year_stamp, month_stamp) as md where md.year_stamp = $year""", dict(year=year))

        months = []

        for record in month_data:
            months.append(record.month_stamp)

        if not len(months):
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.yearly_data_index(months=months)

def yearly_cache_control_headers(year,age):
    """
    Sets cache control headers for a daily data files.
    :param year: Year of the data file
    :type year: int
    :param age: Timestamp of the last record in the data file
    :type age: datetime
    """

    now = datetime.now()
    # HTTP-1.1 Cache Control
    if year == now.year:
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

def get_daily_records(year):
    params = dict(date = date(year,01,01))
    query_data = db.query("""select date_trunc('day', time_stamp)::date as time_stamp,
    max(temperature) as max_temp,
    min(temperature) as min_temp,
    max(relative_humidity) as max_humid,
    min(relative_humidity) as min_humid,
    max(absolute_pressure) as max_pressure,
    min(absolute_pressure) as min_pressure,
    sum(rainfall) as total_rainfall
from sample
where date_trunc('year',time_stamp) = date_trunc('year', $date)
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

    yearly_cache_control_headers(year,data_age)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(page)))

    return page