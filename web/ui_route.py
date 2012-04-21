"""
The code in this file receives requests from the URL router, processes them
slightly, puts on the content-type header where required and then dispatches
to the appropriate handler function for the specified UI.
"""
from web.contrib.template import render_jinja

from basic_ui import BasicUI
import config
from modern_ui import ModernUI
import datetime
from mimetypes import guess_type
import web
import os

__author__ = 'David Goodwin'

# Register available UIs
uis = {ModernUI.ui_code(): ModernUI(),
       BasicUI.ui_code(): BasicUI(),
      }

month_name = {1 : 'january',
              2 : 'february',
              3 : 'march',
              4 : 'april',
              5 : 'may',
              6 : 'june',
              7 : 'july',
              8 : 'august',
              9 : 'september',
              10: 'october',
              11: 'november',
              12: 'december'}
month_number = {'january'  : 1,
                'february' : 2,
                'march'    : 3,
                'april'    : 4,
                'may'      : 5,
                'june'     : 6,
                'july'     : 7,
                'august'   : 8,
                'september': 9,
                'october'  : 10,
                'november' : 11,
                'december' : 12}


class site_index:
    """
    The sites index page (/). Should redirect to the default station.
    """
    def GET(self):

        if len(uis) == 1:
            # If there is only one UI registered just redirect straight to it.
            raise web.seeother(uis.items()[0][0] + '/')

        ui_list = []

        for ui in uis.items():
            ui_key = ui[1].ui_code()
            ui_name = ui[1].ui_name()
            ui_desc = ui[1].ui_description()
            ui_list.append((ui_key,ui_name,ui_desc))

        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        web.header('Content-Type','text/html')
        return render.ui_list(uis=ui_list)

class stationlist:
    """
    Index page for a particular station. Should allow you to choose how to
    view data I suppose. Or give information on the station.
    """
    def GET(self, ui):
        """
        View station details.
        :param ui: The UI to use
        :return: A view.
        """

        if ui not in uis:
            raise web.NotFound()

        # Only one station is currently supported so just redirect straight
        # to it rather than giving the user a choice of only one option.
        raise web.seeother(config.default_station_name + '/')

class index:
    """
    Index page for the station. Should give station records, basic yearly
    overview data, etc.
    """
    def GET(self, ui, station):

        if station != config.default_station_name or\
           ui not in uis:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_station(station)

class now:
    """
    Redirects to the page for today.
    """
    def GET(self, ui, station):

        if station != config.default_station_name or\
           ui not in uis:
            raise web.NotFound()

        now = datetime.datetime.now()

        todays_url = '/' + station + '/' + ui + '/' + str(now.year) + '/'\
                     + month_name[now.month] + '/' + str(now.day) + '/'

        raise web.seeother(todays_url)

class year:
    """
    Gives an overview for a year
    """
    def GET(self, ui, station, year):

        if station != config.default_station_name or\
           ui not in uis:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_year(station, int(year))

class month:
    """
    Gives an overview for a month
    """
    def GET(self, ui, station, year, month):

        if station != config.default_station_name or\
           ui not in uis or\
           month not in month_number:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_month(station, int(year), month_number[month])

class indoor_day:
    """
    Gives an overview for a day.
    """
    def GET(self, ui, station, year, month, day):
        if station != config.default_station_name or\
           ui not in uis or\
           month not in month_number:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_indoor_day(station, int(year), month_number[month], int(day))

class day:
    """
    Gives an overview for a day.
    """
    def GET(self, ui, station, year, month, day):
        if station != config.default_station_name or\
           ui not in uis or\
           month not in month_number:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_day(station, int(year), month_number[month], int(day))

class dayfile:
    def GET(self, ui, station,year,month,day,file):

        if ui not in uis:
            raise web.NotFound()

        pathname = station + '/' + str(year) + '/' + str(month)\
                   + '/' + str(day) + '/' + file
        #return pathname
        return get_file(pathname)

class monthfile:
    def GET(self, ui, station,year,month,file):

        if ui not in uis:
            raise web.NotFound()

        pathname = station + '/' + str(year) + '/' + str(month)\
                   + '/' + file
        return get_file(pathname)

class basefile:
    def GET(self, ui, file):

        if ui not in uis:
            raise web.NotFound()

        return get_file(file)

def get_file(pathname):
    """
    Streams a file back to the client.

    :param pathname: Local file to stream
    :return:
    """

    full_filename = config.static_data_dir + pathname

    if not os.path.exists(full_filename):
        raise web.NotFound()

    #filename = os.path.basename(full_filename)

    # Handle a few extensions specially. Otherwise, for example, python claims
    # png files are "image/x-png" which causes chrome to download the image
    # rather than display it.
    if pathname.endswith(".png"):
        content_type = "image/png"
    elif pathname.endswith(".dat"):
        content_type = "text/plain"
    else:
        content_type = guess_type(full_filename)[0]

    #web.header("Content-Disposition", "inline; filename=%s" % filename)
    web.header("Content-Type", content_type)
    #web.header('Transfer-Encoding','chunked')
    f = open(full_filename, 'rb')
    while 1:
        buf = f.read(1024 * 8)
        if not buf:
            break
        yield buf