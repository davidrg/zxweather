# coding=utf-8
"""
Provides access to zxweather daily data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime
from cache import day_cache_control
from data.daily_datatable import datatable_datasources
from database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp, day_exists
import os
import web
from web.contrib.template import render_jinja
from config import db
import config
import json
from data.util import datetime_to_js_date, outdoor_sample_result_to_datatable, pretty_print

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

    json_data = json.dumps(data)

    day_cache_control(age,day)
    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_day_rainfall(day):
    """
    Gets JSON data containing total rainfall for the day and the past seven
    days.
    :param day:
    """
    rainfall = get_daily_rainfall(day)

    age = get_latest_sample_timestamp()
    json_data = json.dumps(rainfall)
    day_cache_control(age,day)
    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

# Data sources available at the day level.
datasources = {
    'records': {
        'desc': 'Records for the day.',
        'func': get_day_records
    },
    'rainfall': {
        'desc': 'Rainfall totals for the day and the past seven days.',
        'func': get_day_rainfall
    },
}

def datasource_dispatch(station, datasource_dict, dataset, day):
    """
    Dispatches a request for a data source.
    :param station: Station to use
    :type station: str
    :param datasource_dict: Dict of datasources that are valid at this point.
    :type datasource_dict: dict
    :param dataset: dataset that was requested
    :type dataset: str
    :param day: Day that data was requested for
    :type day: date
    :return: Response data
    :raise: web.NotFound() if request is invalid
    """
    if station != config.default_station_name:
        raise web.NotFound()
    print dataset
    # Make sure the day actually exists in the database before we go
    # any further.
    if not day_exists(day):
        raise web.NotFound()

    if dataset in datasource_dict:
        return datasource_dict[dataset]['func'](day)
    else:
        raise web.NotFound()

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
        this_date = date(int(year),int(month),int(day))

        return datasource_dispatch(station,
                                   datatable_datasources,
                                   dataset,
                                   this_date)

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
        this_date = date(int(year),int(month),int(day))

        return datasource_dispatch(station,
                                   datasources,
                                   dataset,
                                   this_date)

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
        if not day_exists(date(int(year),int(month),int(day))):
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.daily_data_index(data_sources=datasources,
                                       dt_datasources=datatable_datasources)
