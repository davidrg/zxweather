# coding=utf-8
"""
This package contains code for producing the user interface (HTML pages for
human consumption rather than JSON data for use by other systems)
"""

from datetime import timedelta, datetime, date
from months import month_name, month_number
from url_util import relative_url
import config
from database import day_exists, month_exists, year_exists, get_station_id
import web

__author__ = 'David Goodwin'

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
    yesterday = home + str(yesterday.year) + '/' +\
                month_name[yesterday.month] + '/' +\
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


# Register available UIs
uis = ['s','b','m']

def validate_request(ui=None,station=None, year=None, month=None, day=None):
    """
    Validates request parameters. All parameters are optional.
    :param ui: UI that was requested.
    :param station: Station that was requested
    :param year: Data year
    :param month: Data month
    :param day: Data day
    :raise: web.NotFound if the request is invalid.
    """

    if ui is not None and ui not in uis:
        raise web.NotFound()

    station_id = get_station_id(station)

    if station_id is None:
        raise web.NotFound()

    # Check the date.
    if year is not None and month is not None and day is not None:
        result = day_exists(date(int(year),month_number[month],int(day)), station_id)
        if not result:
            raise web.NotFound()
    elif year is not None and month is not None:
        result = month_exists(int(year),month_number[month], station_id)
        if not result:
            raise web.NotFound()
    elif year is not None:
        result = year_exists(int(year), station_id)
        if not result:
            raise web.NotFound()

def html_file():
    """
    Puts on headers for HTML files.
    """
    web.header('Content-Type','text/html')


class site_index:
    """
    The sites index page (/). Should redirect to the default station.
    """
    def GET(self):
        """
        Redirects the user to the default UI. If no default is specified the
        standard UI ('s') is used.
        :return: UI Choosing page if anything.
        :raise: web.seeother() if default is specified or there is only one UI.
        """

        if config.default_ui is not None:
            raise web.seeother(config.site_root + config.default_ui + '/' +
                               config.default_station_name + '/')
        else:
            raise web.seeother(config.site_root + 's/' +
                               config.default_station_name + '/')


class stationlist:
    """
    Index page for a particular station. Should allow you to choose how to
    view data I suppose. Or give information on the station.
    """
    def GET(self, ui):
        """
        View station details.
        :param ui: The UI to use
        :return: A view.
        """

        validate_request(ui)

        # Only one station is currently supported so just redirect straight
        # to it rather than giving the user a choice of only one option.
        raise web.seeother(config.site_root + ui + '/' + config.default_station_name + '/')

class now:
    """
    Redirects to the page for today.
    """
    def GET(self, ui, station):
        """
        Raises web.seeother to redirect the client to the current day page.

        :param ui: UI to redirect to
        :param station: Station to redirect to
        :raise: web.seeother
        """
        validate_request(ui,station)

        today = datetime.now().date()

        todays_url = config.site_root + ui + '/' + station + '/' +\
                     str(today.year) + '/' + month_name[today.month] + '/' + str(today.day) + '/'

        raise web.seeother(todays_url)