# coding=utf-8
"""
Data sources at the station level.
"""
import hashlib
import mimetypes
from datetime import datetime, timedelta, date
import json
import web
from web.contrib.template import render_jinja
from cache import live_data_cache_control, rfcformat
from config import site_root
from data import daily, about_nav
from data.daily import get_24hr_samples_data, get_day_rainfall, get_day_dataset, get_24hr_hourly_rainfall_data, \
    get_24hr_reception, get_168hr_reception, get_24hr_rainfall_data
from data.util import outdoor_sample_result_to_json, outdoor_sample_result_to_datatable, rainfall_sample_result_to_json, rainfall_to_datatable, \
    reception_result_to_json, reception_result_to_datatable
from database import get_years, get_live_data, get_station_id, get_latest_sample_timestamp, get_oldest_sample_timestamp, \
    get_station_type_code, get_station_config, get_image_sources_for_station, \
    get_image_source, get_day_images_for_source, \
    get_most_recent_image_id_for_source, get_current_3h_trends, \
    get_month_rainfall, get_year_rainfall, get_last_hour_rainfall, \
    get_10m_avg_bearing_max_gust, get_day_wind_run, get_cumulus_dayfile_data, \
    get_image_source_info, get_image_sources_by_date, get_rain_summary
import os

import math
from database import get_live_indoor_data, get_latest_sample, \
    get_day_evapotranspiration, get_daily_records
from database import get_day_rainfall as get_day_rainfall_data

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
            hw_config = get_station_config(station_id)
            reception_available = hw_config['is_wireless']

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
        elif dataset == '24hr_rainfall':
            return get_day_dataset(datetime.now(),
                                   get_24hr_rainfall_data,
                                   rainfall_sample_result_to_json,
                                   station_id)
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
        elif dataset == 'image_sources':
            return image_sources(station_id, station)
        elif dataset == 'image_sources_by_date':
            return image_sources_by_date(station_id)
        elif dataset == 'rain_summary':
            return rain_summary(station_id)
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


def image_sources(station_id, station_code):
    """
    Returns some basic information about each image source present for a given
    station id. This includes name, description, dates for first and last image
    and if the source is currently active (has images taken in the last 24
    hours)
    :param station_id: Station to get image sources for
    :type station_id: int
    """
    result = {}

    url_base = site_root + "data/{0}/".format(station_code)
    url_template = url_base + "{year}/{month}/{day}/images/{source_code}/" \
                              "{hour:02d}_{minute:02d}_{second:02d}/" \
                              "{type_code}_{size}{extension}"

    data = get_image_source_info(station_id)
    if data is not None:
        for row in data:
            ext = mimetypes.guess_extension(row.last_image_mime_type, False)
            if ext == ".jpe":
                ext = ".jpeg"

            url = url_template.format(
                year=row.last_image_time_stamp.year,
                month=row.last_image_time_stamp.month,
                day=row.last_image_time_stamp.day,
                source_code=row.code.lower(),
                hour=row.last_image_time_stamp.hour,
                minute=row.last_image_time_stamp.minute,
                second=row.last_image_time_stamp.second,
                type_code=row.last_image_type_code.lower(),
                size="full",
                extension=ext
            )

            result[row.code] = {
                'name': row.source_name,
                'description': row.description,
                'first_image': row.first_image.isoformat(),
                'last_image': row.last_image.isoformat(),
                'is_active': row.is_active,
                'last_image_info': {
                    'id': row.last_image_id,
                    'timestamp': row.last_image_time_stamp.isoformat(),
                    'type_code': row.last_image_type_code,
                    'title': row.last_image_title,
                    'description': row.last_image_description,
                    'mime_type': row.last_image_mime_type,
                    'urls': {
                        'full': url
                    }
                }
            }

    web.header('Content-Type', 'application/json')
    return json.dumps(result)


def image_sources_by_date(station_id):
    result = {}

    rows = get_image_sources_by_date(station_id)

    for row in rows:
        result[row.date_stamp.isoformat()] = row.image_source_codes.split(',')

    # TODO: cache control?
    web.header('Content-Type', 'application/json')
    return json.dumps(result)


def rain_summary(station_id):
    result = {}

    rows = get_rain_summary(station_id)

    for row in rows:
        result[row.period] = {
            'total': row.rainfall,
            'start': row.start_time.isoformat(),
            'end': row.end_time.isoformat()
        }

    # TODO: cache control?
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


