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
from database import year_exists, get_station_id, in_archive_mode, get_station_name, get_stations, get_station_message, \
    get_site_name, get_noaa_year_data
from ui import get_nav_urls, make_station_switch_urls, build_alternate_ui_urls
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
   min_sea_level_pressure, min_sea_level_pressure_ts::timestamp, max_sea_level_pressure, 
   max_sea_level_pressure_ts::timestamp,
   coalesce(min_sea_level_pressure, min_absolute_pressure) as min_pressure,
   coalesce(min_sea_level_pressure_ts, min_absolute_pressure_ts)::timestamp as min_pressure_ts,
   coalesce(max_sea_level_pressure, max_absolute_pressure) as max_pressure,
   coalesce(max_sea_level_pressure_ts, max_absolute_pressure_ts)::timestamp as max_pressure_ts,
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


def day_suffix(day):
    day = int(day)
    if 4 <= day <= 20 or 24 <= day <= 30:
        suffix = "th"
    else:
        suffix = ["st", "nd", "rd"][day % 10 - 1]
    return suffix


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

    if ui == 'm' or (ui == 'a' and config.disable_alt_ui):
        # user requested the disabled alt UI or the future 'm' UI. Send the user
        # to the standard UI instead.
        web.seeother(config.site_root + 's' + '/' + station + '/' +
                     str(year) + '/')

    current_location = '/*/' + station + '/' + str(year) + '/'

    station_id = get_station_id(station)

    class data:
        """ Data required by the view """
        this_year = year
        prev_url = None
        next_url = None

        wind_speed_kmh = config.wind_speed_kmh

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

    if ui == 'a':
        sub_dir = 'datatable/'
    else:
        sub_dir = ""

    # TODO: Convert this into a dict
    # Figure out any URLs the page needs to know.
    class urls:
        """Various URLs required by the view"""
        root = '../../../'
        ui_root = '../../'
        data_base = root + 'data/{0}/{1}/'.format(station, year)
        daily_records = data_base + sub_dir + 'daily_records.json'
        prev_url = data.prev_url
        next_url = data.next_url

    if ui in ('s', 'm', 'a'):
        nav_urls = get_nav_urls(station, current_location)

        def _switch_func(station_id):
            return year_exists(year, station_id)

        page_data = {
            "station_name": get_station_name(station_id),
            "stations": make_station_switch_urls(
                get_stations(), current_location, _switch_func,
                (year, ))
        }

        msg = get_station_message(station_id)

        return modern_templates.year(nav=nav_urls,data=data,urls=urls,
                                     sitename=get_site_name(station_id),
                                     ui=ui,
                                     alt_ui_disabled=config.disable_alt_ui,
                                     archive_mode=in_archive_mode(station_id),
                                     page_data=page_data,
                                     switch_url=build_alternate_ui_urls(
                                         current_location),
                                     station=station,
                                     station_message=msg[0],
                                     station_message_ts=msg[1],
                                     tracking_id=config.google_analytics_id
                                     )
    else:
        return basic_templates.year(data=data,
                                    urls=urls,
                                    alt_ui_disabled=config.disable_alt_ui,
                                    switch_url=build_alternate_ui_urls(
                                        current_location))


def get_year_summary(ui,station, year):
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

    if ui == 'm' or (ui == 'a' and config.disable_alt_ui):
        # user requested the disabled alt UI or the future 'm' UI. Send the user
        # to the standard UI instead.
        web.seeother(config.site_root + 's' + '/' + station + '/' +
                     str(year) + '/summary.html')

    current_location = '/*/' + station + '/' + str(year) + '/summary.html'

    station_id = get_station_id(station)

    class data:
        """ Data required by the view """
        this_year = year
        prev_url = None
        next_url = None

        year_stamp = year

        next_year = year + 1
        prev_year = year - 1

    # See if any data exists for the previous and next months (no point
    # showing a navigation link if there is no data)
    if year_exists(data.prev_year, station_id):
        data.prev_url = '../' + str(data.prev_year) + '/summary.html'

    if year_exists(data.next_year, station_id):
        data.next_url = '../' + str(data.next_year) + '/summary.html'

    year_cache_control(year, station_id)

    # TODO: Convert this into a dict
    # Figure out any URLs the page needs to know.
    class urls:
        """Various URLs required by the view"""
        root = '../../../'
        ui_root = '../../'
        prev_url = data.prev_url
        next_url = data.next_url

    if ui in ('s', 'm', 'a'):
        nav_urls = get_nav_urls(station, current_location)

        def _switch_func(station_id):
            return year_exists(year, station_id)

        page_data = {
            "station_name": get_station_name(station_id),
            "stations": make_station_switch_urls(
                get_stations(), current_location, _switch_func,
                (year, ))
        }

        msg = get_station_message(station_id)

        year_summary_data = get_noaa_year_data(
            station, year, False)

        monthly_data = None
        yearly_data = None
        summary_config = None
        if year_summary_data is not None:
            monthly_data, yearly_data, criteria_data = year_summary_data

            noaa_config = config.report_settings[station.lower()]["noaa_year"]

            summary_config = {
                "heatBase": noaa_config["heatBase"],
                "coolBase": noaa_config["coolBase"],
                "tempUnits": "C",
                "windUnits": "m/s",
                "rainUnits": "mm"
            }

            if noaa_config["fahrenheit"]:
                summary_config["tempUnits"] = "F"
            if noaa_config["kmh"]:
                summary_config["windUnits"] = "km/h"
            elif noaa_config["mph"]:
                summary_config["windUnits"] = "mph"
            if noaa_config["inches"]:
                summary_config["rainUnits"] = "inches"

            for i in range(0, len(monthly_data)):
                if monthly_data[i]["hi_temp_date"] is None:
                    continue
                monthly_data[i]["hi_temp_date"] += day_suffix(monthly_data[i]["hi_temp_date"])
                monthly_data[i]["low_temp_date"] += day_suffix(monthly_data[i]["low_temp_date"])
                monthly_data[i]["max_obs_rain_day"] += day_suffix(monthly_data[i]["max_obs_rain_day"])
                monthly_data[i]["high_wind_day"] += day_suffix(monthly_data[i]["high_wind_day"])
                monthly_data[i]["month"] = month_name[int(monthly_data[i]["month"])].capitalize()
        else:
            raise web.NotFound()

        return modern_templates.year_summary(nav=nav_urls,data=data,urls=urls,
                                     sitename=get_site_name(station_id),
                                     ui=ui,
                                     alt_ui_disabled=config.disable_alt_ui,
                                     archive_mode=in_archive_mode(station_id),
                                     page_data=page_data,
                                     switch_url=build_alternate_ui_urls(
                                         current_location),
                                     station=station,
                                     station_message=msg[0],
                                     station_message_ts=msg[1],
                                     tracking_id=config.google_analytics_id,
                                     monthly_data=monthly_data,
                                     yearly_data=yearly_data,
                                     summary_config=summary_config
                                     )


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


class year_summary:
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
        validate_request(ui, station, year)
        html_file()
        return get_year_summary(ui, station, int(year))



