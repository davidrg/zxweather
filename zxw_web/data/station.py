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
from data import daily
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

        pass_through_data_sets = {
            'current_day_records':'records',
            'current_day_rainfall_totals':'rainfall',
            'current_day_samples':'samples',
            'current_day_7day_30m_avg_samples':'7day_30m_avg_samples',
            'current_day_rainfall':'hourly_rainfall',
            'current_day_7day_rainfall':'7day_hourly_rainfall',
        }

        if dataset in pass_through_data_sets.keys():
            return daily.json_dispatch(station, pass_through_data_sets[dataset], datetime.now().date())
        elif dataset == 'live':
            return live_data()
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

        pass_through_data_sets = {
            'current_day_samples':'samples',
            'current_day_7day_30m_avg_samples':'7day_30m_avg_samples',
            'current_day_rainfall':'hourly_rainfall',
            'current_day_7day_rainfall':'7day_hourly_rainfall',
        }

        if dataset in pass_through_data_sets.keys():
            return daily.dt_json_dispatch(station, pass_through_data_sets[dataset], datetime.now().date())
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
