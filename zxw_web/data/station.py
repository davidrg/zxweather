# coding=utf-8
"""
Data sources at the station level.
"""
from datetime import datetime
import json
import web
from web.contrib.template import render_jinja
from cache import live_data_cache_control
import config
from data.daily import get_day_records, get_day_rainfall, get_day_samples_json, get_7day_30mavg_samples_json, get_days_hourly_rainfall_json, get_7day_hourly_rainfall_json, get_7day_30mavg_samples_datatable, get_day_samples_datatable, get_days_hourly_rainfall_datatable, get_7day_hourly_rainfall_datatable
from database import get_years, get_live_data
import os

__author__ = 'David Goodwin'


class index:
    """ station level data sources"""
    def GET(self, station):
        """
        Gets a list of data sources available at the station level.
        :param station: Station to get data for
        :type station: string
        :return: html view data.
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        if station != config.default_station_name:
            raise web.NotFound()

        years = get_years()

        web.header('Content-Type', 'text/html')
        return render.station_data_index(years=years)

class data_json:
    """
    JSON data sources at the station level
    """
    def GET(self, station, dataset):
        """
        Handles station-level JSON data sources.
        :param station: Station to get data for
        :param dataset: Dataset to get.
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        if dataset == 'live':
            return live_data()
        elif dataset == 'current_day_records':
            return current_day_records()
        elif dataset == 'current_day_rainfall_totals':
            return current_day_rainfall()
        elif dataset == 'current_day_samples':
            return current_samples()
        elif dataset == 'current_day_7day_30m_avg_samples':
            return current_7day_30mavg_samples()
        elif dataset == 'current_day_rainfall':
            return current_day_hourly_rainfall_json()
        elif dataset == 'current_day_7day_rainfall':
            return current_day_7day_rainfall_json()
        else:
            raise web.NotFound()

class datatable_json:
    """
    JSON data sources at the station level
    """
    def GET(self, station, dataset):
        """
        Handles station-level JSON data sources.
        :param station: Station to get data for
        :param dataset: Dataset to get.
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        if dataset == 'current_day_samples':
            return current_samples_datatable()
        elif dataset == 'current_day_7day_30m_avg_samples':
            return current_7day_30m_avg_samples_datatable()
        elif dataset == 'current_day_rainfall':
            return current_day_hourly_rainfall_datatable()
        elif dataset == 'current_day_7day_rainfall':
            return current_day_7day_rainfall_datatable()
        else:
            raise web.NotFound()


def live_data():
    """
    Gets a JSON file containing live data.
    :return: JSON data.
    """
    data_ts, data = get_live_data()

    now = datetime.now()
    data_ts = datetime.combine(now.date(), data_ts)

    result = {'relative_humidity': data.relative_humidity,
              'temperature': data.temperature,
              'dew_point': data.dew_point,
              'wind_chill': data.wind_chill,
              'apparent_temperature': data.apparent_temperature,
              'absolute_pressure': data.absolute_pressure,
              'average_wind_speed': data.average_wind_speed,
              'gust_wind_speed': data.gust_wind_speed,
              'wind_direction': data.wind_direction,
              'time_stamp': str(data.time_stamp),
              'age': data.age,
              }

    live_data_cache_control(data_ts)
    web.header('Content-Type', 'application/json')
    return json.dumps(result)

def current_day_records():
    """
    Gets records for the current day.
    :return: Records for the current day.
    """
    return get_day_records(datetime.now().date())

def current_samples_datatable():
    """
    Gets samples for the current day.
    :return: Samples for the current day.
    """
    return get_day_samples_datatable(datetime.now().date())

def current_samples():
    """
    Gets samples for the current day.
    :return: Samples for the current day.
    """
    return get_day_samples_json(datetime.now().date())

def current_7day_30m_avg_samples_datatable():
    """
    Gets 7-day 30minute average samples for the current day.
    :return: 30-minute 7-day samples for the current day.
    """
    return get_7day_30mavg_samples_datatable(datetime.now().date())

def current_7day_30mavg_samples():
    """
    Gets 7-day 30minute average samples for the current day.
    :return: 30-minute 7-day samples for the current day.
    """
    return get_7day_30mavg_samples_json(datetime.now().date())

def current_day_hourly_rainfall_datatable():
    """
    Gets hourly rainfall for the current day.
    :return: Hourly rainfall for the current day.
    """
    return get_days_hourly_rainfall_datatable(datetime.now().date())

def current_day_hourly_rainfall_json():
    """
    Gets hourly rainfall for the current day.
    :return: Hourly rainfall for the current day.
    """
    return get_days_hourly_rainfall_json(datetime.now().date())

def current_day_7day_rainfall_datatable():
    """
    Gets hourly rainfall for the seven days prior to today.
    :return: Hourly rainfall data for the past seven days.
    """
    return get_7day_hourly_rainfall_datatable(datetime.now().date())

def current_day_7day_rainfall_json():
    """
    Gets hourly rainfall for the seven days prior to today.
    :return: Hourly rainfall data for the past seven days.
    """
    return get_7day_hourly_rainfall_json(datetime.now().date())

def current_day_rainfall():
    """
    Gets rainfall totals for the current day and the past seven days.
    :return: Rainfall totals for today and the past seven days.
    """
    return get_day_rainfall(datetime.now().date())