class images_json:
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
        latest_ts = None
        image_set = []

        for image in image_list:
            if latest_ts is None or image.time_stamp > latest_ts:
                latest_ts = image.time_stamp

            mime = image.mime_type

            if mime not in extensions.keys():
                ext = mimetypes.guess_extension(mime, False)
                if ext == ".jpe":
                    ext = ".jpeg"
                extensions[mime] = ext

            url_prefix = str(image.time_stamp.year) + "/" \
                + str(image.time_stamp.month) + "/" \
                + str(image.time_stamp.day) + "/images/" + source_code + "/" \
                + image.time_stamp.time().strftime("%H_%M_%S") \
                + '/' + image.type_code.lower()

            image_url = url_prefix + '_full' + extensions[image.mime_type]

            thumb_url = None
            if image.mime_type.startswith("image/"):
                thumb_url = url_prefix + '_thumbnail' + \
                            extensions[image.mime_type]

            metadata_url = None
            if image.has_metadata:
                metadata_url = url_prefix + '_metadata.json'

            img = {
                "id": image.id,
                "time_stamp": image.time_stamp.isoformat(),
                "mime_type": image.mime_type,
                "type_name": image.type_name,
                "type_code": image.type_code,
                "title": image.title,
                "description": image.description,
                "has_metadata": image.has_metadata,
                "image_url": image_url,
                "thumb_url": thumb_url,
                "metadata_url": metadata_url
            }

            image_set.append(img)

        result = json.dumps(image_set)

        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(result)))
        web.header('Last-Modified', rfcformat(latest_ts))
        return result


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


class data_ascii:
    def GET(self, station, dataset):
        station_id = get_station_id(station)

        if station_id is None:
            raise web.NotFound()

        if dataset not in ascii_data.keys():
            raise web.NotFound

        if dataset == 'dayfile':
            result = make_cumulus_dayfile(station_id)

            today = date.today()
            midnight = datetime(year=today.year, month=today.month,
                                day=today.day)
            tomorrow_midnight = midnight + timedelta(days=1)

            # This file only changes once a day at midnight.
            web.header("Expires", rfcformat(tomorrow_midnight))
            web.header("Last-Modified", rfcformat(midnight))
            # TODO: expires at midnight. No sooner.
        else:
            # This file contains live data. Don't cache it
            web.header("Cache-Control", "no-cache")
            web.header("Last-Modified", rfcformat(datetime.now()))

            result = make_ascii_live(station_id, dataset)

            # One particular client appears to require an ETag
            m = hashlib.md5()
            m.update(result)
            web.header("ETag", '"' + m.hexdigest() + '"')

        web.header("Content-Type", "text/plain")
        return result

ascii_data = {
    # This is the cumulus realtime.txt format as described here:
    # http://wiki.sandaysoft.com/a/Realtime.txt
    "realtime": {
        "description": "Cumulus realtime.txt format",
        "datasets": {"sample", "day_records", "ET", "current_trends",
                     "month_rain", "year_rain", "yesterday_rain",
                     "last_hour_rain", "10m_wind", "day_windrun"},
        "format": "{date} {timehhmmss} {temp} {hum} {dew} {wspeed} "
                  "{wlatest} {bearing} {rrate} {rfall:.1f} {press:.1f} "
                  "{currentwdir} {beaufortnumber} {windunit} {tempunitnodeg} "
                  "{pressunit} {rainunit} {windrun:.1f} {presstrendval} "
                  "{rmonth:.1f} {ryear:.1f} {rfallY:.1f} {intemp:.1f} {inhum} "
                  "{wchill:.1f} {temptrend} {tempTH:.1f} {TtempTH} "
                  "{tempTL:.1f} {TtempTL} {windTM:.1f} {TwindTM} {wgustTM:.1f} "
                  "{TwgustTM} {pressTH:.1f} {TpressTH} {pressTL:.1f} "
                  "{TpressTL} {version} {build} {wgust} {heatindex} "
                  "{humidex} {UV} {ET:.2f} {SolarRad} "
                  "{avgbearing} {rhour} {forecastnumber} {isdaylight} "
                  "{SensorContactLost} {wdir} {cloudbasevalue} {cloudbaseunit} "
                  "{apptemp} {SunshineHours} {CurrentSolarMax} {IsSunny}\r\n"
    },
    "dayfile": None,
}

bft_max = [0.3, 2, 3, 5.4, 8, 10.7, 13.8, 17.1, 20.6, 24.4, 28.3, 32.5, 9999]

wind_directions = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S",
                   "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]


