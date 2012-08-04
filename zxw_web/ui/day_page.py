# coding=utf-8
"""
Handles fetching the day pages in all UIs.
"""

from datetime import datetime, timedelta, date
from web.contrib.template import render_jinja
import config
from months import month_name

from cache import day_cache_control
from database import get_live_data, get_daily_records, total_rainfall_in_last_7_days, day_exists, get_live_indoor_data
from ui import get_nav_urls
import os
from ui import html_file, month_number, validate_request
from url_util import relative_url

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                 os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

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

def get_day_data_urls(station, day, ui, overview_page=False):
    """
    Gets data URLs for a day page or the station overview page.
    :param station: Station to get URLs for
    :type station: str, unicode
    :param day: Day to get URLs for
    :type day: date
    :param overview_page: If the URLs should be relative to the overview
                          page instead of the day page
    :type overview_page: bool
    :param ui: The UI to get data URLs for
    :type ui: str
    """

    base_url = '/data/{0}/{1}/{2}/{3}/'.format(station,
                                               day.year,
                                               day.month,
                                               day.day)
    if overview_page:
        current_url = '/s/{0}/'.format(station)
    else:
        current_url = '/s/{0}/{1}/{2}/{3}/'.format(station,
                                                   day.year,
                                                   month_name[day.month],
                                                   day.day)

    if ui == 'm':
        sub_dir = ''
    else:
        sub_dir = 'datatable/'

    urls = {
        'samples': relative_url(current_url,
                                base_url + sub_dir + 'samples.json'),
        '7day_samples': relative_url(current_url,
                                     base_url + sub_dir + '7day_30m_avg_samples.json'),
        'rainfall': relative_url(current_url,
                                 base_url + sub_dir + 'hourly_rainfall.json'),
        '7day_rainfall': relative_url(current_url,
                                      base_url + sub_dir + '7day_hourly_rainfall.json'),
        'records': relative_url(current_url, base_url + 'records.json'),
        'rainfall_totals': relative_url(current_url, base_url + 'rainfall.json'),
        }

    return urls

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

    # TODO: Make this a dict sometime.
    class data:
        """ Data required by the view. """
        date_stamp = day
        current_data = None

        prev_url = None
        next_url = None
        prev_date = date_stamp - timedelta(1)
        next_date = date_stamp + timedelta(1)
        this_month = month_name[day.month].capitalize()
        records = get_daily_records(date_stamp)

        if ui in ('s','m'):
            rainfall_7days_total = total_rainfall_in_last_7_days(date_stamp)


    # Get live data if the page is for today.
    data_age = None
    if today:
        data.current_data_ts, data.current_data = get_live_data()
        data_age = datetime.combine(day, data.current_data_ts)

    data.prev_url, data.next_url = get_day_nav_urls(ui, station, data.date_stamp)

    day_cache_control(data_age, day)

    if ui in ('s','m'):
        nav_urls = get_nav_urls(station, current_location)
        data_urls = get_day_data_urls(station, data.date_stamp, ui)
        return modern_templates.day(nav=nav_urls,
                             data_urls=data_urls,
                             data=data,
                             station=station,
                             ui=ui,
                             sitename=config.site_name)
    else:
        return basic_templates.day(data=data,
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

def get_indoor_data_urls(station, day, ui):
    """
    Gets the Data URLs for the indoor day page.
    :param station: Station we are looking at
    :type station: str or unicode
    :param day: Day to get data for
    :type day: date
    :return: URLs dict.
    :type: dict
    """

    if ui == 'm':
        sub_dir = ''
    else:
        sub_dir = 'datatable/'

    data_base_url = '../../../../../data/{0}/{1}/{2}/{3}/'\
    .format(station, day.year, day.month, day.day)
    samples = data_base_url + sub_dir + 'indoor_samples.json'
    samples_7day = data_base_url + sub_dir + '7day_indoor_samples.json'
    samples_7day_30mavg = data_base_url + sub_dir + '7day_30m_avg_indoor_samples.json'

    return {
        'samples': samples,
        'samples_7day': samples_7day,
        'samples_7day_30mavg': samples_7day_30mavg
    }



def get_indoor_day(ui, station, day):
    """
    Gets a page showing temperature and humidity at the base station.

    :param ui: UI to get
    :type ui: str
    :param station: The station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param day: Page day
    :type day: date
    :return: View data
    """
    current_location = '/s/' + station + '/' + str(day.year) + '/' +\
                       month_name[day.month] + '/' + str(day.day) + '/indoor.html'

    # TODO: Make this a dict sometime.
    class data:
        """ Data required by the view """
        date_stamp = day
        current_data = None
        current_data_ts = None

    # Figure out if there is current data to show or if this is a history
    # page
    now = datetime.now().date()
    today = now == day

    if today:
        data.current_data = get_live_indoor_data()
        data.current_data_ts = data.current_data.time_stamp

    day_cache_control(data.current_data_ts, day)

    if ui in ('s','m'):
        nav_urls = get_nav_urls(station, current_location)
        data_urls = get_indoor_data_urls(station, data.date_stamp, ui)
        return modern_templates.indoor_day(data=data,
                                 nav=nav_urls,
                                 dataurls=data_urls,
                                 sitename=config.site_name,
                                 ui=ui)
    else:
        return basic_templates.indoor_day(data=data)

class indoor_day:
    """
    Gives an overview for a day.
    """
    def GET(self, ui, station, year, month, day):
        """
        Performs some basic validation and then fetches indoor_day.html
        :param ui: UI to view
        :type ui: str
        :param station: Station to view
        :type station: str
        :param year: Page year
        :type: str
        :param month: Page month (full month name in lowercase)
        :type: str
        :param day: Page day
        :type: str
        :return: HTML for the indoor_day.html page.
        """
        validate_request(ui,station,year,month,day)
        html_file()
        return get_indoor_day(ui, station, date(int(year), month_number[month], int(day)))
