"""
Data sources.
"""

import json
import os
import web
from web.contrib.template import render_jinja
import config
from config import db
from data.util import rfcformat
from datetime import timedelta, datetime, date, time

__author__ = 'David Goodwin'

month_name = {1 : 'january',
              2 : 'february',
              3 : 'march',
              4 : 'april',
              5 : 'may',
              6 : 'june',
              7 : 'july',
              8 : 'august',
              9 : 'september',
              10: 'october',
              11: 'november',
              12: 'december'}

def get_years():
    """
    Gets a list of years in the database.
    :return: A list of years with data in the database
    :rtype: [integer]
    """
    years_result = db.query("select distinct extract(year from time_stamp) as year from sample order by year desc")
    years = []
    for record in years_result:
        years.append(int(record.year))

    return years

class station_index:
    """ station level data sources"""
    def GET(self, station):
        """
        Gets a list of data sources available at the station level.
        :param station: Station to get data for
        :type station: string
        :return: html view data.
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        if station != config.default_station_name:
            raise web.NotFound()

        years = get_years()

        web.header('Content-Type', 'text/html')
        return render.station_data_index(years=years)

class live_data:
    """
    Gets live data.
    """

    @staticmethod
    def get_live_data():
        """
        Gets live data from the database. If config.live_data_available is set
        then the data will come from the live data table and will be at most
        48 seconds old. If it is not set then the data returned will be the
        most recent sample from the sample table.

        :return: Timestamp for the data and the actual data.
        """

        now = datetime.now()
        params = dict(date=date(now.year, now.month, now.day))

        if config.live_data_available:
            # No need to filter or anything - live_data only contains one record.
            current_data = db.query("""select download_timestamp::time as time_stamp,
                        invalid_data, relative_humidity, temperature, dew_point,
                        wind_chill, apparent_temperature, absolute_pressure,
                        average_wind_speed, gust_wind_speed, wind_direction,
                        extract('epoch' from (now() - download_timestamp)) as age
                        from live_data""")[0]
            current_data_ts = current_data.time_stamp
        else:
            # Fetch the latest data for today
            current_data = db.query("""select time_stamp::time as time_stamp, relative_humidity,
                        temperature,dew_point, wind_chill, apparent_temperature,
                        absolute_pressure, average_wind_speed, gust_wind_speed,
                        wind_direction, invalid_data,
                        extract('epoch' from (now() - download_timestamp)) as age
                    from sample
                    where date(time_stamp) = $date
                    order by time_stamp desc
                    limit 1""",params)[0]
            current_data_ts = current_data.time_stamp

        return current_data_ts, current_data

    def GET(self, station):
        """
        Provides live data (/data/live.json)
        :param station: Station to get live data from. Not currently used.
        :type station: string
        """

        if station != config.default_station_name:
            raise web.NotFound()

        data_ts, data = live_data.get_live_data()

        now = datetime.now()
        data_ts = datetime.combine(now.date(), data_ts)

        result = {'relative_humidity': data.relative_humidity,
                  'temperature': data.temperature,
                  'dew_point': data.dew_point,
                  'wind_chill': data.wind_chill,
                  'apparent_temperature': data.apparent_temperature,
                  'absolute_pressure': data.absolute_pressure,
                  'average_wind_speed': data.average_wind_speed,
                  'gust_wind_speed': data.gust_wind_speed,
                  'wind_direction': data.wind_direction,
                  'time_stamp': str(data.time_stamp),
                  'age': data.age,
                  }



        if config.live_data_available:
            web.header('Cache-Control', 'max-age=' + str(48))
            web.header('Expires', rfcformat(data_ts + timedelta(0, 48)))
        else:
            web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
            web.header('Expires', rfcformat(data_ts + timedelta(0, config.sample_interval)))
        web.header('Last-Modified', rfcformat(data_ts))

        web.header('Content-Type', 'application/json')
        return json.dumps(result)


class day_nav:
    def GET(self):
        now = datetime.now().date()
        yesterday = now - timedelta(1)

        data = {
            'yesterday': str(yesterday.year) + '/' + month_name[yesterday.month] + '/' + str(yesterday.day) + '/',
            'this_month': str(now.year) + '/' + month_name[now.month] + '/',
            'this_year': str(now.year) + '/',
            'site_name': config.site_name,
        }

        tomorrow = now + timedelta(1)
        tomorrow = datetime(year=tomorrow.year,
                            month=tomorrow.month,
                            day=tomorrow.day,
                            hour=0,minute=0,second=0)
        today = datetime(year=now.year,
                         month=now.month,
                         day=now.day,
                         hour=0,minute=0,second=0)
        web.header('Last-Modified', rfcformat(today))
        web.header('Expires', rfcformat(tomorrow))
        web.header('Content-Type', 'application/json')
        return json.dumps(data)