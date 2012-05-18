__author__ = 'David Goodwin'

import os,sys
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
import web
import config

# Load settings.
config.load_settings()

# This is the URL structure for the site
urls = (
    '/', 'ui_route.site_index',      # This will be a list of available stations.
    '/(s|b)/', 'ui_route.stationlist',    # Index page for a particular station. Lets you
                                          # pick which UI to use.

    # Data Access (in JSON format mostly)
    '/data/(\w*)/live.json', 'data.live_data',
    # /data/station/year/month/day/... (month is in number format here)
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/datatable/(\w*).json', 'data.daily.datatable_json',
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/(?:index\.html)?', 'data.daily.index',
    '/data/(\w*)/(\d+)/(\d+)/datatable/(\w*).json', 'data.monthly.datatable_json',
    '/data/(\w*)/(\d+)/(\d+)/(?:index\.html)?', 'data.monthly.index',
    '/data/(\w*)/(\d+)/datatable/(\w*).json', 'data.yearly.datatable_json',
    '/data/(\w*)/(\d+)/(?:index\.html)?', 'data.yearly.index',
    '/data/(\w*)/(?:index\.html)?', 'data.station_index',

    # /ui/station/year/month/day
    '/(s|b)/(\w*)/(?:index\.html)?', 'ui_route.index',                 # index page
    '/(s|b)/(\w*)/now', 'ui_route.now',                # Go to todays page
    '/(s|b)/(\w*)/(\d+)/(?:index\.html)?', 'ui_route.year',            # A particular year
    '/(s|b)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'ui_route.month',     # A particular month
    '/(s|b)/(\w*)/(\d+)/(\w*)/(\d+)/indoor.html', 'ui_route.indoor_day', # indoor stats for a particular day. Basic UI only.
    '/(s|b)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'ui_route.day', # A particular day

    # Static file overlays
    '/(?:s|b)/(.*)', 'ui_route.static_overlay',
    '/(.*)', 'ui_route.static_overlay',
)




# This is so we can run it as an application to launch a development web
# server.
if __name__ == "__main__":
    app = web.application(urls, globals())
    app.run()
else:
    # No autoreloading for production
    app = web.application(urls, globals(), autoreload=False)

application = app.wsgifunc()