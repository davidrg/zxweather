# coding=utf-8
"""
Handles loading the configuration file and making configuration data available.
"""
import os

__author__ = 'David Goodwin'

import web

# Database configuration
db = None

# This is the name of the default station. In a future version
# this will live in the database instead.
default_station_name = None

# Where static data lives
static_data_dir = None

# Root directory of the site. Required when doing 301/302 redirects.
site_root = None

# Set to None to use the UI picker page.
default_ui = 's'

# Set to True to disable the alternate UI. The alternate UI requires access to
# the public internet so you may want to turn it off if you're just running it
# on a private network with no internet access.
disable_alt_ui = False

hide_coordinates = False

# Name displayed in the navigation thing
site_name = 'zxweather'

# Websocket URIs
ws_uri = None
wss_uri = None
zxweatherd_hostname = None
zxweatherd_raw_port = None

# Thumbnail settings
cache_thumbnails = False
cache_directory = None
thumbnail_size = (304, 171)
cache_videos = False
video_cache_directory = None

max_thumbnail_cache_size = None
max_video_cache_size = None
cache_expiry_access_time = False

# Google analyics tracking ID. Set to a value to enable.
google_analytics_id = None

image_type_sort = None

array_position_available = True

wind_speed_kmh = False

report_settings = dict()


