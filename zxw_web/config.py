# coding=utf-8
"""
Handles loading the configuration file and making configuration data available.
"""
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

# Name displayed in the navigation thing
site_name = 'zxweather'

# Websocket URIs
ws_uri = None
wss_uri = None

def load_settings():
    """
    Loads settings from the configuration file.
    """

    global db, default_station_name
    global static_data_dir, site_root, default_ui, site_name, ws_uri, wss_uri

    import ConfigParser
    config = ConfigParser.ConfigParser()
    config.read(['config.cfg', 'zxw_web/config.cfg', '/etc/zxweather.cfg'])

    # Configuration sections
    S_DB = 'database'   # Database configuration
    S_S = 'site'        # Site configuration

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

    if config.has_option(S_S, 'ws_uri'):
        ws_uri = config.get(S_S, 'ws_uri')
    if config.has_option(S_S, 'wss_uri'):
        wss_uri = config.get(S_S, 'wss_uri')

    if len(default_station_name) > 5:
        raise Exception('ConfigurationError: Default station name can not be longer than five characters. Consult installation reference manual.')

    if not static_data_dir.endswith("/"):
        static_data_dir += "/"
