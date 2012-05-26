"""
The code in this file receives requests from the URL router, processes them
slightly, puts on the content-type header where required and then dispatches
to the appropriate handler function for the specified UI.
"""

from web.contrib.template import render_jinja

from basic_ui import BasicUI
import config
from data.database import day_exists, month_exists, year_exists
from modern_ui import ModernUI
import datetime
import web
import os
from util import month_number, month_name

__author__ = 'David Goodwin'

# Register available UIs
uis = {ModernUI.ui_code(): ModernUI(),
       BasicUI.ui_code(): BasicUI(),
      }

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
        Shows a UI choosing page (or goes straight to the only UI if there
        is only one) if config.default_ui is None, redirects to
        the default UI if config.default_ui is not None.
        :return: UI Choosing page if anything.
        :raise: web.seeother() if default is specified or there is only one UI.
        """

        if config.default_ui is not None:
            raise web.seeother(config.site_root + config.default_ui + '/' +
                               config.default_station_name + '/')

        if len(uis) == 1:
            # If there is only one UI registered just redirect straight to it.
            raise web.seeother(config.site_root + uis.items()[0][0] + '/'
                               + config.default_station_name + '/')

        ui_list = []

        for ui in uis.items():
            ui_key = ui[1].ui_code() + '/' + config.default_station_name
            ui_name = ui[1].ui_name()
            ui_desc = ui[1].ui_description()
            ui_list.append((ui_key,ui_name,ui_desc))

        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        html_file()
        return render.ui_list(uis=ui_list)

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

class index:
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
        return uis[ui].get_station(station)

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

class year:
    """
    Gives an overview for a year
    """
    def GET(self, ui, station, year):
        """
        Fetches the year page (index.html) after doing some basic validation.
        :param ui: UI to fetch the page for
        :type ui: str
        :param station: Station to fetch data for
        :type station: str
        :param year: Year page to get
        :type year: str
        :return: HTML data for the appropriate year, station and ui
        :rtype: str
        """
        validate_request(ui,station,year)
        html_file()
        return uis[ui].get_year(station, int(year))

