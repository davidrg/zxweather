# coding=utf-8
"""
Controllers for month pages.
"""
from datetime import date
import web
from web.contrib.template import render_jinja
from config import db
import config
from months import month_name, month_number
from cache import month_cache_control
from database import month_exists, get_station_id, in_archive_mode, get_station_name, get_stations, get_station_message
from ui import get_nav_urls, make_station_switch_urls, build_alternate_ui_urls
import os
from ui import validate_request, html_file
from url_util import relative_url

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')


def get_previous_month(year,month):
    """
    Calculates the year and month that comes before the year and month
    parameters. The returned "previous year" will be year-1 if month==1,
    otherwise it will equal the year parameter.

    :param year: Year
    :type year: int
    :param month: Month
    :type month: int
    :return: Previous year, Previous month
    :rtype: int,int
    """
    previous_month = month - 1
    previous_year = year

    if not previous_month:
        previous_month = 12
        previous_year -= 1

    return previous_year,previous_month

def get_next_month(year,month):
    """
    Calculates the year and month that comes after the year and month
    parameters. The returned "next year" will be year+1 if month==12,
    otherwise it will equal the year parameter.

    :param year: Year
    :type year: int
    :param month: Month
    :type month: int
    :return: Next year, Next month
    :rtype: int,int
    """

    next_month = month + 1
    next_year = year

    if next_month > 12:
        next_month = 1
        next_year += 1

    return next_year, next_month

def month_nav_urls(ui,station,year,month, current_location):
    """
    Gets navigation URLs for the month page.
    :param ui: UI the page lives under.
    :type ui: str
    :param station: Station the page lives under.
    :type station: str or unicode
    :param year: Year that is being viewed.
    :type year: int
    :param month: Month that is being viewed.
    :type month: int
    :param current_location: Absolute URL for the current location.
    :type current_location: str
    :return: previous month url, next month url
    :rtype: str,str
    """

    station_id = get_station_id(station)

    previous_year, previous_month = get_previous_month(year,month)
    next_year, next_month = get_next_month(year,month)

    prev_url = ""
    next_url = ""

    url_format_string = '/{0}/{1}/{2}/{3}/'

    if month_exists(previous_year, previous_month, station_id):
        abs_url = url_format_string.format(ui,
                                           station,
                                           previous_year,
                                           month_name[previous_month])
        prev_url = relative_url(current_location, abs_url)

    if month_exists(next_year, next_month, station_id):
        abs_url = url_format_string.format(ui,
                                           station,
                                           next_year,
                                           month_name[next_month])
        next_url = relative_url(current_location, abs_url)

    return prev_url, next_url

def get_month_days(year, month, station_id):
    """
    Returns a list of all days in the specified month for which there
    exists data in the database.

    :param year: Months year
    :type year: integer
    :param month: Month
    :type month: integer
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: A list of days in the month that have data
    :type: [integer]
    """
    month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
       extract(month from time_stamp) as month_stamp,
       extract(day from time_stamp) as day_stamp
from sample
where station_id = $station
group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
order by day_stamp""", dict(year=year, month=month, station=station_id))

    if not len(month_data):
        return None

    days = []
    for day in month_data:
        day = int(day.day_stamp)
        days.append(day)

    return days

def get_monthly_records(year, month, station_id):
    """
    Gets the records for the month (data from the monthly_records view).
    :param year: Year to get records for.
    :type year: integer
    :param month:  Month to get records for
    :type month: integer
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Records for the year.
    :raise: web.NotFound() if there is no data for the year/month.
    """

    params = dict(date=date(year,month,01), station=station_id)
    monthly_records = db.query("""SELECT date_stamp, total_rainfall, max_gust_wind_speed, max_gust_wind_speed_ts::timestamp,
   max_average_wind_speed, max_average_wind_speed_ts::timestamp, min_absolute_pressure,
   min_absolute_pressure_ts::timestamp, max_absolute_pressure, max_absolute_pressure_ts::timestamp,
   min_apparent_temperature, min_apparent_temperature_ts::timestamp, max_apparent_temperature,
   max_apparent_temperature_ts::timestamp, min_wind_chill, min_wind_chill_ts::timestamp,
   max_wind_chill, max_wind_chill_ts::timestamp, min_dew_point, min_dew_point_ts::timestamp,
   max_dew_point, max_dew_point_ts::timestamp, min_temperature, min_temperature_ts::timestamp,
   max_temperature, max_temperature_ts::timestamp, min_humidity, min_humidity_ts::timestamp,
   max_humidity, max_humidity_ts::timestamp
