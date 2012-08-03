# coding=utf-8
"""
Google DataTable data sources at the day level.
"""
from data.daily import datasource_dispatch, get_day_samples_data, get_7day_30mavg_samples_data, get_days_hourly_rainfall_data, get_7day_hourly_rainfall_data

__author__ = 'David Goodwin'

from datetime import date
from cache import day_cache_control
import web
from config import db
import json
from data.util import datetime_to_js_date, outdoor_sample_result_to_datatable, pretty_print


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

    day_cache_control(age,day)

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

    day_cache_control(age,day)

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

    day_cache_control(age,day)

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

    data,age = outdoor_sample_result_to_datatable(result)

    day_cache_control(age,day)

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
    result = get_7day_30mavg_samples_data(day)

    data,age = outdoor_sample_result_to_datatable(result)

    day_cache_control(age,day)

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


    data,age = outdoor_sample_result_to_datatable(get_day_samples_data(day))

    day_cache_control(age,day)

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

def get_days_hourly_rain_datatable(day):
    """
    Gets total rainfall for each hour during the specified day.
    :param day: Date to get data for
    :type day: datetime.date
    """

    result = get_days_hourly_rainfall_data(day)

    json_data, age = rainfall_to_datatable(result)

    day_cache_control(age,day)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_7day_hourly_rain_datatable(day):
    """
    Gets total rainfall for each hour during the past 7 days.
    :param day: Date to get data for
    :type day: datetime.date
    """

    result = get_7day_hourly_rainfall_data(day)

    json_data, age = rainfall_to_datatable(result)

    day_cache_control(age,day)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

# These are all the available datasources ('files') for the day datatable
# route.
datatable_datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': get_day_samples_datatable
    },
    '7day_samples': {
        'desc': 'Every outdoor sample over the past seven days. Should be around 2016 records.',
        'func': get_7day_samples_datatable
    },
    '7day_30m_avg_samples': {
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': get_7day_30mavg_samples_datatable
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': get_day_indoor_samples_datatable
    },
    '7day_indoor_samples': {
        'desc': 'Every indoor sample over the past seven days. Should be around 2016 records.',
        'func': get_7day_indoor_samples_datatable
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': get_7day_30mavg_indoor_samples_datatable
    },
    'hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the day',
        'func': get_days_hourly_rain_datatable
    },
    '7day_hourly_rainfall': {
        'desc': 'Total rainfall for each hour in the past seven days.',
        'func': get_7day_hourly_rain_datatable
    },
}


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

        return datasource_dispatch(station,
                                   datatable_datasources,
                                   dataset,
                                   this_date)