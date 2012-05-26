"""
Data sources.
"""

import json
import web
import config
from data.util import rfcformat
from datetime import timedelta, datetime
from months import month_name

__author__ = 'David Goodwin'

class day_nav:
    """
    Controller for /data/daynav.json. No idea why its called daynav.
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
            'site_name': config.site_name,
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