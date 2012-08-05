# coding=utf-8
"""
Provides access to zxweather monthly data over HTTP in a number of formats.
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
from data.util import outdoor_sample_result_to_datatable, datetime_to_js_date, pretty_print, outdoor_sample_result_to_json, daily_records_result_to_datatable, daily_records_result_to_json

__author__ = 'David Goodwin'

### Data URLs:
# This file provides URLs to access raw data in json format.
#
# /data/{year}/{month}/datatable/samples.json
#       All samples for the month.
# /data/{year}/{month}/datatable/30m_avg_samples.json
#       30 minute averages for the month.
# /data/{year}/{month}/datatable/daily_records.json
#       daily records for the month.<b>

# TODO: handle gaps of an entire day in the daily records output

class datatable_json:
    """
    Gets data for a particular month in Googles DataTable format.
    """

    def GET(self, station, year, month, dataset):
        """
        Gets DataTable formatted JSON data.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: Month to get data for. Unlike in other areas of the site
                      this is not the month name but rather its number.
        :type month: str
        :param dataset: Dataset (file) to fetch.
        :type dataset: str
        :return: JSON File.
        :raise: web.notfound if the file doesn't exist.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year),int(month),1))
        recs = db.query("select 42 from sample where date(date_trunc('month',time_stamp)) = $date  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_month_samples_datatable(int_year,int_month)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_datatable(int_year,int_month)
        elif dataset == 'daily_records':
            return get_daily_records_datatable(int_year,int_month)
        else:
            raise web.NotFound()

class data_json:
    """
    Gets data for a particular month in a generic JSON format..
    """

    def GET(self, station, year, month, dataset):
        """
        Gets JSON-formatted data.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: Month to get data for. Unlike in other areas of the site
                      this is not the month name but rather its number.
        :type month: str
        :param dataset: Dataset (file) to fetch.
        :type dataset: str
        :return: JSON File.
        :raise: web.notfound if the file doesn't exist.
        """
        if station != config.default_station_name:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year),int(month),1))
        recs = db.query("select 42 from sample where date(date_trunc('month',time_stamp)) = $date  limit 1", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_month_samples_json(int_year,int_month)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_json(int_year,int_month)
        elif dataset == 'daily_records':
            return get_daily_records_json(int_year,int_month)
        else:
            raise web.NotFound()

class index:
    """Gets an index page for monthly data"""
    def GET(self, station, year, month):
        """
        Gets an index page for data sources available at the month level.
        :param station: Station to get data for.
        :type station: string
        :param year: Year to get data for
        :type year: string
        :param month: Month to get data for
        :type month: string
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                         os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp,
           extract(day from time_stamp) as day_stamp
    from sample
    group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
    order by day_stamp""", dict(year=year, month=month))

        days = []

        for day in month_data:
            day = int(day.day_stamp)
            days.append(day)

        if not len(days):
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.monthly_data_index(days=days)

#
# Daily records
#

def get_daily_records_data(year,month):
    """
    Gets daily records data.
    :param year: Year to get records for
    :param month: Month to get records for
    :return: Query data
    """
    params = dict(date = date(year,month,01))
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
    where date_trunc('month', time_stamp) = date_trunc('month', $date)
    group by date_trunc('day', time_stamp)
    order by time_stamp asc""", params)

    return query_data

def get_daily_records_dataset(year,month,output_function):
    """
    Gets a daily records dataset at the month level. It is converted to
    JSON format using the supplied output function.

    :param year: Data set year
    :param month: Data set month
    :param output_function: Function to produce JSON output
    :return:
    """

    query_data = get_daily_records_data(year,month)

    json_data, data_age = output_function(query_data)

    cache_control_headers(data_age,year,month)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data

def get_daily_records_datatable(year,month):
    """
    Gets records for each day in the month.
    :param year: Year to get records for
    :type year: int
    :param month: Month to get records for
    :type month: int
    :return: JSON data containing the records. Structure is Google DataTable.
    """

    return get_daily_records_dataset(year,
                                     month,
                                     daily_records_result_to_datatable)

def get_daily_records_json(year,month):
    """
    Gets daily records for the entire month in a generic JSON format.
    :param year: Year to get records for.
    :param month: Month to get records for.
    :return:
    """

    return get_daily_records_dataset(year,
                                     month,
                                     daily_records_result_to_json)

#
# 30-minute averaged monthly samples
#

def get_30m_avg_month_samples_data(year,month):
    """
    Gets query data for 30-minute averaged data sets (month level).
    :param year: Data set year
    :param month: Data set month
    :return: Query data
    """
    params = dict(date = date(year,month,01))
    query_data = db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       avg(iq.gust_wind_speed) as gust_wind_speed
from (
        select cur.time_stamp,
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
                  true
               else
                  false
               end as gap,
               cur.average_wind_speed,
               cur.gust_wind_speed
        from sample cur, sample prev
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',$date))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
where date_trunc('month',iq.time_stamp) = date_trunc('month', $date)
group by iq.quadrant
order by iq.quadrant asc""", params)

    return query_data

def get_30m_avg_month_samples_dataset(year,month,output_function):
    """
    Gets a 30-minute averaged monthly samples data set. The data is
    transformed to JSON using the supplied output function.

    :param year: Data set year
    :param month: Data set month
    :param output_function: Function to use to generate JSON output.
    :return: JSON output.
    """

    query_data = get_30m_avg_month_samples_data(year,month)

    data, data_age = output_function(query_data)

    cache_control_headers(data_age,year,month)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_30m_avg_month_samples_datatable(year, month):
    """
    Gets outdoor samples for the month as 30-minute averages. Output format
    is Google Visualisation API DataTable JSON.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :return: JSON data containing 30 minute averaged samples for the month.
    Structure is Googles DataTable format.
    :rtype: str
    """

    return get_30m_avg_month_samples_dataset(year,
                                             month,
                                             outdoor_sample_result_to_datatable)

def get_30m_avg_month_samples_json(year,month):
    """
    Gets outdoor samples for the month as 30-minute averages.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :return: Generic JSON data containing 30 minute averaged samples for the month.
    :rtype: str
    """
    return get_30m_avg_month_samples_dataset(year,
                                             month,
                                             outdoor_sample_result_to_json)

#
# Monthly samples (full data set)
#

def get_month_samples_dataset(year,month,output_function):
    """
    Gets samples for the entire month in Googles DataTable format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param output_function: Function to produce JSON output
    :return: JSON data using Googles DataTable structure.
    :rtype: str
    """
    params = dict(date = date(year,month,01))
    query_data = db.query("""select cur.time_stamp,
       cur.temperature,
       cur.dew_point,
       cur.apparent_temperature,
       cur.wind_chill,
       cur.relative_humidity,
       cur.absolute_pressure,
       cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap,
       cur.average_wind_speed,
       cur.gust_wind_speed
from sample cur, sample prev
where date(date_trunc('month',cur.time_stamp)) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
order by cur.time_stamp asc""", params)

    data, data_age = output_function(query_data)

    cache_control_headers(data_age,year,month)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_month_samples_datatable(year,month):
    """
    Gets samples for the entire month in Googles DataTable format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :return: JSON data using Googles DataTable structure.
    :rtype: str
    """
    return get_month_samples_dataset(year,month,outdoor_sample_result_to_datatable)

def get_month_samples_json(year,month):
    """
    Gets samples for the entire month in a generic JSON format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :return: JSON data.
    :rtype: str
    """
    return get_month_samples_dataset(year,month,outdoor_sample_result_to_json)