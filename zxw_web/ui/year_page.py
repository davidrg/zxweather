# coding=utf-8
"""
Handles pages at the year level.
"""
import web
from web.contrib.template import render_jinja
from config import db
import config
from months import month_name
from cache import year_cache_control
from database import year_exists, get_station_id, in_archive_mode, get_station_name, get_stations
from ui import get_nav_urls, make_station_switch_urls
import os
from ui import validate_request, html_file

__author__ = 'David Goodwin'


basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

def get_year_months(year, station_id):
    """
    Returns a list of all months in the specified year for which there
    exists data in the database
    :param year: The year to get months for
    :type year: integer
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: A list of months in the year that have data
    :type: [integer]
    :raise: web.NotFound() if there is no data for the year.
    """
    month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    where station_id = $station
    group by year_stamp, month_stamp) as md where md.year_stamp = $year
    order by month_stamp asc""", dict(year=year, station=station_id))


    if not len(month_data):
        # If there are no months in this year then there can't be any data.
        raise web.NotFound()

    months = []
    for month in month_data:
        the_month_name = month_name[month.month_stamp]
        months.append(the_month_name)

    return months

def get_yearly_records(year, station_id):
    """
    Gets the records for the year (data from the yearly_records view).
    :param year: Year to get records for.
    :type year: integer
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Records for the year.
    :raise: web.NotFound() if there is no data for the year.
    """
    params = dict(year=year, station=station_id)
    yearly_records = db.query("""SELECT year_stamp, total_rainfall, max_gust_wind_speed, max_gust_wind_speed_ts::timestamp,
   max_average_wind_speed, max_average_wind_speed_ts::timestamp, min_absolute_pressure,
   min_absolute_pressure_ts::timestamp, max_absolute_pressure, max_absolute_pressure_ts::timestamp,
   min_apparent_temperature, min_apparent_temperature_ts::timestamp, max_apparent_temperature,
   max_apparent_temperature_ts::timestamp, min_wind_chill, min_wind_chill_ts::timestamp,
   max_wind_chill, max_wind_chill_ts::timestamp, min_dew_point, min_dew_point_ts::timestamp,
   max_dew_point, max_dew_point_ts::timestamp, min_temperature, min_temperature_ts::timestamp,
   max_temperature, max_temperature_ts::timestamp, min_humidity, min_humidity_ts::timestamp,
   max_humidity, max_humidity_ts::timestamp
FROM yearly_records
WHERE year_stamp = $year
and station_id = $station""", params)

    if not len(yearly_records):
        # Bad url or something.
        raise web.NotFound()

    return yearly_records[0]

def get_year(ui,station, year):
    """
    Gives an overview for a year.
    :param ui: UI to use
    :type ui: str
    :param station: Station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param year: Page year
    :type year: integer
    :return: View data
    """
    current_location = '/s/' + station + '/' + str(year) + '/'

    station_id = get_station_id(station)

    class data:
        """ Data required by the view """
        this_year = year
        prev_url = None
        next_url = None

        year_stamp = year

        next_year = year + 1
        prev_year = year - 1

        # A list of months in the year that have data
        months = [(item,item.capitalize()) for item in get_year_months(year, station_id)]

        # Min/max values for the year.
        records = get_yearly_records(year, station_id)

    # See if any data exists for the previous and next months (no point
    # showing a navigation link if there is no data)
    if year_exists(data.prev_year, station_id):
        data.prev_url = '../' + str(data.prev_year) + '/'

    if year_exists(data.next_year, station_id):
        data.next_url = '../' + str(data.next_year) + '/'

    year_cache_control(year, station_id)

    if ui in ('s','m'):

        if ui == 'm':
            sub_dir = ''
        else:
            sub_dir = "datatable/"

        # TODO: Convert this into a dict
        # Figure out any URLs the page needs to know.
        class urls:
            """Various URLs required by the view"""
            root = '../../../'
            ui_root = '../../'
            data_base = root + 'data/{0}/{1}/'.format(station,year)
            daily_records = data_base + sub_dir + 'daily_records.json'
            prev_url = data.prev_url
            next_url = data.next_url

        nav_urls = get_nav_urls(station, current_location)

        page_data = {
            "station_name": get_station_name(station_id),
            "stations": make_station_switch_urls(get_stations(),
                                                 current_location)
        }

        return modern_templates.year(nav=nav_urls,data=data,urls=urls,
                                     sitename=config.site_name,
                                     ui=ui,
                                     archive_mode=in_archive_mode(station_id),
                                     page_data=page_data)
    else:
        return basic_templates.year(data=data)

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
        return get_year(ui, station, int(year))

