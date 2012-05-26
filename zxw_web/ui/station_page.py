"""
Handles the station overview page and any other station-level pages.
"""

from datetime import datetime, timedelta
from web.contrib.template import render_jinja
from cache import day_cache_control
from data.database import get_daily_records, get_years, total_rainfall_in_last_7_days, day_exists
from modern_ui import ModernUI
import os
from months import month_name
from ui.ui_route import validate_request, html_file

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

def get_station_standard(station):
    """
    Index page for the weather station. Should give station records and
    perhaps some basic yearly overview data.
    :param station: Name of the station to show info for.
    :return: View data.
    """

    current_location = '/s/' + station + '/'

    now = datetime.now()

    class data:
        """ Data required by the view """
        records = get_daily_records(now.date())
        years = get_years()
        year = now.year
        month_s = month_name[now.month]
        yesterday = now - timedelta(1)
        yesterday_month_s = month_name[yesterday.month]
        rainfall_7days_total = total_rainfall_in_last_7_days(now.date())

    if not day_exists(data.yesterday):
        data.yesterday = None
        data.yesterday_month_s = None

    day_cache_control(None, now.year, now.month, now.day)
    nav_urls = ModernUI.get_nav_urls(station, current_location)
    return modern_templates.station(nav=nav_urls,
                               data=data,
                               station=station)

def get_station_basic(station):
    """
    Index page for the weather station. Should give station records and
    perhaps some basic yearly overview data.
    :param station:
    :return:
    """

    return basic_templates.station(years=get_years(),station=station)

class station:
    """
    Index page for the station. Should give station records, basic yearly
    overview data, etc.
    """
    def GET(self, ui, station):
        """
        Station overview page (on the standard UI) or year list page (basic UI)
        :param ui: UI to use
        :type ui: str
        :param station: Station to get data for
        :type station: str
        :return: HTML data.
        """
        validate_request(ui,station)
        html_file()

        if ui == 's':
            return get_station_standard(station)
        else:
            return get_station_basic(station)