# coding=utf-8
"""
Data sources at the station level.
"""
import mimetypes
from datetime import datetime
import json
import web
from web.contrib.template import render_jinja
from cache import live_data_cache_control
import config
from data import daily, about_nav
from data.daily import get_24hr_samples_data, get_day_rainfall, get_day_dataset, get_24hr_hourly_rainfall_data, \
    get_24hr_reception, get_168hr_reception
from data.util import outdoor_sample_result_to_json, outdoor_sample_result_to_datatable, rainfall_sample_result_to_json, rainfall_to_datatable, \
    reception_result_to_json, reception_result_to_datatable
from database import get_years, get_live_data, get_station_id, get_latest_sample_timestamp, get_oldest_sample_timestamp, \
    get_station_type_code, get_station_config, get_image_sources_for_station, \
    get_image_source, get_day_images_for_source, \
    get_most_recent_image_id_for_source
import os

__author__ = 'David Goodwin'


class index:
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

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        years = get_years(station_id)

        reception_available = False
        hw_type = get_station_type_code(station_id)
        if hw_type in ['DAVIS']:
            reception_available = True

        # See if we have any images for this station
        sources = get_image_sources_for_station(station_id)

        web.header('Content-Type', 'text/html')
        return render.station_data_index(
            years=years, reception_available=reception_available,
            image_sources=sources)


class data_json:
    """
    JSON data sources at the station level
    """
    def GET(self, station, dataset):
        """
        Handles station-level JSON data sources.
        :param station: Station to get data for
        :param dataset: Dataset to get.
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        hw_type = get_station_type_code(station_id)

        hw_config = get_station_config(station_id)

        pass_through_data_sets = {
            'current_day_records':'records',
            'current_day_rainfall_totals':'rainfall',
            'current_day_samples':'samples',
            'current_day_7day_30m_avg_samples':'7day_30m_avg_samples',
            'current_day_rainfall':'hourly_rainfall',
            'current_day_7day_rainfall':'7day_hourly_rainfall'
        }

        if dataset in pass_through_data_sets.keys():
            return daily.json_dispatch(station,
                                       pass_through_data_sets[dataset],
                                       datetime.now().date())
        elif dataset == '24hr_samples':
            return daily.get_day_dataset(datetime.now(),
                                         get_24hr_samples_data,
                                         outdoor_sample_result_to_json,
                                         station_id)
        elif dataset == '24hr_rainfall_totals':
            return get_day_rainfall(datetime.now(), station_id, True)
        elif dataset == '24hr_hourly_rainfall':
            return get_day_dataset(datetime.now(),
                                   get_24hr_hourly_rainfall_data,
                                   rainfall_sample_result_to_json,
                                   station_id)
        elif dataset == '24hr_reception' and hw_type in ["DAVIS"]:
            # This is only supported for wireless DAVIS hardware at this time.

            if not hw_config['is_wireless']:
                raise web.NotFound()

            return get_day_dataset(datetime.now(),
                                   get_24hr_reception,
                                   reception_result_to_json,
                                   station_id)
        elif dataset == '168hr_reception' and hw_type in ["DAVIS"]:
            # This is only supported for wireless DAVIS hardware at this time.

            if not hw_config['is_wireless']:
                raise web.NotFound()

            return get_day_dataset(datetime.now(),
                                   get_168hr_reception,
                                   reception_result_to_json,
                                   station_id)
        elif dataset == '168hr_30m_avg_samples':
            return daily.get_day_dataset(datetime.now(),
                                         daily.get_168hr_30mavg_samples_data,
                                         daily.outdoor_sample_result_to_json,
                                         station_id)
        elif dataset == '168hr_rainfall':
            return daily.get_day_dataset(datetime.now(),
                                         daily.get_168hr_hourly_rainfall_data,
                                         rainfall_sample_result_to_json,
                                         station_id)
        elif dataset == 'live':
            return live_data(station_id)
        elif dataset == 'samplerange':
            return sample_range(station_id)
        elif dataset == 'about':
            nav = about_nav()
            return nav.GET(station)
        else:
            raise web.NotFound()


class datatable_json:
    """
    JSON data sources at the station level
    """
    def GET(self, station, dataset):
        """
        Handles station-level JSON data sources.
        :param station: Station to get data for
        :param dataset: Dataset to get.
        :return: JSON data
        :raise: web.NotFound if an invalid request is made.
        """

        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        hw_type = get_station_type_code(station_id)
        hw_config = get_station_config(station_id)

        pass_through_data_sets = {
            'current_day_samples':'samples',
            'current_day_7day_30m_avg_samples':'7day_30m_avg_samples',
            'current_day_rainfall':'hourly_rainfall',
            'current_day_7day_rainfall':'7day_hourly_rainfall',
        }

        if dataset in pass_through_data_sets.keys():
            return daily.dt_json_dispatch(station, pass_through_data_sets[dataset], datetime.now().date())
        elif dataset == '24hr_samples':
            return daily.get_day_dataset(datetime.now(),
                                         get_24hr_samples_data,
                                         outdoor_sample_result_to_datatable,
                                         station_id)
        elif dataset == '24hr_hourly_rainfall':
            return get_day_dataset(datetime.now(),
                                   get_24hr_hourly_rainfall_data,
                                   rainfall_to_datatable,
                                   station_id)
        elif dataset == '168hr_30m_avg_samples':
            return daily.get_day_dataset(datetime.now(),
                                         daily.get_168hr_30mavg_samples_data,
                                         outdoor_sample_result_to_datatable,
                                         station_id)
        elif dataset == '168hr_rainfall':
            return daily.get_day_dataset(datetime.now(),
                                         daily.get_168hr_hourly_rainfall_data,
                                         rainfall_to_datatable,
                                         station_id)
        elif dataset == '24hr_reception' and hw_type in ["DAVIS"]:
            # This is only supported for wireless DAVIS hardware at this time.

            if not hw_config['is_wireless']:
                raise web.NotFound()

            return get_day_dataset(datetime.now(),
                                   get_24hr_reception,
                                   reception_result_to_datatable,
                                   station_id)
        elif dataset == '168hr_reception' and hw_type in ["DAVIS"]:
            # This is only supported for wireless DAVIS hardware at this time.

            if not hw_config['is_wireless']:
                raise web.NotFound()

            return get_day_dataset(datetime.now(),
                                   get_168hr_reception,
                                   reception_result_to_datatable,
                                   station_id)
        else:
            raise web.NotFound()


def live_data(station_id):
    """
    Gets a JSON file containing live data.
    :param station_id: The ID of the weather station to work with
    :type station_id: int
    :return: JSON data.
    """
    data_ts, data, hw_type = get_live_data(station_id)



    if data is not None:
        result = {'relative_humidity': data.relative_humidity,
                  'temperature': data.temperature,
                  'dew_point': data.dew_point,
                  'wind_chill': data.wind_chill,
                  'apparent_temperature': data.apparent_temperature,
                  'absolute_pressure': data.absolute_pressure,
                  'average_wind_speed': data.average_wind_speed,
                  'wind_direction': data.wind_direction,
                  'time_stamp': str(data.time_stamp),
                  'age': data.age,
                  's': 'ok',
                  'hw_type': hw_type,
                  'davis': None
                  }

        if hw_type == 'DAVIS':

            uv = None
            if data.uv_index is not None:
                uv = float(data.uv_index)

            result['davis'] = {
                'bar_trend': data.bar_trend,
                'rain_rate': data.rain_rate,
                'storm_rain': data.storm_rain,
                'current_storm_date': str(data.current_storm_start_date),
                'tx_batt': data.transmitter_battery,
                'console_batt': data.console_battery_voltage,
                'forecast_icon': data.forecast_icon,
                'forecast_rule': data.forecast_rule_id,
                'uv_index': uv,
                'solar_radiation': data.solar_radiation
            }
    else:
        result = {'s': 'bad'}

    if data_ts is not None:
        now = datetime.now()
        data_ts = datetime.combine(now.date(), data_ts)
        live_data_cache_control(data_ts, station_id)

    web.header('Content-Type', 'application/json')
    return json.dumps(result)


def sample_range(station_id):
    """
    Returns the minimum and maximum sample dates available for the station.
    :param station_id: Weather station ID
    :type station_id: int
    :return: JSON data.
    """

    latest = str(get_latest_sample_timestamp(station_id))
    oldest = str(get_oldest_sample_timestamp(station_id))

    result = {"latest": latest, "oldest": oldest}
    web.header('Content-Type', 'application/json')
    return json.dumps(result)


class images:
    """
    Provides an index of available daily data sources
    """
    def GET(self, station, source_code):
        """
        Returns an index page containing a list of json files available for
        the day.
        :param station: Station to get data for
        :type station: string
        :param source_code: Image source code
        :type source_code: str
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        source = get_image_source(station_id, source_code)
        if source is None:
            raise web.NotFound()

        image_list = get_day_images_for_source(source.image_source_id)

        extensions = dict()

        image_set = []

        for image in image_list:
            image_set.append(image)
            mime = image.mime_type

            if mime not in extensions.keys():
                ext = mimetypes.guess_extension(mime, False)
                if ext == ".jpe":
                    ext = ".jpeg"
                extensions[mime] = ext

        web.header('Content-Type', 'text/html')
        return render.station_image_index(
                source_name=source.source_name,
                source_code=source_code,
                image_list=image_set,
                extensions=extensions)

