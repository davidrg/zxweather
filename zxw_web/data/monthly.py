# coding=utf-8
"""
Provides access to zxweather monthly data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""
import chevron
from datetime import date, datetime, timedelta
from cache import cache_control_headers, rfcformat
import os
import web
from web.contrib.template import render_jinja
from config import db
from data.util import outdoor_sample_result_to_datatable, outdoor_sample_result_to_json, \
    daily_records_result_to_datatable, daily_records_result_to_json
from database import get_station_id, get_sample_interval, \
     get_month_data_wp_age, get_extra_sensors_enabled, get_noaa_month_data, get_station_config

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

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year), int(month), 1), station=station_id)
        recs = db.query(
            """select 42 from sample
            where date(date_trunc('month',time_stamp)) = $date
            and station_id = $station
            limit 1""", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_month_samples_datatable(int_year, int_month, station_id)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_datatable(int_year, int_month, station_id)
        elif dataset == 'daily_records':
            return get_daily_records_datatable(int_year, int_month, station_id)
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

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year), int(month), 1), station=station_id)
        recs = db.query("""select 42 from sample
        where date(date_trunc('month',time_stamp)) = $date
        and station_id = $station
        limit 1""", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            return get_month_samples_json(int_year, int_month, station_id)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_json(int_year, int_month, station_id)
        elif dataset == 'daily_records':
            return get_daily_records_json(int_year, int_month, station_id)
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

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp,
           extract(day from time_stamp) as day_stamp
    from sample
    where station_id = $station
    group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
    order by day_stamp""", dict(year=year, month=month, station=station_id))

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

def get_daily_records_data(year, month, station_id):
    """
    Gets daily records data.
    :param year: Year to get records for
    :param month: Month to get records for
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Query data
    """
    params = dict(date=date(year, month, 01), station=station_id)
    query_data = db.query("""select date_trunc('day', time_stamp)::date as time_stamp,
        max(temperature) as max_temp,
        min(temperature) as min_temp,
        max(relative_humidity) as max_humid,
        min(relative_humidity) as min_humid,
        max(coalesce(mean_sea_level_pressure, absolute_pressure)) as max_pressure,
        min(coalesce(mean_sea_level_pressure, absolute_pressure)) as min_pressure,
        sum(rainfall) as total_rainfall,
        max(average_wind_speed) as max_average_wind_speed,
        max(gust_wind_speed) as max_gust_wind_speed
    from sample
    where date_trunc('month', time_stamp) = date_trunc('month', $date)
    and station_id = $station
    group by date_trunc('day', time_stamp)
    order by time_stamp asc""", params)

    return query_data


def get_daily_records_dataset(year, month, output_function, station_id):
    """
    Gets a daily records dataset at the month level. It is converted to
    JSON format using the supplied output function.

    :param year: Data set year
    :param month: Data set month
    :param output_function: Function to produce JSON output
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return:
    """

    query_data = get_daily_records_data(year, month, station_id)

    json_data, data_age = output_function(query_data)

    cache_control_headers(station_id, data_age, year, month)
    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(json_data)))
    return json_data


def get_daily_records_datatable(year, month, station_id):
    """
    Gets records for each day in the month.
    :param year: Year to get records for
    :type year: int
    :param month: Month to get records for
    :type month: int
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data containing the records. Structure is Google DataTable.
    """

    return get_daily_records_dataset(year,
                                     month,
                                     daily_records_result_to_datatable,
                                     station_id)


def get_daily_records_json(year, month, station_id):
    """
    Gets daily records for the entire month in a generic JSON format.
    :param year: Year to get records for.
    :param month: Month to get records for.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return:
    """

    return get_daily_records_dataset(year,
                                     month,
                                     daily_records_result_to_json,
                                     station_id)


#
# 30-minute averaged monthly samples
#

def get_30m_avg_month_samples_data(year, month, station_id):
    """
    Gets query data for 30-minute averaged data sets (month level).
    :param year: Data set year
    :param month: Data set month
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Query data
    """
    params = dict(date=date(year, month, 1), station=station_id)
    query_data = db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       avg(mean_sea_level_puressure) as mean_sea_level_pressure,
       avg(pressure) as pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap,
       avg(iq.average_wind_speed) as average_wind_speed,
       max(iq.gust_wind_speed) as gust_wind_speed,
       avg(iq.uv_index)::real as uv_index,
       avg(iq.solar_radiation)::real as solar_radiation
