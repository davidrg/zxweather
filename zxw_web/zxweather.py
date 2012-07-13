# coding=utf-8
"""
zxweather web frontend. Allows weather data to be viewed in a web browser in
various ways.
"""
__author__ = 'David Goodwin'

import os,sys
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
import web
import config

# Load settings.
config.load_settings()

# This is the URL structure for the site
urls = (
    # Data Index
    # Provides a list of available data sourcesd at the various levels.
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(?:index\.html)?', 'data.daily.index',
    '/data/(\w*)/(\d+)/(\d+)/(?:index\.html)?', 'data.monthly.index',
    '/data/(\w*)/(\d+)/(?:index\.html)?', 'data.yearly.index',
    '/data/(\w*)/(?:index\.html)?', 'data.station.index',

    # Data sources (mostly JSON)
    # /data/station/year/month/day/... (month is in number format here)
    # All code resides in the data package.
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/datatable/(\w*).json', 'data.daily.datatable_json', # Daily, DT
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(\w*).json', 'data.daily.data_json',                # Daily
    '/data/(\w*)/(\d+)/(\d+)/datatable/(\w*).json', 'data.monthly.datatable_json',     # Monthly, DT
    '/data/(\w*)/(\d+)/datatable/(\w*).json', 'data.yearly.datatable_json',            # Yearly, DT
    '/data/(\w*)/datatable/(\w*).json', 'data.station.datatable_json',                 # Station, DT
    '/data/(\w*)/(\w*).json', 'data.station.data_json',                                # Station
    '/data/daynav.json', 'data.day_nav',                                               # Global

    # User interface (HTML output).
    # /ui/station/year/month/day
    # All code resides in the ui package.
    '/(s|b|m)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'ui.day_page.day', # A particular day
    '/(s|b|m)/(\w*)/(\d+)/(\w*)/(\d+)/indoor.html', 'ui.day_page.indoor_day', # indoor stats for a particular day.
    '/(s|b|m)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'ui.month_page.month',   # A particular month
    '/(s|b|m)/(\w*)/(\d+)/(?:index\.html)?', 'ui.year_page.year',           # A particular year
    '/(s|b|m)/(\w*)/now', 'ui.now',                                         # Go to todays page
    '/(s|b|m)/(\w*)/(?:index\.html)?', 'ui.station_page.station',           # Index page
    '/(s|b|m)/(?:index\.html)?', 'ui.stationlist',                          # Station redirect
    '/(?:index\.html)?', 'ui.site_index',                                 # Site index redirect

    # Web services
    '/ws/dataload', 'webservice.data_load',     # For loading data into the database
    '/ws/latest_sample', 'webservice.latest_sample', # For loading data into the database

    # Static file overlays.
    '/(?:s|b|m)/(.*)', 'static_overlays.overlay_file',
    '/(.*)', 'static_overlays.overlay_file',
)


# This is so we can run it as an application to launch a development web
# server.
if __name__ == "__main__":
    app = web.application(urls, globals())
    app.run()
else:
    # No auto-reloading for production
    app = web.application(urls, globals(), autoreload=False)

application = app.wsgifunc()