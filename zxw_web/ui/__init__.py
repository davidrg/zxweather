# coding=utf-8
"""
This package contains code for producing the user interface (HTML pages for
human consumption rather than JSON data for use by other systems)
"""

from datetime import timedelta, datetime, date, time
from months import month_name, month_number
from url_util import relative_url
import config
from database import day_exists, month_exists, year_exists, get_station_id, get_sample_range, \
    get_station_type_code
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
        archived = station[2]
        station_id = get_station_id(code)

        is_valid = True
        ok = True

        if validation_func is not None:
            is_valid = validation_func(station_id)

        if not is_valid:
            # There is no data for the target URL we were going to build.
            # We need to build a javascript function to get the

            min_ts, max_ts = get_sample_range(station_id)

            if min_ts is not None or max_ts is not None:


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

                new_url = "javascript:show_no_data('{station_name}', " \
                          "'{date}', '{earliest_date}', '{earliest_url}', " \
                          "'{latest_date}', '{latest_url}');"\
                    .format(station_name=name.replace("'", r"\'"),
                            date=disp_date,
                            earliest_date=earliest_date,
                            earliest_url=earliest_url,
                            latest_date=latest_date,
                            latest_url=latest_url)

                new_station_list.append((new_url, name, code, archived))
        else:
            target = '/*/' + code + '/'

            # The first three segments in the URL will be '', 's' and the station
            # code. We can throw those away to get the new target URL.
            target += '/'.join(current_url.split('/')[3:])

            new_url = relative_url(current_url, target)
            new_station_list.append((new_url, name, code, archived))

    return new_station_list


# Register available UIs
uis = [
    's',    # Standard UI. HTML5 Canvas required
    'b',    # Basic UI. IE 4.0+, Nescape 4.04+. Requires PNG support.
    'a',    # Alternate UI. Requires SVG or VML. Otherwise the same as standard.
    'm'     # Mobile. Currently this is the same as standard.
]


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
        switch_urls[ui] = relative_url(current_url, url)

    return switch_urls


def validate_request(ui=None, station=None, year=None, month=None, day=None):
    """
    Validates request parameters. All parameters are optional. It also stores
    the UI (if supplied) in the last_ui cookie used in redirects.
    :param ui: UI that was requested.
    :param station: Station that was requested
    :param year: Data year
    :param month: Data month
    :param day: Data day
    :raise: web.NotFound if the request is invalid.
    """

    # Check valid UI
    if ui is not None and ui not in uis:
        raise web.NotFound()

    if ui is not None:
        web.setcookie('last_ui', ui)

    # Check station exists
    station_id = None
    if station is not None:
        station_id = get_station_id(station)

    if station_id is None:
        raise web.NotFound()

    # Check month name is valid
    if month is not None and month not in month_number.keys():
        raise web.NotFound()

    # Check year and day are integers
    year_int = None
    day_int = None
    try:
        if year is not None:
            year_int = int(year)
        if day is not None:
            day_int = int(day)
    except ValueError:
        raise web.NotFound()

    month_int = None
    if month is not None:
        month_int = month_number[month]

    # Check the date.
    if year_int is not None and month is not None and day_int is not None:
        requested_date = date(year_int, month_int, day_int)
        result = day_exists(requested_date, station_id)
        if not result:
            # The requested date isn't valid. Yet.
            if requested_date == datetime.now().date():
                # The requested date is today. Today should be valid if the
                # weather station is still operating. Probably we're just
                # waiting for the data logger to catch up. If we're only
                # 30 minutes into the day (that is, its just past midnight)
                # we'll let it pass.
                if datetime.now().time() >= time(0, 30, 0):
                    raise web.NotFound()
                # else pass
            else:
                raise web.NotFound()
    elif year is not None and month is not None:
        result = month_exists(year_int, month_number[month], station_id)
        if not result:
            raise web.NotFound()
    elif year is not None:
        result = year_exists(year_int, station_id)
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

        ui = web.cookies().get('last_ui')
        if ui is None:
            ui = config.default_ui

        if ui not in ('s', 'm', 'a', 'b'):
            ui = 's'

        raise web.seeother(config.site_root + ui + '/' +
                           config.default_station_name + '/')


class dir_redirect:
    """
    If a directory-type URL is requested (eg, /s) this will add on the slash
    to produce the proper URL and redirect.
    """
    def GET(self):
        raise web.seeother(web.ctx["path"] + "/")


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

        if ui == 'm':
            # 'm' interface doesn't exist right now. Send the user to the
            # standard UI instead.
            web.seeother(config.site_root + 's' + '/')
            return

        if ui is not None and ui not in uis:
            raise web.NotFound()

        # Just redirect straight to the default station. There should be a menu
        # on that page where the user can navigate to a different one if
        # required. TODO: Add a cookie to remember the users setting.
        raise web.seeother(config.site_root + ui + '/' +
                           config.default_station_name + '/')

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
