# coding=utf-8
"""
Handles the station overview page and any other station-level pages.
"""

from datetime import datetime, timedelta
import web
from web.contrib.template import render_jinja
from cache import day_cache_control
import config
from database import get_daily_records, get_years, \
    total_rainfall_in_last_7_days, day_exists, get_station_id, \
    get_station_name, in_archive_mode, get_station_type_code, get_stations, \
    get_live_data, get_station_message, no_data_in_24_hours
import os
from months import month_name
from ui import get_nav_urls, make_station_switch_urls, build_alternate_ui_urls
from ui import validate_request, html_file

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

def get_station_standard(ui, station):
    """
    Index page for the weather station. Should give station records and
    perhaps some basic yearly overview data.
    :param ui: The UI being used
    :type ui: str
    :param station: Name of the station to show info for.
    :return: View data.
    """

    if ui == 'm':
        # 'm' interface doesn't exist right now. Send the user to the
        # standard UI instead.
        web.seeother(config.site_root + 's' + '/' + station + '/')

    current_location = '/*/' + station + '/'

    station_id = get_station_id(station)

    now = datetime.now().date()

    class data:
        """ Data required by the view """
        records = get_daily_records(now, station_id)
        years = get_years(station_id)
        year = now.year
        month_s = month_name[now.month]
        yesterday = now - timedelta(1)
        yesterday_month_s = month_name[yesterday.month]
        rainfall_7days_total = total_rainfall_in_last_7_days(now, station_id)

    data.current_data_ts, data.current_data, \
        data.nw_type = get_live_data(station_id)

    if not day_exists(data.yesterday, station_id):
        data.yesterday = None
        data.yesterday_month_s = None

    page_data = {
        "station_name": get_station_name(station_id),
        "stations": make_station_switch_urls(
            get_stations(), current_location, None,
            (now.year, now.month, now.day)),
        "no_content": no_data_in_24_hours(station_id)
    }

    if ui == 'a':
        sub_dir = 'datatable/'
    else:
        sub_dir = ''

    msg = get_station_message(station_id)

    hw_type = get_station_type_code(station_id)

    reception_available = False

    if hw_type == 'DAVIS' and station in config.davis_station_ids:
        # TODO: check station is wireless
        reception_available = True

    day_cache_control(None, now, station_id)
    nav_urls = get_nav_urls(station, current_location)
    return modern_templates.station(nav=nav_urls,
                                    data=data,
                                    station=station,
                                    hw_type=hw_type,
                                    ui=ui,
                                    sitename=config.site_name,
                                    subdir=sub_dir,
                                    page_data=page_data,
                                    archive_mode=in_archive_mode(station_id),
                                    ws_uri=config.ws_uri,
                                    wss_uri=config.wss_uri,
                                    switch_url=build_alternate_ui_urls(
                                        current_location),
                                    station_message=msg[0],
                                    station_message_ts=msg[1],
                                    reception_available=reception_available)

def get_station_basic(station):
    """
    Index page for the weather station. Should give station records and
    perhaps some basic yearly overview data.
    :param station:
    :return:
    """

    current_location = '/*/' + station + '/'

    station_id = get_station_id(station)

    now = datetime.now().date()

    class data:
        """ Data required by the view """
        records = get_daily_records(now, station_id)
        years = get_years(station_id)
        year = now.year
        month_s = month_name[now.month]
        yesterday = now - timedelta(1)
        yesterday_month_s = month_name[yesterday.month]
        rainfall_7days_total = total_rainfall_in_last_7_days(now, station_id)

    data.current_data_ts, data.current_data, \
        data.nw_type = get_live_data(station_id)

    if not day_exists(data.yesterday, station_id):
        data.yesterday = None
        data.yesterday_month_s = None

    page_data = {
        "station_name": get_station_name(station_id),
        "station_code": station,
        "no_content": data.records is None,
        "stations": get_stations()
    }

    day_cache_control(None, now, station_id)
    nav_urls = get_nav_urls(station, current_location)

    image_base = str(now.year) + '/' + month_name[now.month] + '/' + \
        str(now.day) + '/'

    return basic_templates.station(years=get_years(station_id),
                                   station=station,
                                   data=data,
                                   nav=nav_urls,
                                   image_base=image_base,
                                   page_data=page_data,
                                   archive_mode=in_archive_mode(station_id),
                                   switch_url=build_alternate_ui_urls(
                                       current_location))


class station(object):
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

        if ui in ('s', 'm', 'a'):
            return get_station_standard(ui, station)
        else:
            return get_station_basic(station)


def get_station_reception_standard(ui, station):
    """
    Gets a page showing reception quality for the last 24 hours.

    :param ui: UI to get
    :type ui: str
    :param station: The station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param day: Page day
    :type day: date
    :return: View data
    """

    day = datetime.now().date()

    current_location = '/*/' + station + '/reception.html'

    station_id = get_station_id(station)

    def _switch_func(station_id):
        return True

    page_data = {
        "station_name": get_station_name(station_id),
        "stations": make_station_switch_urls(
            get_stations(), current_location, _switch_func,
            (day.year, day.month, day.day))
    }

    if ui in ('s', 'm', 'a'):
        nav_urls = get_nav_urls(station, current_location)
        msg = get_station_message(station_id)
        return modern_templates.reception(
            nav=nav_urls,
            sitename=config.site_name,
            ui=ui,
            archive_mode=in_archive_mode(station_id),
            page_data=page_data,
            switch_url=build_alternate_ui_urls(current_location),
            station=station,
            station_message=msg[0],
            station_message_ts=msg[1],
            basic_ui_available=False # Not available in the basic UI.
        )
    else:
        raise web.NotFound()


class reception(object):
    """
    Page giving details on station reception for the last 24 hours and 7 days.
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
        validate_request(ui, station)

        station_id = get_station_id(station)
        hw_type = get_station_type_code(station_id)

        if hw_type != 'DAVIS' or station not in config.davis_station_ids:
            # This page is only available for wireless Davis stations
            # TODO: check the station is wireless
            raise web.NotFound()

        if ui == 'b':
            # Not available in the basic UI (too much work for too little gain)
            raise web.NotFound()

        html_file()

        return get_station_reception_standard(ui, station)