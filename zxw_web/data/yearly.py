# coding=utf-8
"""
Provides access to zxweather yearly data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date
import json
from cache import cache_control_headers
import os
import web
from web.contrib.template import render_jinja
from config import db
import config
from data.util import datetime_to_js_date, pretty_print, daily_records_result_to_datatable, daily_records_result_to_json

__author__ = 'David Goodwin'

### Data URLs:
# This file provides URLs to access raw data in json format.
#
# /data/{year}/datatable/daily_records.json
#       daily records for the year.

# TODO: round temperatures, etc.

class datatable_json:
    """
    Gets data for a particular month in Googles DataTable format.
    """

    def GET(self, station, year, dataset):
        """
        Handles DataTable JSON data sources.
        :param station: Station to get data for.
        :param year: Year to get data for.
        :param dataset: Dataset to get
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        int_year = int(year)

        # Make sure the year actually exists in the database before we go
        # any further.
        params = dict(year=int_year)
        recs = db.query("select 42 from sample where extract(year from time_stamp) = $year  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'daily_records':
            return get_daily_records_datatable(int_year)
        else:
            raise web.NotFound()

class data_json:
    """
    Gets data for a particular month in a generic JSON format..
    """

    def GET(self, station, year, dataset):
        """
        Handles JSON data sources.
        :param station: Station to get data for.
        :param year: Year to get data for.
        :param dataset: Dataset to get
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        int_year = int(year)

        # Make sure the year actually exists in the database before we go
        # any further.
        params = dict(year=int_year)
        recs = db.query("select 42 from sample where extract(year from time_stamp) = $year  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'daily_records':
            return get_daily_records_json(int_year)
        else:
            raise web.NotFound()

class index:
    """ Year level data sources"""
    def GET(self, station, year):
        """
        Gets a list of data sources available at the year level.
        :param station: Station to get data for
        :type station: string
        :param year: Year to get data for
        :type year: string
        :return: html view data.
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    group by year_stamp, month_stamp) as md where md.year_stamp = $year
    order by month_stamp asc""", dict(year=year))

        months = []

        for record in month_data:
            months.append(record.month_stamp)

        if not len(months):
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.yearly_data_index(months=months)

#
# Daily records (year-level)
#

def get_daily_records_data(year):
    """
    Gets daily records for the entire year.
    :param year: Year to get records for
    :return: daily records query data
    """
    params = dict(date = date(year,01,01))
    query_data = db.query("""select date_trunc('day', time_stamp)::date as time_stamp,
        max(temperature) as max_temp,
        min(temperature) as min_temp,
        max(relative_humidity) as max_humid,
        min(relative_humidity) as min_humid,
        max(absolute_pressure) as max_pressure,
        min(absolute_pressure) as min_pressure,
        sum(rainfall) as total_rainfall,
        max(average_wind_speed) as max_average_wind_speed,
        max(gust_wind_speed) as max_gust_wind_speed
    from sample
    where date_trunc('year',time_stamp) = date_trunc('year', $date)
    group by date_trunc('day', time_stamp)
    order by time_stamp asc""", params)
    return query_data

def get_daily_records_dataset(year,output_function):
    """
    Gets the daily records data set at the year level. It is converted to
    JSON format using the supplied output function.
    :param year: Year to get the data set for
    :param output_function: Output function to produce JSON data with.
    :return: JSON data.
    """

    query_data = get_daily_records_data(year)

    json_data, data_age = output_function(query_data)

    cache_control_headers(data_age,year)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_daily_records_datatable(year):
    """
    Gets records for each day in the year. Output is in DataTable JSON for
    the Google Visualisation API.
    :param year: Year to get records for.
    :type year: int
    :return: JSON data containing records for the year.
    """

    return get_daily_records_dataset(year, daily_records_result_to_datatable)

def get_daily_records_json(year):
    """
    Gets records for each day in the year. Output is a generic JSON format.
    :param year: Year to get records for.
    :type year: int
    :return: JSON data containing records for the year.
    """

    return get_daily_records_dataset(year,
                                     daily_records_result_to_json)