def load_settings():
    """
    Loads settings from the configuration file.
    """

    global db, default_station_name, array_position_available
    global static_data_dir, site_root, default_ui, site_name, ws_uri, wss_uri
    global zxweatherd_hostname, zxweatherd_raw_port, disable_alt_ui
    global hide_coordinates, google_analytics_id, image_type_sort
    global cache_thumbnails, cache_directory, thumbnail_size, cache_videos
    global video_cache_directory, max_thumbnail_cache_size, max_video_cache_size
    global cache_expiry_access_time, report_settings, wind_speed_kmh

    import ConfigParser
    config = ConfigParser.ConfigParser()
    config.read(['config.cfg', 'zxw_web/config.cfg', '/etc/zxweather/web.cfg',
                 '/etc/zxweather.cfg'])

    # Configuration sections
    S_DB = 'database'   # Database configuration
    S_S = 'site'        # Site configuration
    S_D = 'zxweatherd'  # zxweatherd configuration information
    S_T = 'image_thumbnails'    # Image thumbnail options

    # Make sure a few important settings people might overlook are set.
    if not config.has_option(S_S,'site_root'):
        raise Exception("ConfigurationError: site_root not specified. Consult installation reference manual.")
    if not config.has_option(S_S,'static_data_dir'):
        raise Exception("ConfigurationError: static data location not specified. Consult installation reference manual.")
    if not config.has_option(S_S,'station_name'):
        raise Exception("ConfigurationError: station name not specified. Consult installation reference manual.")

    # Database
    hostname = config.get(S_DB,'host')
    port = config.getint(S_DB,'port')
    database = config.get(S_DB,'database')
    user = config.get(S_DB,'user')
    pw = config.get(S_DB,'password')
    db = web.database(dbn='postgres',
                      user=user,
                      pw=pw,
                      db=database,
                      host=hostname,
                      port=port)
    array_position_available = config.getboolean(S_DB, "array_position_available")

    # Site
    default_station_name = config.get(S_S, 'station_name')
    site_root = config.get(S_S, 'site_root')

    if not site_root.endswith("/"):
        site_root += "/"

    default_ui = config.get(S_S, 'default_ui')
    site_name = config.get(S_S, 'site_name')
    static_data_dir = config.get(S_S, 'static_data_dir')

    if config.has_option(S_S, 'wind_speed_kmh'):
        wind_speed_kmh = config.getboolean(S_S, 'wind_speed_kmh')

    if config.has_option(S_S, 'disable_alt_ui'):
        disable_alt_ui = config.getboolean(S_S, 'disable_alt_ui')

    if config.has_option(S_S, 'hide_coordinates'):
        hide_coordinates = config.getboolean(S_S, 'hide_coordinates')

    if config.has_option(S_S, "google_analytics_id"):
        google_analytics_id = config.get(S_S, "google_analytics_id")

    if config.has_option(S_S, "image_type_sort"):
        image_type_sort = config.get(S_S, "image_type_sort").strip()
        # remove any padding or empty items from the list
        if len(image_type_sort) > 0:
            bits = image_type_sort.split(",")
            bits = [x.strip() for x in bits if len(x.strip()) > 0]
            image_type_sort = ",".join(bits)
    if image_type_sort is None:
        raise Exception("ConfigurationError: image_type_sort option not specified")

    if config.has_option(S_D, 'hostname'):
        zxweatherd_hostname = config.get(S_D, 'hostname')
        if config.has_option(S_D, 'ws_port'):
            ws_port = config.getint(S_D, 'ws_port')
            ws_uri = "ws://{host}:{port}/".format(host=zxweatherd_hostname,
                                                  port=ws_port)
        if config.has_option(S_D, 'wss_port'):
            wss_port = config.getint(S_D, 'wss_port')
            wss_uri = "wss://{host}:{port}/".format(host=zxweatherd_hostname,
                                                    port=wss_port)
        if config.has_option(S_D, 'raw_port'):
            zxweatherd_raw_port = config.getint(S_D, 'raw_port')

    if len(default_station_name) > 5 or default_station_name is None:
        raise Exception('ConfigurationError: Default station name can not be '
                        'longer than five characters. Consult installation '
                        'reference manual.')

    if not static_data_dir.endswith("/"):
        static_data_dir += "/"

    # Thumbnail settings
    if config.has_option(S_T, "cache_thumbnails"):
        cache_thumbnails = config.getboolean(S_T, "cache_thumbnails")

        if cache_thumbnails:
            if config.has_option(S_T, "max_thumbnail_cache_size"):
                max_thumbnail_cache_size = config.getint(S_T, "max_thumbnail_cache_size")

            if config.has_option(S_T, "cache_directory"):
                cache_directory = config.get(S_T, "cache_directory")

                if not cache_directory.endswith(os.path.sep):
                    cache_directory += os.path.sep
                cache_directory += "thumbnails" + os.path.sep

                if not os.path.exists(cache_directory):
                            os.makedirs(cache_directory)
            else:
                raise Exception("Thumbnail cache directory not set")

    if config.has_option(S_T, "thumbnail_size"):
        val = config.get(S_T, "thumbnail_size")
        bits = val.split("x")
        if len(bits) != 2:
            raise Exception("Invalid thumbnail dimensions. "
                            "Expected WIDTHxHEIGHT.")
        thumbnail_size = (int(bits[0]), int(bits[1]))

    if config.has_option(S_T, "cache_videos"):
        cache_videos = config.getboolean(S_T, "cache_thumbnails")
        if cache_videos:
            if config.has_option(S_T, "max_video_cache_size"):
                max_video_cache_size = config.getint(S_T, "max_video_cache_size")

            if config.has_option(S_T, "cache_directory"):
                video_cache_directory = config.get(S_T, "cache_directory")

                if not video_cache_directory.endswith(os.path.sep):
                    video_cache_directory += os.path.sep
                    video_cache_directory += "videos" + os.path.sep

                if not os.path.exists(video_cache_directory):
                    os.makedirs(video_cache_directory)
            else:
                raise Exception("Thumbnail cache directory not set")

    if cache_videos or cache_thumbnails:
        if config.has_option(S_T, "expire_cache_by_access_time"):
            cache_expiry_access_time = config.getboolean(S_T, "expire_cache_by_access_time")

    station_report_sections = [s[8:] for s in config.sections() if s.startswith("reports_")]

    for stn in station_report_sections:
        section = "reports_" + stn

        noaa_month = {
            "city": "",
            "state": "",
            "heatBase": 18.3,
            "coolBase": 18.3,
            "celsius": True,
            "fahrenheit": False,
            "kmh": True,  # false=m/s
            "mph": False,  # true=mph
            "inches": False,
            "altFeet": False,
            "stationCode": stn
        }

        if config.has_option(section, "city"):
            noaa_month["city"] = config.get(section, "city")

        if config.has_option(section, "state"):
            noaa_month["state"] = config.get(section, "state")

        if config.has_option(section, "wind_kmh"):
            noaa_month["kmh"] = config.getboolean(section, "wind_kmh")

        if config.has_option(section, "heat_base"):
            noaa_month["heatBase"] = config.getfloat(section, "heat_base")

        if config.has_option(section, "cool_base"):
            noaa_month["coolBase"] = config.getfloat(section, "cool_base")

        report_settings[stn] = {
            "noaa_month": noaa_month
        }

        if config.has_option(section, "normal_temperature") and \
                config.has_option(section, "normal_rainfall"):

            noaa_year = noaa_month.copy()

            try :
                normal_temps = [float(s.strip()) for s in config.get(section, "normal_temperature").split(",")]
                normal_rainfall = [float(s.strip()) for s in config.get(section, "normal_rainfall").split(",")]

                noaa_year.update({
                    "normJan": normal_temps[0],
                    "normJanRain": normal_rainfall[0],
                    "normFeb": normal_temps[1],
                    "normFebRain": normal_rainfall[1],
                    "normMar": normal_temps[2],
                    "normMarRain": normal_rainfall[2],
                    "normApr": normal_temps[3],
                    "normAprRain": normal_rainfall[3],
                    "normMay": normal_temps[4],
                    "normMayRain": normal_rainfall[4],
                    "normJun": normal_temps[5],
                    "normJunRain": normal_rainfall[5],
                    "normJul": normal_temps[6],
                    "normJulRain": normal_rainfall[6],
                    "normAug": normal_temps[7],
                    "normAugRain": normal_rainfall[7],
                    "normSep": normal_temps[8],
                    "normSepRain": normal_rainfall[8],
                    "normOct": normal_temps[9],
                    "normOctRain": normal_rainfall[9],
                    "normNov": normal_temps[10],
                    "normNovRain": normal_rainfall[10],
                    "normDec": normal_temps[11],
                    "normDecRain": normal_rainfall[11],
                })

                report_settings[stn]["noaa_year"] = noaa_year

            except Exception as e:
                print(e)
                pass