from (
        select cur.time_stamp,
               --(extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               (extract(epoch from cur.time_stamp::date) + date_part('hour', cur.time_stamp)::int) * 10 + date_part('minute', cur.time_stamp)::int / 30 as quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.mean_sea_level_pressure,
               coalesce(cur.mean_sea_level_pressure, cur.absolute_pressure) as pressure,
               cur.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
                  true
               else
                  false
               end as gap,
               cur.average_wind_speed,
               cur.gust_wind_speed,
               ds.average_uv_index as uv_index,
               ds.solar_radiation
        from sample cur
        inner join sample prev on prev.station_id = cur.station_id and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp and station_id = $station)
        inner join station st on st.station_id = prev.station_id
        left outer join davis_sample ds on ds.sample_id = cur.sample_id
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',$date))
          and cur.station_id = $station
        order by cur.time_stamp asc) as iq
where date_trunc('month',iq.time_stamp) = date_trunc('month', $date)
group by iq.quadrant
order by iq.quadrant asc""", params)

    return query_data


def get_30m_avg_month_samples_dataset(year, month, output_function, station_id):
    """
    Gets a 30-minute averaged monthly samples data set. The data is
    transformed to JSON using the supplied output function.

    :param year: Data set year
    :param month: Data set month
    :param output_function: Function to use to generate JSON output.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON output.
    """

    query_data = get_30m_avg_month_samples_data(year, month, station_id)

    data, data_age = output_function(query_data)

    cache_control_headers(station_id, data_age, year, month)

    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(data)))
    return data


def get_30m_avg_month_samples_datatable(year, month, station_id):
    """
    Gets outdoor samples for the month as 30-minute averages. Output format
    is Google Visualisation API DataTable JSON.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data containing 30 minute averaged samples for the month.
    Structure is Googles DataTable format.
    :rtype: str
    """

    return get_30m_avg_month_samples_dataset(year,
                                             month,
                                             outdoor_sample_result_to_datatable,
                                             station_id)


def get_30m_avg_month_samples_json(year, month, station_id):
    """
    Gets outdoor samples for the month as 30-minute averages.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: Generic JSON data containing 30 minute averaged samples for the month.
    :rtype: str
    """
    return get_30m_avg_month_samples_dataset(year,
                                             month,
                                             outdoor_sample_result_to_json,
                                             station_id)


#
# Monthly samples (full data set)
#

def get_month_samples_dataset(year, month, output_function, station_id):
    """
    Gets samples for the entire month in Googles DataTable format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param output_function: Function to produce JSON output
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data using Googles DataTable structure.
    :rtype: str
    """
    params = dict(date=date(year, month, 01), station=station_id,
                  sample_interval=get_sample_interval(station_id))
    query_data = db.query("""select cur.time_stamp,
       cur.temperature,
       cur.dew_point,
       cur.apparent_temperature,
       cur.wind_chill,
       cur.relative_humidity,
       cur.absolute_pressure,
       cur.mean_sea_level_pressure,
       coalesce(cur.mean_sea_level_pressure, cur.absolute_pressure) as pressure,
       cur.time_stamp - (st.sample_interval * '1 second'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 second'::interval) THEN
          true
       else
          false
       end as gap,
       cur.average_wind_speed,
       cur.gust_wind_speed,
       ds.average_uv_index as uv_index,
       ds.solar_radiation
from sample cur, davis_sample ds, sample prev
inner join station st on st.station_id = prev.station_id
where date(date_trunc('month',cur.time_stamp)) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp and station_id = $station)
  and ds.sample_id = cur.sample_id
  and cur.station_id = $station
  and prev.station_id = $station
