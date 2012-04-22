"""
Data sources.
"""

import json
import web
import config
from config import db
from data.util import rfcformat
from datetime import timedelta, datetime, date

__author__ = 'David Goodwin'

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
            current_data = db.query("""select timetz(download_timestamp) as time_stamp,
                        invalid_data, relative_humidity, temperature, dew_point,
                        wind_chill, apparent_temperature, absolute_pressure,
                        average_wind_speed, gust_wind_speed, wind_direction
                        from live_data""")[0]
            current_data_ts = current_data.time_stamp
        else:
            # Fetch the latest data for today
            current_data = db.query("""select timetz(time_stamp) as time_stamp, relative_humidity,
                        temperature,dew_point, wind_chill, apparent_temperature,
                        absolute_pressure, average_wind_speed, gust_wind_speed,
                        wind_direction, invalid_data
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
                  'wind_direction': data.wind_direction}

        if config.live_data_available:
            web.header('Cache-Control', 'max-age=' + str(48))
            web.header('Expires', rfcformat(data_ts + timedelta(0, 48)))
        else:
            web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
            web.header('Expires', rfcformat(data_ts + timedelta(0, config.sample_interval)))
        web.header('Last-Modified', rfcformat(data_ts))

        web.header('Content-Type', 'application/json')
        return json.dumps(result)