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
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/datatable/(\w*).json', 'data.daily.datatable_json', # Daily, DT
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(\w*).json', 'data.daily.data_json',                # Daily
    '/data/(\w*)/(\d+)/(\d+)/datatable/(\w*).json', 'data.monthly.datatable_json',     # Monthly, DT
    '/data/(\w*)/(\d+)/datatable/(\w*).json', 'data.yearly.datatable_json',            # Yearly, DT
    '/data/(\w*)/datatable/(\w*).json', 'data.station.datatable_json',                 # Station, DT
    '/data/(\w*)/(\w*).json', 'data.station.data_json',                                # Station
    '/data/daynav.json', 'data.day_nav',                                               # Global

    # User interface (HTML output)
    # /ui/station/year/month/day
    '/(s|b)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'day_page.day', # A particular day
    '/(s|b)/(\w*)/(\d+)/(\w*)/(\d+)/indoor.html', 'day_page.indoor_day', # indoor stats for a particular day.
    '/(s|b)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'ui_route.month',     # A particular month
    '/(s|b)/(\w*)/(\d+)/(?:index\.html)?', 'ui_route.year',            # A particular year
    '/(s|b)/(\w*)/now', 'ui_route.now',                                # Go to todays page
    '/(s|b)/(\w*)/(?:index\.html)?', 'ui_route.index',                 # Index page
    '/(s|b)/(?:index\.html)?', 'ui_route.stationlist',                 # Station redirect
    '/(?:index\.html)?', 'ui_route.site_index',                        # Site index redirect

    # Static file overlays.
    '/(?:s|b)/(.*)', 'static_overlays.overlay_file',
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