# coding=utf-8
"""
Handles the station overview page and any other station-level pages.
"""

from datetime import datetime, timedelta
from web.contrib.template import render_jinja
from cache import day_cache_control
import config
from database import get_daily_records, get_years, total_rainfall_in_last_7_days, day_exists, get_station_id, get_station_name, in_archive_mode
import os
from months import month_name
from ui import get_nav_urls
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

    current_location = '/s/' + station + '/'

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

    page_data = {}

    if not day_exists(data.yesterday, station_id):
        data.yesterday = None
        data.yesterday_month_s = None

    if data.records is None:
        page_data["no_content"] = True
        page_data["station_name"] = get_station_name(station_id)
    else:
        page_data["no_content"] = False

    if ui == 'm':
        sub_dir = ''
    else:
        sub_dir = 'datatable/'

    day_cache_control(None, now, station_id)
    nav_urls = get_nav_urls(station, current_location)
    return modern_templates.station(nav=nav_urls,
                                    data=data,
                                    station=station,
                                    ui=ui,
                                    sitename=config.site_name,
                                    subdir=sub_dir,
                                    page_data=page_data,
                                    archive_mode=in_archive_mode(station_id))

def get_station_basic(station):
    """
    Index page for the weather station. Should give station records and
    perhaps some basic yearly overview data.
    :param station:
    :return:
    """
    station_id = get_station_id(station)
    return basic_templates.station(years=get_years(station_id),station=station)

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

        if ui in ('s','m'):
            return get_station_standard(ui, station)
        else:
            return get_station_basic(station)