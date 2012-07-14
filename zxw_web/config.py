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

# Set this to True if you are running wh1080d to feed samples and live data
# into the database.
live_data_available = True

# How often new samples appear from the weather station, in seconds.
sample_interval = 300 # 5 minutes

# How often charts are re-plotted, in seconds
plot_interval = 1800

# Where static data lives
static_data_dir = None

# Root directory of the site. Required when doing 301/302 redirects.
site_root = None

# Set to None to use the UI picker page.
default_ui = 's'

# Name displayed in the navigation thing
site_name = 'zxweather'

# Allow updated data (with a valid signature) to be uploaded?
enable_data_loading = False

# Home directory for GnuPG
gnupg_home = None

# Binary for GnuPG.
gpg_binary = None

# Only accept data verified with this public key:
key_fingerprint = None

def load_settings():
    """
    Loads settings from the configuration file.
    """

    global db, default_station_name, live_data_available, sample_interval
    global plot_interval, static_data_dir, site_root, default_ui, site_name
    global enable_data_loading, gnupg_home, gpg_binary, key_fingerprint

    import ConfigParser
    config = ConfigParser.ConfigParser()
    config.read(['config.cfg', 'zxw_web/config.cfg', '/etc/zxweather.cfg'])

    # Configuration sections
    S_DB = 'database'   # Database configuration
    S_D = 'data'        # Data configuration
    S_S = 'site'        # Site configuration
    S_DBR = 'database_replication' # Database replication configuration

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

    # Data
    live_data_available = config.getboolean(S_D,'live_data_available')
    sample_interval = config.getint(S_D,'sample_interval')
    plot_interval = config.getint(S_D,'plot_interval')

    # Site
    default_station_name = config.get(S_S, 'station_name')
    site_root = config.get(S_S, 'site_root')
    default_ui = config.get(S_S, 'default_ui')
    site_name = config.get(S_S, 'site_name')
    static_data_dir = config.get(S_S, 'static_data_dir')

    if not static_data_dir.endswith("/"):
        static_data_dir += "/"

    # Database Replication
    if not config.has_option(S_DBR, 'enable'):
        enable_data_loading = False
    else:
        enable_data_loading = config.get(S_DBR, 'enable')

    # Don't bother loading the rest of the configuration data for database
    # replication if its not enabled.
    if enable_data_loading:
        if not config.has_option(S_DBR, 'gnupg_home'):
            raise Exception("ConfigurationError: GnuPG home directory not specified. Consult installation reference manual.")

        gnupg_home = config.get(S_DBR, 'gnupg_home')

        if not config.has_option(S_DBR, 'gpg_binary'):
            gpg_binary = None
        else:
            gpg_binary = config.get(S_DBR, 'gpg_binary')

        if not config.has_option(S_DBR, 'key_fingerprint'):
            print("WARNING: No key fingerprint specified. Any valid signature will be accepted. Consult installation reference manual.")
        else:
            key_fingerprint = config.get(S_DBR, 'key_fingerprint')