"""
The code in this file receives requests from the URL router, processes them
slightly, puts on the content-type header where required and then dispatches
to the appropriate handler function for the specified UI.
"""

import datetime
from mimetypes import guess_type
import os
import web
from basic_ui import BasicUI
import config

__author__ = 'David Goodwin'

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

# Register available UIs
uis = {BasicUI.ui_code(): BasicUI()}

class index:
    """
    Index page for the station. Should give station records, basic yearly
    overview data, etc.
    """
    def GET(self, ui, station):

        if station != config.default_station_name or \
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

        todays_url = '/' + station + '/' + ui + '/' + str(now.year) + '/' \
                    + month_name[now.month] + '/' + str(now.day) + '/'

        raise web.seeother(todays_url)

class year:
    """
    Gives an overview for a year
    """
    def GET(self, ui, station, year):

        if station != config.default_station_name or \
           ui not in uis:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_year(station, int(year))

class month:
    """
    Gives an overview for a month
    """
    def GET(self, ui, station, year, month):

        if station != config.default_station_name or \
           ui not in uis or \
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
        if station != config.default_station_name or \
           ui not in uis or \
           month not in month_number:
            raise web.NotFound()

        web.header('Content-Type','text/html')
        return uis[ui].get_day(station, int(year), month_number[month], int(day))

class dayfile:
    def GET(self, ui, station,year,month,day,file):
        pathname = station + '/' + str(year) + '/' + str(month) \
                   + '/' + str(day) + '/' + file
        #return pathname
        return get_file(pathname)

class monthfile:
    def GET(self, ui, station,year,month,file):
        pathname = station + '/' + str(year) + '/' + str(month)\
                   + '/' + file
        return get_file(pathname)

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