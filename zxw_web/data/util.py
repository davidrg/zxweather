# coding=utf-8
"""
Utility functions for handling data URLs
"""

from datetime import date, datetime, time
import json


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
            {'id': 'avgws',
             'label': 'Average Wind Speed',
             'type': 'number'},
            {'id': 'gws',
             'label': 'Gust Wind Speed',
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
                {'v': record.average_wind_speed},
                {'v': record.gust_wind_speed},
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        return json.dumps(data, sort_keys=True, indent=4), data_age
    else:
        return json.dumps(data), data_age

def outdoor_sample_result_to_json(query_data):
    """
    Converts the supplied outdoor sample query data to JSON format.
    :param query_data: Data to convert.
    :return: json_data, data_age
    """

    labels = ["Time",
              "Temperature",
              "Dew Point",
              "Apparent Temperature",
              "Wind Chill",
              "Humidity",
              "Pressure",
              "Average Wind Speed",
              "Gust Wind Speed",
              ]

    data_age = None
    data_set = []

    for record in query_data:
        if record.gap:
            # Insert a gap
            data_set.append([
                record.time_stamp.isoformat(),
                None, None, None, None, None, None, None, None
            ])

        data_set.append(
            [
                record.time_stamp.isoformat(),
                record.temperature,
                record.dew_point,
                record.apparent_temperature,
                record.wind_chill,
                record.relative_humidity,
                record.absolute_pressure,
                record.average_wind_speed,
                record.gust_wind_speed
            ]
        )

        data_age = record.time_stamp



    result = {
        'data': data_set,
        'labels': labels
    }

    json_data = json.dumps(result)

    return json_data, data_age

def rainfall_sample_result_to_json(query_data):
    """
    Converts the supplied rainfall sample query data to JSON format.
    :param query_data: Data to convert.
    :return: json_data, data_age
    """

    labels = ["Time",
              "Rainfall",
              ]

    data_age = None
    data_set = []

    for record in query_data:
#        if record.gap:
#            # Insert a gap
#            data_set.append([])

        data_set.append(
            [
                record.time_stamp.isoformat(),
                record.rainfall,
            ]
        )

        data_age = record.time_stamp

    result = {
        'data': data_set,
        'labels': labels
    }

    json_data = json.dumps(result)

    return json_data, data_age

def daily_records_result_to_datatable(query_data):
    """
    Converts daily records query data into DataTable JSON format for the
    Google Visualisation API.
    :param query_data: Records query data.
    :return: JSON data, data age
    """

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
             'label': 'Minimum Absolute Pressure',
             'type': 'number'},
            {'id': 'total_rainfall',
             'label': 'Total Rainfall',
             'type': 'number'},
            {'id': 'max_average_wind_speed',
             'label': 'Maximum Average Wind Speed',
             'type': 'number'},
            {'id': 'max_gust_wind_speed',
             'label': 'Maximum Gust Wind Speed',
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
                {'v': record.max_average_wind_speed},
                {'v': record.max_gust_wind_speed}
        ]
        })

        data_age = record.time_stamp

    data = {'cols': cols,
            'rows': rows}

    if pretty_print:
        page = json.dumps(data, sort_keys=True, indent=4)
    else:
        page = json.dumps(data)

    return page, data_age

def daily_records_result_to_json(query_data):
    """
    Converts daily records query data to a generic JSON format.
    :param query_data: Query data to convert.
    :return: JSON data, data age
    """

    labels = [
        "Date",
        "Maximum Temperature",
        "Minimum Temperature",
        "Maximum Humidity",
        "Minimum Humidity",
        "Maximum Pressure",
        "Minimum Pressure",
        "Rainfall",
        "Maximum Average Wind Speed",
        "Maximum Gust Wind Speed"
    ]

    data_age = None
    data_set = []

    for record in query_data:
        data_set.append(
            [
                record.time_stamp.isoformat(),
                record.max_temp,
                record.min_temp,
                record.max_humid,
                record.min_humid,
                record.max_pressure,
                record.min_pressure,
                record.total_rainfall,
                record.max_average_wind_speed,
                record.max_gust_wind_speed
            ]
        )

        data_age = record.time_stamp

    result = {
        'data': data_set,
        'labels': labels
    }

    json_data = json.dumps(result)

    return json_data, data_age

def indoor_sample_result_to_json(query_data):
    """
    Converts the supplied indoor sample query data to JSON format.
    :param query_data: Data to convert.
    :return: json_data, data_age
    """

    labels = ["Time",
              "Temperature",
              "Relative Humidity",
              ]

    data_age = None
    data_set = []

    for record in query_data:
        if record.gap:
            # Insert a gap
            data_set.append([
                record.time_stamp.isoformat(),
                None, None
            ])

        data_set.append(
            [
                record.time_stamp.isoformat(),
                record.indoor_temperature,
                record.indoor_relative_humidity,
                ]
        )

        data_age = record.time_stamp

    result = {
        'data': data_set,
        'labels': labels
    }

    json_data = json.dumps(result)

    return json_data, data_age

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