def bearing_to_compass(bearing):
    if bearing is None:
        return "---"

    index = int(math.floor(((bearing * 100 + 1125) % 36000) / 2250))
    return wind_directions[index]


def coalesce(value):
    if value is None:
        return "-"
    return value


def coalesce_float(value):
    if value is None:
        return "-"
    return "{0:.1f}".format(value)


def convert_wind_speed(unit, value):
    if value is None:
        return value
    elif unit == "m/s":
        return value
    elif unit == "km/h":
        return value * 18 / 5


def add_pos_symbol(value):
    if value is None:
        return "-"

    if value > 0:
        return "+{0:.1f}".format(value)
    return "{0:.1f}".format(value)


def make_ascii_live(station_id, filename):
    fmt = ascii_data[filename]["format"]
    sets = ascii_data[filename]["datasets"]
    # These are values that are currently static because
    # zxweather only does metric.
    param = {
        "windunit": "km/h",  # change to m/s for wind in m/s
        "tempunitnodeg": "C",
        "pressunit": "hPa",
        "rainunit": "mm",

        # We don't calculate it now but if we ever do we'll do it metric-like
        "cloudbaseunit": "m",

        # We're not Cumulus. But we'll pretend to be 1.9.4.
        "version": "1.9.4",
        "build": "1099"  # Still not Cumulus.
    }

    data_ts, data, hw_type = get_live_data(station_id)
    data_indoor = get_live_indoor_data(station_id)  # TODO: merge this into above

    ts = data.date_stamp
    ts = datetime(year=ts.year, month=ts.month, day=ts.day, hour=data_ts.hour, minute=data_ts.minute, second=data_ts.second)

    param["date"] = datetime.strftime(ts, "%d/%m/%y")
    param["timehhmmss"] = datetime.strftime(ts, "%H:%M:%S")

    bft = -1
    for i in range(0, 13):
        if i < 1:
            min_val = -1
        else:
            min_val = bft_max[i-1]
        max_val = bft_max[i]

        if min_val < data.average_wind_speed <= max_val:
            bft = i

    live_param = {
        "intemp": data_indoor.indoor_temperature,
        "inhum": data_indoor.indoor_relative_humidity,
        "temp": coalesce_float(data.temperature),
        "hum": data.relative_humidity,
        "dew": coalesce_float(data.dew_point),
        "wchill": data.wind_chill,
        "apptemp": coalesce_float(data.apparent_temperature),
        "press": data.absolute_pressure,  # TODO: convert to sea level
        "wlatest": coalesce_float(convert_wind_speed(param["windunit"],
                                                     data.average_wind_speed)),
        "beaufortnumber": bft,
        "bearing": coalesce(data.wind_direction),
        "currentwdir": bearing_to_compass(data.wind_direction),
        "rrate": 0.0,  # TODO: Calculate for WH1080
        "UV": 0,
        "SolarRad": 0,
    }

    if hw_type == "DAVIS":
        live_param["rrate"] = coalesce_float(data.rain_rate)

        hw_config = get_station_config(station_id)
        if hw_config["has_solar_and_uv"]:
            uv = data.uv_index
            solar_rad = data.solar_radiation
            if uv is not None:
                uv = int(round(uv))
            if solar_rad is not None:
                solar_rad = int(round(solar_rad))

            live_param["UV"] = uv
            live_param["SolarRad"] = solar_rad

    param.update(live_param)

    if "sample" in sets:
        sample = get_latest_sample(station_id)

        sample_param = {
            "wspeed": coalesce_float(convert_wind_speed(
                param["windunit"], sample.average_wind_speed)),
            "wdir": bearing_to_compass(sample.wind_direction)
        }

        # This should really only be set for WH1080 hardware but we'll
        # do it for everything:
        if sample.lost_sensor_contact:
            sample_param["SensorContactLost"] = 1
        else:
            sample_param["SensorContactLost"] = 0

        param.update(sample_param)

    if "ET" in sets:
        param["ET"] = 0
        if hw_type == "DAVIS":
            param["ET"] = get_day_evapotranspiration(station_id)

    def time_fmt(t):
        n = datetime.now()
        t = datetime(year=n.year, month=n.month, day=n.day, hour=t.hour,
                     minute=t.minute, second=t.second)
        return datetime.strftime(t, "%H:%M")

    if "day_records" in sets:
        records = get_daily_records(
            get_latest_sample_timestamp(station_id).date(), station_id)

        record_param = {
            "rfall": records.total_rainfall,
            "tempTH": records.max_temperature,
            "TtempTH": time_fmt(records.max_temperature_ts),
            "tempTL": records.min_temperature,
            "TtempTL": time_fmt(records.min_temperature_ts),
            "windTM": convert_wind_speed(param["windunit"],
                                         records.max_average_wind_speed),
            "TwindTM": time_fmt(records.max_average_wind_speed_ts),
            "wgustTM": convert_wind_speed(param["windunit"],
                                          records.max_gust_wind_speed),
            "TwgustTM": time_fmt(records.max_gust_wind_speed_ts),
            "pressTH": records.max_absolute_pressure,  # TODO: convert to sea level
            "TpressTH": time_fmt(records.max_absolute_pressure_ts),
            "pressTL": records.min_absolute_pressure,  # TODO: convert to sea level
            "TpressTL": time_fmt(records.min_absolute_pressure_ts),
            "apptempTH": records.max_apparent_temperature,
            "TapptempTH": time_fmt(records.max_apparent_temperature_ts),
            "apptempTL": records.min_apparent_temperature,
            "TapptempTL": time_fmt(records.min_apparent_temperature_ts),
            "wchillTL": records.min_wind_chill,
            "TwchillTL": time_fmt(records.min_wind_chill_ts),
            "wchillTH": records.max_wind_chill,
            "TwchillTH": time_fmt(records.max_wind_chill_ts),
            "dewpointTH": records.max_dew_point,
            "TdewpointTH": time_fmt(records.max_dew_point_ts),
            "dewpointTL": records.min_dew_point,
            "TdewpointTL": time_fmt(records.min_dew_point_ts)
        }
        param.update(record_param)

    if "current_trends" in sets:
        # 3-hour trend values
        trends = get_current_3h_trends(station_id)
        param.update(
            {
                "presstrendval": add_pos_symbol(trends.absolute_pressure_trend),
                "temptrend": add_pos_symbol(trends.temperature_trend),
                "intemptrend": add_pos_symbol(trends.indoor_temperature_trend),
                "inhumtrend": add_pos_symbol(trends.indoor_humidity_trend),
                "humtrend": add_pos_symbol(trends.humidity_trend),
                "dewpointtrend": add_pos_symbol(trends.dew_point_trend),
                "windchilltrend": add_pos_symbol(trends.wind_chill_trend),
                "apptemptrend": add_pos_symbol(
                    trends.apparent_temperature_trend)
            }
        )

    if "yesterday_rain" in sets:
        yesterday = ts.date() - timedelta(1)
        param["rfallY"] = get_day_rainfall_data(yesterday, station_id)

    if "month_rain" in sets:
        param["rmonth"] = get_month_rainfall(ts.year, ts.month, station_id)

    if "year_rain" in sets:
        param["ryear"] = get_year_rainfall(ts.year, station_id)

    if "last_hour_rain" in sets:
        param["rhour"] = coalesce_float(get_last_hour_rainfall(station_id))

    if "10m_wind" in sets:
        wind_10m = get_10m_avg_bearing_max_gust(station_id)

        bearing = wind_10m.avg_bearing
        if bearing is not None:
            bearing = int(round(bearing))

        param["avgbearing"] = coalesce(bearing)
        param["wgust"] = coalesce_float(convert_wind_speed(param["windunit"],
                                                           wind_10m.max_gust))

    if "day_windrun" in sets:
        # The database will report in meters. Convert to kilometers.
        param["windrun"] = get_day_wind_run(ts.date(), station_id) / 1000

    # TODO: calculate all of these - fthey're used by cumulus realtime.txt
    param.update({
        "heatindex": coalesce_float(0),  # Heat index
        "humidex": coalesce_float(0),  # Humidex
        "forecastnumber": 0,  # Zambretti forecast string from cumulus strings.ini
        "isdaylight": 0,  # 1 = daylight, 0 = not daylight (based on sunrise/sunset times)
        "cloudbasevalue": 0,  # cloud base
        "SunshineHours": 0,  # Sunshine hours so far today
        "CurrentSolarMax": 0,  # Current theoretical max solar radiatino
        "IsSunny": 0,  # 1 = sun shining (>70% of Ryan-Stolzenbach result), 0 = not shining
    })

    # TODO: wrap in try/catch. Set result to error on missing format key.
    result = fmt.format(**param)

    return result


