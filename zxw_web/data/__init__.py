# coding=utf-8
"""
Data sources.
"""

import json
import os
import web
from web.contrib.template import render_jinja
from cache import rfcformat
import config
from datetime import timedelta, datetime
from database import day_exists, get_station_id, get_station_name, get_stations, get_full_station_info, \
    get_site_name
from months import month_name
from static_overlays import overlay_file

__author__ = 'David Goodwin'

class day_nav(object):
    """
    Controller for /data/daynav.json. No idea why its called daynav.

    obsolete: This URI doesn't take a station code which limits its usefulness
    somewhat. You should use /(s|m)/(code)/about.json instead now.
    """
    def GET(self):
        """
        Gets a JSON file containing URLs for various pages (yesterday, this
        month, this year, etc). Its used by the about page.
        :return:
        """
        now = datetime.now().date()
        yesterday = now - timedelta(1)

        data = {
            'yesterday': str(yesterday.year) + '/' + month_name[yesterday.month] + '/' + str(yesterday.day) + '/',
            'this_month': str(now.year) + '/' + month_name[now.month] + '/',
            'this_year': str(now.year) + '/',
            'site_name': get_site_name(None)
        }

        tomorrow = now + timedelta(1)
        tomorrow = datetime(year=tomorrow.year,
                            month=tomorrow.month,
                            day=tomorrow.day,
                            hour=0,minute=0,second=0)
        today = datetime(year=now.year,
                         month=now.month,
                         day=now.day,
                         hour=0,minute=0,second=0)
        web.header('Last-Modified', rfcformat(today))
        web.header('Expires', rfcformat(tomorrow))
        web.header('Content-Type', 'application/json')
        return json.dumps(data)

    @staticmethod
    def get_day_nav():
        nav = day_nav()
        return nav.GET()


class about_nav(object):
    """
    Controller for /ui/station/about.json.
    """
    def GET(self, station_code):
        """
        Gets a JSON file containing URLs for various pages (yesterday, this
        month, this year, etc). Its used by the about page.
        :param station_code: Code for this weather station
        :type station_code: str or unicode
        :return:
        """

        station_id = get_station_id(station_code)

        now = datetime.now().date()
        yesterday = now - timedelta(1)

        archive_mode = False
        if not day_exists(datetime.now().date(), station_id):
            archive_mode = True

        data = {
            'yesterday': str(yesterday.year) + '/' + month_name[yesterday.month] + '/' + str(yesterday.day) + '/',
            'this_month': str(now.year) + '/' + month_name[now.month] + '/',
            'this_year': str(now.year) + '/',
            'site_name': get_site_name(station_id),
            'archive_mode': archive_mode,
            'station_code': station_code,
            'station_name': get_station_name(station_id),
            'station_list': get_stations(),
        }

        tomorrow = now + timedelta(1)
        tomorrow = datetime(year=tomorrow.year,
            month=tomorrow.month,
            day=tomorrow.day,
            hour=0,minute=0,second=0)
        today = datetime(year=now.year,
            month=now.month,
            day=now.day,
            hour=0,minute=0,second=0)
        web.header('Last-Modified', rfcformat(today))
        web.header('Expires', rfcformat(tomorrow))
        web.header('Content-Type', 'application/json')
        return json.dumps(data)

class data_index(object):
    def GET(self):
        """
        Gets a list of data sources available at the station level.
        :param station: Station to get data for
        :type station: string
        :return: html view data.
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        stations = get_stations()

        web.header('Content-Type', 'text/html')
        return render.data_index(stations=stations)

class data_json(object):
    def GET(self, resource):
        if resource == 'daynav':
            return day_nav.get_day_nav()
        elif resource == 'sysconfig':
            return get_sys_config_json()
        else:
            static = overlay_file()
            return static.GET('/data/{0}.json'.format(resource))


def get_sys_config_json():
    sys_config = {
        'ws_uri': config.ws_uri,
        'wss_uri': config.wss_uri,
        'zxweatherd_host': config.zxweatherd_hostname,
        'zxweatherd_raw_port': config.zxweatherd_raw_port,
        'site_name': get_site_name(None),
        'stations': get_full_station_info()
    }

    return json.dumps(sys_config)