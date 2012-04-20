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
pretty_print = True

class day_datatable_json:
    """
    Gets data for a particular day in googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        if dataset not in ('samples'):
            raise web.NotFound()

        if dataset == 'samples':
            return get_day_samples_datatable_json(int(year),int(month),int(day))


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

def get_day_samples_datatable_json(year,month,day):
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

    for record in result:

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

    web.header('Content-Type','application/json')
    if pretty_print:
        return json.dumps(data, default=dthandler, sort_keys=True, indent=4)
    else:
        return json.dumps(data, default=dthandler)



