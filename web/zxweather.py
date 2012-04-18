__author__ = 'David Goodwin'

import os,sys
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
import web
import config

# This is the URL structure for the site
urls = (
    '/', 'site_index',      # This will be a list of available stations.
    '/(\w*)/', 'station',    # Index page for a particular station. Lets you
                            # pick which UI to use.

    # /station/ui/year/month/day
    '/(\w*)/(\w*)/(?:index\.html)?', 'baseui.index',                 # index page
    '/(\w*)/(\w*)/now', 'baseui.now',                # Go to todays page
    '/(\w*)/(\w*)/(\d+)/(?:index\.html)?', 'baseui.year',            # A particular year
    '/(\w*)/(\w*)/(\d+)/(\w*)/(?:index\.html)?', 'baseui.month',     # A particular month
    '/(\w*)/b/(\d+)/(\w*)/(\d+)/indoor.html', 'basic_ui.indoor_day', # indoor stats for a particular day. Basic UI only.
    '/(\w*)/(\w*)/(\d+)/(\w*)/(\d+)/(?:index\.html)?', 'baseui.day', # A particular day

    # Static file downloads
    '/(\w*)/(\w*)/(\d+)/(\w*)/(\d+)/(.*)', 'baseui.dayfile',  # for day pages
    '/(\w*)/(\w*)/(\d+)/(\w*)/(.*)', 'baseui.monthfile',  # for month pages
#    '/(\w*)/(\w*)/(\d+)/(.*)', 'baseui.file',  # for year pages
#    '/(\w*)/(\w*)/(.*)', 'baseui.file',  # UI-specific stuff
#    '/(\w*)/(.*)', 'baseui.file',  # Station-specific stuff
)


class site_index:
    """
    The sites index page (/). Should redirect to the default station.
    """
    def GET(self):
        raise web.seeother(config.default_station_name + '/')

class station:
    """
    Index page for a particular station. Should allow you to choose how to
    view data I suppose. Or give information on the station.
    """
    def GET(self, station_name):
        """
        View station details.
        :param url: The station to view
        :return: A view.
        """

        if station_name == config.default_station_name:
            return self._render_station(station_name)
        else:
            raise web.NotFound(message="The station '" + station_name +
                                        "' does not exist.")

    def _render_station(self, station_name):
        web.header('Content-Type','text/plain')
        return "Station info goes here. You are at /hrua/"

# This is so we can run it as an application to launch a development web
# server.
if __name__ == "__main__":
    app = web.application(urls, globals())
    app.run()
else:
    # No autoreloading for production
    app = web.application(urls, globals(), autoreload=False)

application = app.wsgifunc()