FROM monthly_records
WHERE date_stamp = $date
and station_id = $station""", params)

    if not len(monthly_records):
        # Bad url or something.
        raise web.NotFound()

    return monthly_records[0]

def get_month(ui, station, year, month):
    """
    Gives an overview for a month.
    :param ui: UI to fetch
    :type ui: str
    :param station: Station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param year: Page year
    :type year: integer
    :param month: Page month
    :type month: integer
    :return: View data
    """

    if ui == 'm':
        # 'm' interface doesn't exist right now. Send the user to the
        # standard UI instead.
        web.seeother(config.site_root + 's' + '/' + station + '/' +
                     str(year) + '/' + month_name[month] + '/')

    current_location = '/*/' + station + '/' + str(year) + '/' +\
                       month_name[month] + '/'

    station_id = get_station_id(station)

    previous_year, previous_month = get_previous_month(year,month)
    the_next_year, the_next_month = get_next_month(year,month)

    # TODO: Make this a dict sometime.
    class data:
        """ Data required by the view """
        year_stamp = year
        month_stamp = month
        current_data = None
        this_year = year

        prev_url, next_url = month_nav_urls(ui,
                                            station,
                                            year,
                                            month,
                                            current_location)

        next_month = month_name[the_next_month].capitalize()
        next_year = the_next_year

        prev_month = month_name[previous_month].capitalize()
        prev_year = previous_year

        this_month = month_name[month].capitalize()

        # List of days in the month that have data
        days = get_month_days(year,month, station_id)
        if days is None:
            # No days in this month? Can't be any month then.
            raise web.NotFound()

        # Min/Max values for the month
        records = get_monthly_records(year,month, station_id)

    month_cache_control(year, month, station_id)
    if ui in ('s','m', 'a'):
        if ui == 'a':
            sub_dir = "datatable/"
        else:
            sub_dir = ""

        # TODO: Make this a dict.
        class urls:
            """Various URLs required by the view"""
            root = '../../../../'
            data_base = root + 'data/{0}/{1}/{2}/'.format(station,year,month)
            samples = data_base + sub_dir + 'samples.json'
            samples_30m_avg = data_base + sub_dir + '30m_avg_samples.json'
            daily_records = data_base + sub_dir + 'daily_records.json'

        nav_urls = get_nav_urls(station, current_location)

        def _switch_func(station_id):
            return month_exists(year, month, station_id)

        page_data = {
            "station_name": get_station_name(station_id),
            "stations": make_station_switch_urls(
                get_stations(), current_location, _switch_func,
                (year, month))
        }

        msg = get_station_message(station_id)

        return modern_templates.month(nav=nav_urls, data=data,dataurls=urls,
                                      ui=ui, sitename=config.site_name,
                                      archive_mode=in_archive_mode(station_id),
                                      page_data=page_data,
                                      switch_url=build_alternate_ui_urls(
                                          current_location),
                                      station=station,
                                      station_message=msg[0],
                                      station_message_ts=msg[1])
    else:
        return basic_templates.month(data=data,
                                     switch_url=build_alternate_ui_urls(
                                         current_location))


class month:
    """
    Gives an overview for a month
    """
    def GET(self, ui, station, year, month):
        """
        Gets the month page (index.html) after doing a bit of validation.
        :param ui: UI variant to get.
        :type ui: str
        :param station: Station to fetch data for.
        :type station: str
        :param year: Year to fetch
        :type year: str
        :param month: Month name (in lowercase) to fetch.
        :type month: str
        :return: HTML data for the page.
        :rtype: str
        """
        validate_request(ui,station,year,month)
        html_file()
        return get_month(ui, station, int(year), month_number[month])