def make_cumulus_dayfile(station_id):
    # This is the Cumulus dayfile format:
    # http://wiki.sandaysoft.com/a/Dayfile.txt
    data = get_cumulus_dayfile_data(station_id)

    def time_fmt(t):
        if t is None:
            return "00:00"

        n = datetime.now()
        t = datetime(year=n.year, month=n.month, day=n.day, hour=t.hour,
                     minute=t.minute, second=t.second)
        return datetime.strftime(t, "%H:%M")

    result = ""

    fmt = "{date},{max_gust_wind_speed},{max_gust_wind_speed_direction}," \
          "{max_gust_wind_speed_ts},{min_temperature},{min_temperature_ts}," \
          "{max_temperature},{max_temperature_ts},{min_absolute_pressure}," \
          "{min_absolute_pressure_ts},{max_absolute_pressure}," \
          "{max_absolute_pressure_ts},{max_rain_rate},{max_rain_rate_ts}," \
          "{total_rainfall},{average_temperature},{wind_run}," \
          "{max_average_wind_speed},{max_average_wind_speed_ts}," \
          "{min_humidity},{min_humidity_ts},{max_humidity}," \
          "{max_humidity_ts},{evapotranspiration},{total_sunshine_hours}," \
          "{high_heat_index},{high_heat_index_ts}," \
          "{max_apparent_temperature},{max_apparent_temperature_ts}," \
          "{min_apparent_temperature},{min_apparent_temperature_ts}," \
          "{max_hour_rain},{max_hour_rain_ts},{min_wind_chill}," \
          "{min_wind_chill_ts},{max_dew_point},{max_dew_point_ts}," \
          "{min_dew_point},{min_dew_point_ts},{dominant_wind_direction}," \
          "{heating_degree_days},{cooling_degree_days}," \
          "{max_solar_radiation},{max_solar_radiation_ts}," \
          "{max_uv_index},{max_uv_index_ts}\n"
    for row in data:
        result += fmt.format(
            date=datetime.strftime(row.date_stamp, "%d/%m/%y"),
            max_gust_wind_speed=row.max_gust_wind_speed,
            max_gust_wind_speed_direction=row.max_gust_wind_speed_direction,
            max_gust_wind_speed_ts=time_fmt(row.max_gust_wind_speed_ts),
            min_temperature=row.min_temperature,
            min_temperature_ts=time_fmt(row.min_temperature_ts),
            max_temperature=row.max_temperature,
            max_temperature_ts=time_fmt(row.max_temperature_ts),
            min_absolute_pressure=row.min_absolute_pressure,
            min_absolute_pressure_ts=time_fmt(row.min_absolute_pressure_ts),
            max_absolute_pressure=row.max_absolute_pressure,
            max_absolute_pressure_ts=time_fmt(row.max_absolute_pressure_ts),
            max_rain_rate=row.max_rain_rate,
            max_rain_rate_ts=time_fmt(row.max_rain_rate_ts),
            total_rainfall=row.total_rainfall,
            average_temperature=row.average_temperature,
            wind_run=row.wind_run,
            max_average_wind_speed=row.max_average_wind_speed,
            max_average_wind_speed_ts=time_fmt(row.max_average_wind_speed_ts),
            min_humidity=row.min_humidity,
            min_humidity_ts=time_fmt(row.min_humidity_ts),
            max_humidity=row.max_humidity,
            max_humidity_ts=time_fmt(row.max_humidity_ts),
            evapotranspiration=row.evapotranspiration,
            total_sunshine_hours=0,
            high_heat_index=0,
            high_heat_index_ts=time_fmt(None),
            max_apparent_temperature=row.max_apparent_temperature,
            max_apparent_temperature_ts=time_fmt(row.max_apparent_temperature_ts),
            min_apparent_temperature=row.min_apparent_temperature,
            min_apparent_temperature_ts=time_fmt(row.min_apparent_temperature_ts),
            max_hour_rain=row.max_hour_rain,
            max_hour_rain_ts=time_fmt(row.max_hour_rain_ts),
            min_wind_chill=row.min_wind_chill,
            min_wind_chill_ts=time_fmt(row.min_wind_chill_ts),
            max_dew_point=row.max_dew_point,
            max_dew_point_ts=time_fmt(row.max_dew_point_ts),
            min_dew_point=row.min_dew_point,
            min_dew_point_ts=time_fmt(row.min_dew_point_ts),
            dominant_wind_direction=row.average_wind_direction,
            heating_degree_days=0,
            cooling_degree_days=0,
            max_solar_radiation=row.max_solar_radiation,
            max_solar_radiation_ts=time_fmt(row.max_solar_radiation_ts),
            max_uv_index=row.max_uv_index,
            max_uv_index_ts=time_fmt(row.max_uv_index_ts)
        )
    return result