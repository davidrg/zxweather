"""
The code in this file receives requests from the URL router, processes them
slightly, puts on the content-type header where required and then dispatches
to the appropriate handler function for the specified UI.
"""

from months import month_number, month_name

import config
from database import day_exists, month_exists, year_exists
import datetime
import web

__author__ = 'David Goodwin'

# Register available UIs
uis = ['s','b']

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

    if station is not None and station != config.default_station_name:
        raise web.NotFound()

    # Check the date.
    if year is not None and month is not None and day is not None:
        result = day_exists(datetime.date(int(year),month_number[month],int(day)))
        if not result:
            raise web.NotFound()
    elif year is not None and month is not None:
        result = month_exists(int(year),month_number[month])
        if not result:
            raise web.NotFound()
    elif year is not None:
        result = year_exists(int(year))
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

        now = datetime.datetime.now()

        todays_url = config.site_root + ui + '/' + station + '/' + \
                     str(now.year) + '/' + month_name[now.month] + '/' + str(now.day) + '/'

        raise web.seeother(todays_url)

