"""
Modern HTML5/CSS/Javascript UI.
"""

from datetime import datetime, timedelta
from baseui import BaseUI
from cache import year_cache_control, day_cache_control
from data.database import day_exists, year_exists, total_rainfall_in_last_7_days, get_daily_records, get_years
from util import relative_url, month_name

__author__ = 'David Goodwin'

class ModernUI(BaseUI):
    """
    Class for the basic UI - plain old HTML 4ish output.
    """

    def __init__(self):
        BaseUI.__init__(self, 'modern_templates')

    @staticmethod
    def ui_code():
        """
        :return: URL code for the UI.
        """
        return 's'
    @staticmethod
    def ui_name():
        """
        :return: Name of the UI
        """
        return 'Standard Modern'
    @staticmethod
    def ui_description():
        """
        :return: Description of the UI.
        """
        return 'Standard interface. Not compatible with older browsers or computers.'

    @staticmethod
    def get_nav_urls(station, current_url):
        """
        Gets a dict containing standard navigation URLs relative to the
        current location in the site.
        :param station: The station we are currently working with.
        :type station: str, unicode
        :param current_url: Where in the site we currently are
        :type current_url: str
        """

        now = datetime.now().date()
        yesterday = now - timedelta(1)

        home = '/s/' + station + '/'
        yesterday = home + str(yesterday.year) + '/' + \
                    month_name[yesterday.month] + '/' + \
                    str(yesterday.day) + '/'
        this_month = home + str(now.year) + '/' + month_name[now.month] + '/'
        this_year = home + str(now.year) + '/'
        about = home + 'about.html'

        urls = {
            'home': relative_url(current_url, home),
            'yesterday': relative_url(current_url,yesterday),
            'this_month': relative_url(current_url,this_month),
            'this_year': relative_url(current_url,this_year),
            'about': relative_url(current_url, about),
        }

        return urls


