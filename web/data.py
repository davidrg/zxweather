"""
Provides access to zxweather data over HTTP in a number of formats. Used for
generating charts in JavaScript, etc.
"""

from datetime import date, datetime
import web
from config import db
import config
import json

__author__ = 'David Goodwin'

# Pretty-print JSON output? (makes debugging easier)
pretty_print = False

class day_datatable_json:
    """
    Gets data for a particular day in googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        if dataset not in ('samples', '7day_samples'):
            raise web.NotFound()

        year = int(year)
        month = int(month)
        day = int(day)

        if dataset == 'samples':
            return get_day_samples_datatable(year,month,day)
        elif dataset == '7day_samples':
            return get_7day_samples_datatable(year,month,day)


def datetime_to_js_date(timestamp):
    """
    Converts a python datetime.datetime or datetime.date object to a
    representation compatbile with googles DataTable structure
    :param timestamp: A timestamp
    :return: datatable representation
    """
    if timestamp is None:
        return "null"
    elif isinstance(timestamp, datetime):
        return "Date({0},{1},{2},{3},{4},{5})".format(timestamp.year,
                                                          timestamp.month - 1,
                                                          timestamp.day,
                                                          timestamp.hour,
                                                          timestamp.minute,
                                                          timestamp.second)
    elif isinstance(timestamp, date):
        return "Date({0},{1},{2})".format(timestamp.year,
                                              timestamp.month - 1, # for JS
                                              timestamp.day)
    else:
        raise TypeError

def sample_result_to_datatable(query_data):
    """
    Converts a query on the sample table to DataTable-compatible JSON.

    Expected columns are time_stamp, temperature, dew_point,
    apparent_temperature, wind_chill, relative_humidity, absolute_pressure.

    :param query_data: Result from the query
    :return: Query data in JSON format.
    """
    cols = [{'id': 'timestamp',
             'label': 'Time Stamp',
             'type': 'datetime'},
            {'id': 'temperature',
             'label': 'Temperature',
             'type': 'number'},
            {'id': 'dewpoint',
             'label': 'Dewpoint',
             'type': 'number'},
            {'id': 'apparenttemp',
             'label': 'Apparent Temperature',
             'type': 'number'},
            {'id': 'windchill',
             'label': 'Wind Chill',
             'type': 'number'},
            {'id': 'humidity',
             'label': 'Humidity',
             'type': 'number'},
            {'id': 'abspressure',
             'label': 'Absolute Pressure',
             'type': 'number'},
    ]

    rows = []

    for record in query_data:

        rows.append({'c': [{'v': datetime_to_js_date(record.time_stamp)},
                {'v': record.temperature},
                {'v': record.dew_point},
                {'v': record.apparent_temperature},
                {'v': record.wind_chill},
                {'v': record.relative_humidity},
                {'v': record.absolute_pressure},
        ]
        })

    data = {'cols': cols,
            'rows': rows}

    def dthandler(obj):
        if isinstance(obj, datetime) or isinstance(obj,date):
            return obj.isoformat()
        else:
            raise TypeError

    if pretty_print:
        return json.dumps(data, default=dthandler, sort_keys=True, indent=4)
    else:
        return json.dumps(data, default=dthandler)

def get_7day_samples_datatable(year,month,day):
    """
    Gets data for a specific day in a Google DataTable compatible format.
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
           s.indoor_temperature,
           s.indoor_relative_humidity
    from sample s,
         (select max(time_stamp) as ts from sample where date(time_stamp) = $date) as max_ts
    where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
      and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
    order by s.time_stamp asc
    """, params)

    web.header('Content-Type','application/json')
    return sample_result_to_datatable(result)

def get_day_samples_datatable(year,month,day):
    """
    Gets data for a specific day in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(year,month,day))
    result = db.query("""select time_stamp::timestamptz,
temperature,
dew_point,
apparent_temperature,
wind_chill,
relative_humidity,
absolute_pressure
from sample
where date(time_stamp) = $date""", params)

    web.header('Content-Type','application/json')
    return sample_result_to_datatable(result)