order by cur.time_stamp asc""", params)

    data, data_age = output_function(query_data)

    cache_control_headers(station_id, data_age, year, month)

    web.header('Content-Type', 'application/json')
    web.header('Content-Length', str(len(data)))
    return data


def get_month_samples_datatable(year, month, station_id):
    """
    Gets samples for the entire month in Googles DataTable format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data using Googles DataTable structure.
    :rtype: str
    """
    return get_month_samples_dataset(year,
                                     month,
                                     outdoor_sample_result_to_datatable,
                                     station_id)


def get_month_samples_json(year, month, station_id):
    """
    Gets samples for the entire month in a generic JSON format.
    :param year: Year to get data for
    :type year: int
    :param month: Month to get data for
    :type month: int
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data.
    :rtype: str
    """
    return get_month_samples_dataset(year,
                                     month,
                                     outdoor_sample_result_to_json,
                                     station_id)


class data_ascii:
    def GET(self, station, year, month, dataset):
        """
        Gets plain text data.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: Month to get data for. Unlike in other areas of the site
                      this is not the month name but rather its number.
        :type month: str
        :param dataset: Dataset (file) to fetch.
        :type dataset: str
        :return: text file.
        :raise: web.notfound if the file doesn't exist.
        """

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year), int(month), 1), station=station_id)
        recs = db.query("""select 42 from sample
        where date(date_trunc('month',time_stamp)) = $date
        and station_id = $station
        limit 1""", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == "noaamo":
            return self.noaa_month(station, int_year, int_month)

        raise web.NotFound()

        # if dataset == 'samples':
        #     result, age = get_month_samples_tab_delimited(int_year, int_month,
        #                                                   station_id)
        #     cache_control_headers(station_id, age, int_year, int_month)
        # else:
        #     raise web.NotFound()
        #
        # web.header("Content-Type", "text/plain")
        # return result

    def noaa_month(self, station, year, month):
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))

        data = get_noaa_month_data(station, year, month)

        if data is None:
            raise web.NotFound()

        monthly, daily_data, criteria = data

        report_criteria = {
            'daily': daily_data,
            'month': monthly,
            'criteria': criteria
        }

        with open(os.path.join(template_dir, 'noaamo.txt'), 'r') as f:
            result = chevron.render(f, report_criteria)

        web.header('Content-Type', 'text/plain; charset=utf-8')
        web.header('Content-Length', str(len(result)))
        return result


class data_dat:
    def GET(self, station, year, month, dataset):
        """
        Gets plain text data.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: Month to get data for. Unlike in other areas of the site
                      this is not the month name but rather its number.
        :type month: str
        :param dataset: Dataset (file) to fetch.
        :type dataset: str
        :return: text file.
        :raise: web.notfound if the file doesn't exist.
        """

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        # Make sure the month actually exists in the database before we go
        # any further.
        params = dict(date=date(int(year), int(month), 1), station=station_id)
        recs = db.query("""select 42 from sample
        where date(date_trunc('month',time_stamp)) = $date
        and station_id = $station
        limit 1""", params)
        if recs is None or len(recs) == 0:
            raise web.NotFound()

        if dataset == 'samples':
            result, age = get_month_samples_tab_delimited(int_year, int_month,
                                                          station_id, 1)
            cache_control_headers(station_id, age, int_year, int_month)
        elif dataset == 'samples_v2':
            result, age = get_month_samples_tab_delimited(int_year, int_month,
                                                          station_id, 2)
            cache_control_headers(station_id, age, int_year, int_month)
        else:
            raise web.NotFound()

        web.header("Content-Type", "text/plain")
        return result

    def HEAD(self, station, year, month, dataset):
        """
        Gets headers for plain text data. Its primarily for the benefit of the
        desktop UI which checks the cache control headers before trying to
        download the files.

        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: Month to get data for. Unlike in other areas of the site
                      this is not the month name but rather its number.
        :type month: str
        :param dataset: Dataset (file) to fetch.
        :type dataset: str
        :return: text file.
        :raise: web.notfound if the file doesn't exist.
        """

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        int_year = int(year)
        int_month = int(month)

        if dataset == 'samples':
            # TODO: this really needs to not involve the database in any way.
            age = get_month_data_wp_age(int_year, int_month, station_id)

            if age is None:
                raise web.NotFound()

            now = datetime.now()

            web.header('Last-Modified', rfcformat(age))
            if int_year == now.year and int_month == now.month:
                # TODO: look up sample interval and use that. Its what
                # cache_control_headers() does but we can't currently use
                # that as the sample interval isn't cached and looking it up
                # on every request is too expensive here.
                pass
            else:
                web.header('Expires', rfcformat(now + timedelta(60, 0)))

            # cache_control_headers(station_id, age, int_year, int_month)
        else:
            raise web.NotFound()

        web.header("Content-Type", "text/plain")
        return


def get_month_samples_tab_delimited(int_year, int_month, station_id, version):
    """
    Gets a tab-delimited data file containing all data for the requested month.
    It is available in two versions - version 1 includes median sea level
    pressure if available, otherwise absolute pressure. Version 2 includes both.

    :param int_year: Year
    :type int_year: int
    :param int_month: Month
    :type int_month: int
    :param station_id: Station to get data for
    :type station_id: int
    :param version: File version
    :type version: int
    """
    station_config = get_station_config(station_id)
    broadcast_id = None
    if station_config is not None and 'broadcast_id' in station_config:
        broadcast_id = station_config['broadcast_id']

    extra_sensors = get_extra_sensors_enabled(station_id)

    results = db.query("""
    select max(sample_ts) as max_ts, 
           string_agg(sample_data, chr(10)) as file_data 
    from month_samples_tsv($station, $month, $broadcast_id, $version, $extra_sensors)""",
                       dict(station=station_id, month=date(year=int_year, month=int_month, day=1),
                            broadcast_id=broadcast_id, version=version, extra_sensors=extra_sensors))

    row = results[0]
    max_ts = row.max_ts
    file_data = row.file_data

    return file_data, max_ts
