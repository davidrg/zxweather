"""
Data sources at the station level.
"""
from datetime import datetime, timedelta
import json
import web
from web.contrib.template import render_jinja
import config
from data.database import get_years
from data.util import rfcformat
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
        else:
            raise web.NotFound()


def live_data():
    """
    Gets a JSON file containing live data.
    :return: JSON data.
    """
    data_ts, data = live_data.get_live_data()

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



    if config.live_data_available:
        web.header('Cache-Control', 'max-age=' + str(48))
        web.header('Expires', rfcformat(data_ts + timedelta(0, 48)))
    else:
        web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
        web.header('Expires', rfcformat(data_ts + timedelta(0, config.sample_interval)))
    web.header('Last-Modified', rfcformat(data_ts))

    web.header('Content-Type', 'application/json')
    return json.dumps(result)