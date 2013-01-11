# coding=utf-8
"""
Data sources.
"""

import json
import web
from cache import rfcformat
import config
from datetime import timedelta, datetime
from database import day_exists, get_station_id
from months import month_name

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
            'site_name': config.site_name
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
            'site_name': config.site_name,
            'archive_mode': archive_mode
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