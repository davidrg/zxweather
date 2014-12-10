#!/usr/bin/env python
# coding=utf-8
"""
This script is for sending data to the UK Met Office Weather Observations
Website (WOW).

The Met Office recommends the interval between submissions is at least 15
minutes. So the best way to run this script will be to:
    1. chmod +x this script
    2. copy wow_update.cfg into /etc and edit it to match your setup
    3. throw something like the following in /etc/crontab:
            0,15,30,45 * * * * /path/to/wow_update.py


This script will submit the following data:
    * Wind direction
    * Wind speed
    * Gust wind speed
    * Gust wind direction (Davis weather stations only)
    * Outdoor Humidity
    * Outdoor Dew point
    * Outdoor Temperature
    * Rainfall in last hour
    * Rainfall today
    * Station air pressure

In your site config page on the WOW site you should set your pressure settings
under Site Data Preferences to:
  Pressure (At Station): Hecto Pascal
  Mean Sea-Level Pressure: Not Captured
"""
from collections import OrderedDict
from urllib import urlencode
import psycopg2
from psycopg2.extras import NamedTupleCursor
import requests

__author__ = 'david'

_sw_type = "zxweather wow_update (http://www.sw.zx.net.nz/zxweather/)"
_submit_url = "http://wow.metoffice.gov.uk/automaticreading"
_submit_method = "POST"


class Config(object):
    """
    Handles reading the configuration file.
    """

    _config_file_locations = [
        'wow_update.cfg',
        '/etc/zxweather/wow_update.cfg',
        '/etc/wow_update.cfg'
    ]

    def __init__(self):
        import ConfigParser
        config = ConfigParser.ConfigParser()
        config.read(self._config_file_locations)

        self.aws_pin = config.get("site_info", "aws_pin")
        self.site_id = config.get("site_info", "site_id")

        self.station_code = config.get('station', 'station_code')

        # Read database config
        hostname = config.get('database', 'host')
        port = config.getint('database', 'port')
        database = config.get('database', 'database')
        user = config.get('database', 'user')
        pw = config.get('database', 'password')

        self._dsn = "host={host} port={port} user={user} password={password} " \
                   "dbname={name}".format(
            host=hostname,
            port=port,
            user=user,
            password=pw,
            name=database
        )

    def getDatabaseConnection(self):
        con = psycopg2.connect(self._dsn)
        return con


class Data(object):
    """
    Manages data to be submitted to WOW.
    """
    def __init__(self, data):
        self.winddir=data.wind_direction
        self.windspeedmph=self._ms_to_mph(data.average_wind_speed)
        self.windgustdir=data.gust_wind_direction
        self.windgustmph=self._ms_to_mph(data.gust_wind_speed)
        self.humidity=data.relative_humidity
        self.dewptf=self._c_to_f(data.dew_point)
        self.tempf=self._c_to_f(data.temperature)
        self.rainin=self._mm_to_inch(data.rainfall_60_minutes)
        self.dailyrainin=self._mm_to_inch(data.day_rainfall)
        self.baromin=self._mb_to_inhg(data.absolute_pressure)
        self.timestamp=data.time_stamp


    @staticmethod
    def _ms_to_mph(ms):
        """meters per second to miles per hour"""
        return (ms * 0.0006214) * 3600.0

    @staticmethod
    def _c_to_f(c):
        """Celsius to Farenheit"""
        return (c * 1.8) + 32

    @staticmethod
    def _mm_to_inch(mm):
        """millimeters to inches"""
        return mm / 25.4

    @staticmethod
    def _mb_to_inhg(mb):
        """ Convert hectopascals to inches of mercury """
        return (29.92 * mb) / 1013.25



def get_latest_data_for_station(con, station_code):
    query = """
select to_char(s.time_stamp at time zone 'gmt', 'YYYY-MM-DD HH:MI:SS') as time_stamp,
       s.wind_direction::integer,
       s.average_wind_speed,
       s.gust_wind_speed,
       ds.gust_wind_direction::integer,
       s.relative_humidity,
       s.dew_point,
       s.temperature,
       hour_rain.rainfall as rainfall_60_minutes,
       day_rain.rainfall as day_rainfall,
       s.absolute_pressure
from sample s
inner join station st on st.station_id = s.station_id
left outer join davis_sample ds on ds.sample_id = s.sample_id
inner join (
     select station_id,
            sum(rainfall) as rainfall
     from sample
     where time_stamp >= NOW() - '1 hour'::interval
     group by station_id) as hour_rain on hour_rain.station_id = s.station_id
inner join (
     select station_id,
            sum(rainfall) as rainfall
     from sample
     where time_stamp >= date_trunc('day', NOW())
     group by station_id) as day_rain on hour_rain.station_id = s.station_id
where st.code = %s
order by s.time_stamp desc
limit 1
    """

    cur = con.cursor(cursor_factory=NamedTupleCursor)
    cur.execute(query, (station_code,))

    result = cur.fetchone()

    if result is None:
        return None

    return Data(result)


def build_submit_url(site_id, aws_pin, data):
    url_data = OrderedDict(
        dateutc=data.timestamp,
        siteid=site_id,
        siteAuthenticationKey=aws_pin,
        softwaretype=_sw_type,
        baromin=data.baromin,
        winddir=data.winddir,
        windspeedmph=data.windspeedmph,
        windgustmph=data.windgustmph,
        humidity=data.humidity,
        dewptf=data.dewptf,
        tempf=data.tempf,
        rainin=data.rainin,
        dailyrainin=data.dailyrainin,

    )

    # Only davis hardware supplies this data this time
    if data.windgustdir is not None:
        url_data['windgustdir'] = data.windgustdir

    query_string = urlencode(url_data)

    return _submit_url + "?" + query_string


def submit_data(url):
    print("Submitting data: " + url)
    r = requests.post(url)

    if r.status_code == 200:
        print("success!")
        return

    print("Error code {0}\nResponse: {1}".format(r.status_code, r.text))


def main():
    config = Config()

    con = config.getDatabaseConnection()

    data = get_latest_data_for_station(con, config.station_code)

    con.close()

    if data == None:
        print("No data available at this time.")
        return

    url = build_submit_url(config.site_id, config.aws_pin, data)

    submit_data(url)

if __name__ == "__main__":
    main()