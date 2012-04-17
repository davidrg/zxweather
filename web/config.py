__author__ = 'David Goodwin'

import web

# Database configuration
db = web.database(dbn='postgres',
                  user='zxweather',
                  pw='password',
                  db='weather')

# This is the name of the default station. In a future version
# this will live in the database instead.
default_station_name = 'hrua' # Hamilton, Ruakura

# Set this to True if you are running wh1080d to feed samples and live data
# into the database.
live_data_available = True

static_data_dir = 'Z:/current/zxweather/web/static/'