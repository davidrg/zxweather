__author__ = 'David Goodwin'

import os,sys
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
import web

# This is the URL structure for the site
urls = (
    '/', 'ui_route.site_index',      # This will be a list of available stations.
    '/(\w*)/', 'ui_route.stationlist',    # Index page for a particular station. Lets you
                                          # pick which UI to use.

    # Data Access (in JSON format mostly)
    # /data/station/year/month/day/... (month is in number format here)
    '/data/(\w*)/(\d+)/(\d+)/(\d+)/datatable/(\w*).json', 'data.day_datatable_json',
    '/data/(\w*)/(\d+)/(\d+)/datatable/(\w*).json', 'data.month_datatable_json',

    # /ui/station/year/month/day
    '/(\w*)/(\w*)/(?:index\.html)?', 'ui_route.index',                 # index page
    '/(\w*)/(\w*)/now', 'ui_route.now',                # Go to todays page
    '/(\w*)/(\w*)/(\d+)/(?:index\.html)?', 'ui_route.year',            # A particular year
    '/(\w*)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'ui_route.month',     # A particular month
    '/(\w*)/(\w*)/(\d+)/(\w*)/(\d+)/indoor.html', 'ui_route.indoor_day', # indoor stats for a particular day. Basic UI only.
    '/(\w*)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'ui_route.day', # A particular day

    # Static file downloads
    '/(\w*)/(\w*)/(\d+)/(\w*)/(\d+)/(.*)', 'ui_route.dayfile',  # for day pages
    '/(\w*)/(\w*)/(\d+)/(\w*)/(.*)', 'ui_route.monthfile',  # for month pages
#    '/(\w*)/(\w*)/(\d+)/(.*)', 'baseui.file',  # for year pages
#    '/(\w*)/(\w*)/(.*)', 'baseui.file',  # UI-specific stuff
#    '/(\w*)/(.*)', 'baseui.file',  # Station-specific stuff
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