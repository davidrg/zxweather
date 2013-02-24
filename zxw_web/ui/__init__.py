# coding=utf-8
"""
This package contains code for producing the user interface (HTML pages for
human consumption rather than JSON data for use by other systems)
"""

from datetime import timedelta, datetime, date
from months import month_name, month_number
from url_util import relative_url
import config
from database import day_exists, month_exists, year_exists, get_station_id, get_sample_range
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

    home = '/*/' + station + '/'
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


def make_station_switch_urls(station_list, current_url, validation_func=None,
                             target_date=None):
    """
    Makes a list of URLs for switching from the current URL to the equivalent
    URL on each other station in the database.
    :param station_list: List of stations to build URLs for. The list should
            be of (station_code, station_name) tuples.
    :type station_list: list
    :param current_url: The URL to build equivalent URLs for on other stations
    :type current_url: str
    :param validation_func: Function to check if the equivalent URL actually
            exists on a given station. It should take only a station_id
            parameter.
    :type validation_func: callable
    :param target_date: The date that we will pages we will be building URLs
            for. This should be a tuple containing one to three elements in
            the order year, month, day. A year only page would, of course, only
            supply (year,)
    :type target_date: tuple
    :return: A similar station list to that supplied but the first item in
            each tuple will be the target URL instead of the station code.
    :rtype: list
    """
    new_station_list = []

    for station in station_list:
        code = station[0]
        name = station[1]
        station_id = get_station_id(code)

        is_valid = True

        if validation_func is not None:
            is_valid = validation_func(station_id)

        if not is_valid:
            # There is no data for the target URL we were going to build.
            # We need to build a javascript function to get the

            min_ts, max_ts = get_sample_range(station_id)

            earliest_target = '/*/' + code + '/' + str(min_ts.year) + '/' + \
                              month_name[min_ts.month] + '/' + \
                              str(min_ts.day) + '/'
            latest_target = '/*/' + code + '/' + str(max_ts.year) + '/' + \
                            month_name[max_ts.month] + '/' + \
                            str(max_ts.day) + '/'

            earliest_url = relative_url(current_url, earliest_target)
            latest_url = relative_url(current_url, latest_target)
            earliest_date = str(min_ts.day) + ' ' + month_name[min_ts.month] +\
                ' ' + str(min_ts.year)
            latest_date = str(max_ts.day) + ' ' + month_name[max_ts.month] + \
                ' ' + str(max_ts.year)

            disp_date = str(target_date[0])
            if len(target_date) > 1:
                disp_date = month_name[target_date[1]] + ' ' + disp_date
            if len(target_date) == 3:
                disp_date = str(target_date[2]) + ' ' + disp_date

            new_url = "javascript:show_no_data('{station_name}', '{date}', " \
                      "'{earliest_date}', '{earliest_url}', '{latest_date}', " \
                      "'{latest_url}');".format(station_name=name.replace(
                                                "'", r"\'"),
                                                date=disp_date,
                                                earliest_date=earliest_date,
                                                earliest_url=earliest_url,
                                                latest_date=latest_date,
                                                latest_url=latest_url)
        else:
            target = '/*/' + code + '/'

            # The first three segments in the URL will be '', 's' and the station
            # code. We can throw those away to get the new target URL.
            target += '/'.join(current_url.split('/')[3:])

            new_url = relative_url(current_url, target)

        new_station_list.append((new_url, name))
    return new_station_list


# Register available UIs
uis = ['s','b','m']


def build_alternate_ui_urls(current_url):
    """
    Builds URLs to switch to the current URL in all available user interfaces.
    :param current_url:
    :return:
    """
    global uis

    # The first two segments in the URL will be '' and 's'. We want everything
    # after this
    page = '/'.join(current_url.split('/')[2:])

    switch_urls = {}

    for ui in uis:
        url = '/{0}/{1}'.format(ui, page)

        # A little hack to redirect from the modern station overview page to
        # the basic current day page instead of the basic station overview page.
        if len(url.split('/')) == 4 and ui == 'b':
            if url.split('/')[3] == '':
                url += 'now'

        switch_urls[ui] = relative_url(current_url, url)

    return switch_urls


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

    if station is not None:
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

        # Just redirect straight to the default station. There should be a menu
        # on that page where the user can navigate to a different one if
        # required. TODO: Add a cookie to remember the users setting.
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
