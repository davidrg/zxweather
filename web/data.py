"""
Provides access to zxweather data over HTTP in a number of formats. Used for
generating charts in JavaScript, etc.
"""

from datetime import date, datetime
import web
from config import db
import json

__author__ = 'David Goodwin'

class day_data:
    def GET(self):
        return get_day_data()

def get_day_data():
    """
    Gets data for a specific day in a Google DataTable compatible format.
    :return:
    """
    params = dict(date = date(2012,04,19))
    result = db.query("""select time_stamp::timestamptz, temperature
    from sample where date(time_stamp) = $date""", params)

    cols = [{'id': 'timestamp',
             'label': 'Time Stamp',
             'type': 'timestamp'},
            {'id': 'temperature',
             'label': 'Temperature',
             'type': 'number'}]

    rows = []

    for record in result:

        rows.append({'c': [{'v': str(record.time_stamp)},
                           {'v': record.temperature}
                          ]
                    })

    data = {'cols': cols,
            'rows': rows}

    web.header('Content-Type','application/json')
    return json.dumps(data, sort_keys=True, indent=4)



