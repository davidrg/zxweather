# coding=utf-8
"""
zxweather web frontend. Allows weather data to be viewed in a web browser in
various ways.
"""

__author__ = 'David Goodwin'

import os,sys
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
import config
from web.application import application as web_application

# Load settings.
config.load_settings()

# This is the URL structure for the site
urls = (
    # Data Index
    # Provides a list of available data sourcesd at the various levels.
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/images/(\w*)/(?:index\.html)?', 'data.daily_index.images',
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(?:index\.html)?', 'data.daily_index.index',
    '/data/(\w*)/(\d+)/(\d+)/(?:index\.html)?', 'data.monthly.index',
    '/data/(\w*)/(\d+)/(?:index\.html)?', 'data.yearly.index',
    '/data/(\w*)/images/(\w*)/(?:index\.html)?', 'data.station.images',
    '/data/(\w*)/(?:index\.html)?', 'data.station.index',
    '/data/(?:index\.html)?', 'data.data_index',

    # Data sources (mostly JSON)
    # /data/station/year/month/day/... (month is in number format here)
    # All code resides in the data package.
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/images/(\w*)/(\d+)/(\w*).(\w*)', 'data.daily.image', # Daily, image
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/datatable/(\w*).json', 'data.daily.dt_json',     # Daily, DT
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(\w*).txt', 'data.daily.data_ascii',             # Daily
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(\w*).dat', 'data.daily.data_dat',               # Daily
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(\w*).json', 'data.daily.data_json',             # Daily
    '/data/(\w*)/(\d+)/(\d+)/datatable/(\w*).json', 'data.monthly.datatable_json',  # Monthly, DT
    '/data/(\w*)/(\d+)/(\d+)/(\w*).txt', 'data.monthly.data_ascii',                 # Monthly,
    '/data/(\w*)/(\d+)/(\d+)/(\w*).dat', 'data.monthly.data_dat',                   # Monthly,
    '/data/(\w*)/(\d+)/(\d+)/(\w*).json', 'data.monthly.data_json',                 # Monthly,
    '/data/(\w*)/(\d+)/datatable/(\w*).json', 'data.yearly.datatable_json',         # Yearly, DT
    '/data/(\w*)/(\w*).txt', 'data.station.data_ascii',                             # Station
    '/data/(\w*)/(\d+)/(\w*).json', 'data.yearly.data_json',                        # Yearly
    '/data/(\w*)/datatable/(\w*).json', 'data.station.datatable_json',              # Station, DT
    '/data/(\w*)/images/(\w*)/latest/(\w*)', 'data.station.latest_image',           # Link to latest image
    '/data/(\w*)/(\w*).json', 'data.station.data_json',                             # Station
    '/data/(\w*).json', 'data.data_json',                                           # Global


    # User interface (HTML output).
    # /ui/station/year/month/day
    # All code resides in the ui package.
    '/(s|b|m|a)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'ui.day_page.day', # A particular day
    '/(s|b|m|a)/(\w*)/(\d+)/(\w*)/(\d+)/indoor.html', 'ui.day_page.indoor_day', # indoor stats for a particular day.
    '/(?:s|b|m|a)/(?:\w*)/(?:\d+)/(?:\w*)/(?:\d+)', 'ui.dir_redirect',        # Redirect: day
    '/(s|b|m|a)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'ui.month_page.month',   # A particular month
    '/(s|b|m|a)/(\w*)/(\d+)/(?:index\.html)?', 'ui.year_page.year',           # A particular year
    '/(?:s|b|m|a)/(?:\w*)/(?:\d+)/(?:\w*)', 'ui.dir_redirect',                # Redirect: month
    '/(?:s|b|m|a)/(?:\w*)/(?:\d+)', 'ui.dir_redirect',                        # Redirect: year
    '/(s|b|m|a)/(\w*)/now', 'ui.now',                                         # Go to todays page
    '/(s|b|m|a)/(\w*)/reception\.html', 'ui.station_page.reception',          # Reception page
    '/(s|b|m|a)/(\w*)/images\.html', 'ui.station_page.all_images',            # All images for a station
    '/(s|b|m|a)/(\w*)/(?:index\.html)?', 'ui.station_page.station',           # Index page
    '/(?:s|b|m|a)/(?:\w+)', 'ui.dir_redirect',                                # Redirect: station
    '/(?:s|m|a)/(\w*)/about.json', 'data.about_nav',                          #
    '/(s|b|m|a)/(?:index\.html)?', 'ui.stationlist',                          # Station redirect
    '/(?:s|b|m|a)', 'ui.dir_redirect',                                        # Redirect: UI
    '/(?:index\.html)?', 'ui.site_index',                                 # Site index redirect

    # Static file overlays.
    '/(?:s|b|m|a)/(.*)', 'static_overlays.overlay_file',
    '/(.*)', 'static_overlays.overlay_file',
)


# This is so we can run it as an application to launch a development web
# server.
if __name__ == "__main__":
    app = web_application(urls, globals())
    app.run()
else:
    # No auto-reloading for production
    app = web_application(urls, globals(), autoreload=False)

application = app.wsgifunc()