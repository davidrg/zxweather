"""
Utility functions for handling data URLs
"""

from datetime import date, datetime
import json
from time import time

__author__ = 'David Goodwin'

pretty_print = False

def datetime_to_js_date(timestamp):
    """
    Converts a python datetime.datetime or datetime.date object to a
    representation compatible with Googles DataTable structure
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
    elif isinstance(timestamp, time):
        return [timestamp.hour, timestamp.minute, timestamp.second]
    else:
        raise TypeError


def outdoor_sample_result_to_datatable(query_data):
    """
    Converts a query on the sample table to DataTable-compatible JSON.

    Expected columns are time_stamp, temperature, dew_point,
    apparent_temperature, wind_chill, relative_humidity, absolute_pressure,
    prev_sample_time, gap.

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
                {'v': record.temperature},
                {'v': record.dew_point},
                {'v': record.apparent_temperature},
                {'v': record.wind_chill},
                {'v': record.relative_humidity},
                {'v': record.absolute_pressure},
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        return json.dumps(data, sort_keys=True, indent=4), data_age
    else:
        return json.dumps(data), data_age