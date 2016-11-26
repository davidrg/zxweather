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

# Google analyics tracking ID. Set to a value to enable.
google_analytics_id = None


def load_settings():
    """
    Loads settings from the configuration file.
    """

    global db, default_station_name
    global static_data_dir, site_root, default_ui, site_name, ws_uri, wss_uri
    global zxweatherd_hostname, zxweatherd_raw_port, disable_alt_ui
    global hide_coordinates, google_analytics_id
    global cache_thumbnails, cache_directory, thumbnail_size, cache_videos
    global video_cache_directory

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

    # Site
    default_station_name = config.get(S_S, 'station_name')
    site_root = config.get(S_S, 'site_root')
    default_ui = config.get(S_S, 'default_ui')
    site_name = config.get(S_S, 'site_name')
    static_data_dir = config.get(S_S, 'static_data_dir')

    if config.has_option(S_S, 'disable_alt_ui'):
        disable_alt_ui = config.getboolean(S_S, 'disable_alt_ui')

    if config.has_option(S_S, 'hide_coordinates'):
        hide_coordinates = config.getboolean(S_S, 'hide_coordinates')

    if config.has_option(S_S, "google_analytics_id"):
        google_analytics_id = config.get(S_S, "google_analytics_id")

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

    if len(default_station_name) > 5:
        raise Exception('ConfigurationError: Default station name can not be '
                        'longer than five characters. Consult installation '
                        'reference manual.')

    if not static_data_dir.endswith("/"):
        static_data_dir += "/"

    # Thumbnail settings
    if config.has_option(S_T, "cache_thumbnails"):
        cache_thumbnails = config.getboolean(S_T, "cache_thumbnails")

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

        if config.has_option(S_T, "cache_directory"):
            video_cache_directory = config.get(S_T, "cache_directory")

            if not video_cache_directory.endswith(os.path.sep):
                video_cache_directory += os.path.sep
                video_cache_directory += "videos" + os.path.sep

            if not os.path.exists(video_cache_directory):
                os.makedirs(video_cache_directory)
        else:
            raise Exception("Thumbnail cache directory not set")