class latest_image:
    """
    Provides an index of available daily data sources
    """
    def GET(self, station, source_code, mode):
        """
        Returns an index page containing a list of json files available for
        the day.
        :param station: Station to get data for
        :type station: string
        :param source_code: Image source code
        :type source_code: str
        :param mode: Image mode (full, thumbnail, metadata, etc)
        :type mode: str
        """

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        source = get_image_source(station_id, source_code)
        if source is None:
            raise web.NotFound()

        # The requested URL will be something like this:
        # /data/sb/images/test/latest/full

        # Figure out what the latest image for the image source is and bounce
        # the user to:
        # ../../../{year}/{month}/{day}/images/{source}/{id}/full.jpeg

        most_recent = get_most_recent_image_id_for_source(
            source.image_source_id)
        most_recent_id = most_recent.image_id
        most_recent_ts = most_recent.time_stamp
        most_recent_mime = most_recent.mime_type

        if mode == "metadata":
            extension = "json"
        else:
            extension = mimetypes.guess_extension(most_recent_mime, False)
            if extension == ".jpe":
                extension = ".jpeg"

        if not extension.startswith("."):
            extension = "." + extension

        parameters = {
            "year": most_recent_ts.year,
            "month": most_recent_ts.month,
            "day": most_recent_ts.day,
            "source": source_code,
            "id": most_recent_id,
            "mode": mode,
            "extension": extension
        }

        url = "../../../{year}/{month}/{day}/images/{source}/" \
              "{id}/{mode}{extension}".format(**parameters)

        raise web.found(url)
