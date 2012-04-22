__author__ = 'David Goodwin'

import web

# Database configuration
db = web.database(dbn='postgres',
                  user='zxweather',
                  pw='password',
                  db='weather')

# This is the name of the default station. In a future version
# this will live in the database instead.
default_station_name = 'rua' # Ruakura, Hamilton

# Set this to True if you are running wh1080d to feed samples and live data
# into the database.
live_data_available = True

# How often new samples appear from the weather station, in seconds.
sample_interval = 300 # 5 minutes

# How often charts are re-plotted, in seconds
plot_interval = 1800

# Where static data lives
static_data_dir = 'Z:/current/zxweather/web/static/'

# Root directory of the site. Required when doing 301/302 redirects.
site_root = "http://localhost:8080/"

# Set to None to use the UI picker page.
default_ui = 's'