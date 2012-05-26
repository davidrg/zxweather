"""
Handles fetching the day pages in all UIs.
"""

from datetime import datetime, timedelta, date
from web.contrib.template import render_jinja
from baseui import BaseUI

from basic_ui import month_name
from data.database import get_live_data, get_daily_records, total_rainfall_in_last_7_days, day_exists
from modern_ui import ModernUI
import os
from ui_route import html_file, month_number, validate_request
from util import relative_url

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                 os.path.join('basic_templates'))
basic_render = render_jinja(basic_template_dir, encoding='utf-8').day
#
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('modern_templates'))
modern_render = render_jinja(modern_template_dir, encoding='utf-8').day

def get_day_nav_urls(ui, station, day):
    """
    Returns navigation urls for the next and previous day pages relative
    to the current day page.
    :param ui: UI to get URLs for.
    :type ui: str
    :param station: Station to get URLs for
    :type station: str or unicode
    :param day: Current day page.
    :type day: date
    :return: previous_url, next_url
    :rtype: str,str
    """

    # Figure out the URL for the previous day
    prev_date = day - timedelta(1)

    url_format_string = '/{0}/{1}/{2}/{3}/{4}/'
    current_location = url_format_string.format(ui, station, day.year,
                                                month_name[day.month],
                                                day.day)

    prev_url = ""
    next_url = ""

    # Only calculate previous days data if there is any.
    if day_exists(prev_date):

        abs_url = url_format_string.format(ui,
                                           station,
                                           prev_date.year,
                                           month_name[prev_date.month],
                                           prev_date.day)
        prev_url = relative_url(current_location, abs_url)

    # Only calculate the URL for tomorrow if there is tomorrow in the database.
    next_date = day + timedelta(1)

    if day_exists(next_date):
        abs_url = url_format_string.format(ui,
                                           station,
                                           next_date.year,
                                           month_name[next_date.month],
                                           next_date.day)
        next_url = relative_url(current_location, abs_url)

    return prev_url, next_url

def get_day_page(ui, station, day):
    """
    Gives an overview for a day. If the day is today then current weather
    conditions are also shown.
    :param ui: The user interface to fetch. 's' for the standard (modern) UI
               or 'b' for the basic one.
    :type ui: str
    :param station: Station to get the page for. Only used for building
    URLs at the moment
    :type station: string
    :param day: Page day
    :type day: date
    :return: View data
    """
    current_location = '/s/' + station + '/' + str(day.year) + '/' +\
                       month_name[day.month] + '/' + str(day.day) + '/'

    # Figure out if there is current data to show or if this is a history
    # page
    now = datetime.now().date()
    today = now == day

    class data:
        """ Data required by the view. """
        date_stamp = day
        current_data = None

        prev_url = None
        next_url = None
        prev_date = date_stamp + timedelta(1)
        next_date = date_stamp + timedelta(1)
        this_month = month_name[day.month].capitalize()
        records = get_daily_records(date_stamp)

        if ui == 's':
            rainfall_7days_total = total_rainfall_in_last_7_days(date_stamp)


    # Get live data if the page is for today.
    data_age = None
    if today:
        data.current_data_ts, data.current_data = get_live_data()
        data_age = datetime.combine(day, data.current_data_ts)

    data.prev_url, data.next_url = get_day_nav_urls(ui, station, data.date_stamp)

    BaseUI.day_cache_control(data_age, day.year, day.month, day.day)

    if ui == 's':
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        data_urls = ModernUI.get_day_data_urls(station, data.date_stamp, False)
        return modern_render(nav=nav_urls,
                             data_urls=data_urls,
                             data=data,
                             station=station)
    else:
        return basic_render(data=data,
                            station=station)

class day:
    """
    Gives an overview for a day.
    """
    def GET(self, ui, station, year, month, day):
        """
        Gets the day page after validating and translating parameters.

        :param ui: The UI to fetch.
        :param station: The station to fetch.
        :param year: Year to get data for
        :param month: month name (in lower case)
        :param day: Day to get data for
        :return: HTML data for the page.
        """
        validate_request(ui,station,year,month,day)
        html_file()
        return get_day_page(ui, station, date(int(year), month_number[month], int(day)))