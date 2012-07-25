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
from database import year_exists
from ui import get_nav_urls
import os
from ui import validate_request, html_file

__author__ = 'David Goodwin'


basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

def get_year_months(year):
    """
    Returns a list of all months in the specified year for which there
    exists data in the database
    :param year: The year to get months for
    :type year: integer
    :return: A list of months in the year that have data
    :type: [integer]
    :raise: web.NotFound() if there is no data for the year.
    """
    month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    group by year_stamp, month_stamp) as md where md.year_stamp = $year
    order by month_stamp asc""", dict(year=year))


    if not len(month_data):
        # If there are no months in this year then there can't be any data.
        raise web.NotFound()

    months = []
    for month in month_data:
        the_month_name = month_name[month.month_stamp]
        months.append(the_month_name)

    return months

def get_yearly_records(year):
    """
    Gets the records for the year (data from the yearly_records view).
    :param year: Year to get records for.
    :type year: integer
    :return: Records for the year.
    :raise: web.NotFound() if there is no data for the year.
    """
    params = dict(year=year)
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
WHERE year_stamp = $year""", params)

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

    class data:
        """ Data required by the view """
        this_year = year
        prev_url = None
        next_url = None

        year_stamp = year

        next_year = year + 1
        prev_year = year - 1

        # A list of months in the year that have data
        months = [(item,item.capitalize()) for item in get_year_months(year)]

        # Min/max values for the year.
        records = get_yearly_records(year)

    # See if any data exists for the previous and next months (no point
    # showing a navigation link if there is no data)
    if year_exists(data.prev_year):
        data.prev_url = '../' + str(data.prev_year)

    if year_exists(data.next_year):
        data.next_url = '../' + str(data.next_year)

    year_cache_control(year)

    if ui in ('s','m'):
        # Figure out any URLs the page needs to know.
        class urls:
            """Various URLs required by the view"""
            root = '../../../'
            ui_root = '../../'
            data_base = root + 'data/{0}/{1}/'.format(station,year)
            daily_records = data_base + 'datatable/daily_records.json'
            prev_url = data.prev_url
            next_url = data.next_url

        nav_urls = get_nav_urls(station, current_location)
        return modern_templates.year(nav=nav_urls,data=data,urls=urls,
                                     sitename=config.site_name)
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

