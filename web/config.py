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