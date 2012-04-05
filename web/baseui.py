import datetime
import web
import basic_ui
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

valid_uis = ('m','a','b')

class index:
    """
    Index page for the station. Should give station records, basic yearly
    overview data, etc.
    """
    def GET(self, station, ui):

        if station != config.default_station_name or \
           ui not in valid_uis:
            raise web.NotFound()

        return "Station: '" + station + "', UI: '" + ui + "'"

class now:
    """
    Redirects to the page for today.
    """
    def GET(self, station, ui):

        if station != config.default_station_name or\
           ui not in valid_uis:
            raise web.NotFound()

        now = datetime.datetime.now()

        todays_url = '/' + station + '/' + ui + '/' + str(now.year) + '/' \
                    + month_name[now.month] + '/' + str(now.day) + '/'

        raise web.seeother(todays_url)

class year:
    """
    Gives an overview for a year
    """
    def GET(self, station, ui, year):

        if station != config.default_station_name or \
           ui not in valid_uis:
            raise web.NotFound()

        return "Station: '" + station + "', UI: '" + ui \
               + "', Year: '" + str(year) + "'"

class month:
    """
    Gives an overview for a month
    """
    def GET(self, station, ui, year, month):

        if station != config.default_station_name or \
           ui not in valid_uis or \
           month not in month_number:
            raise web.NotFound()

        if ui == 'b':
            return basic_ui.get_month(station, int(year), month_number[month])

        return "Station: '" + station + "', UI: '" + ui\
               + "', Year: '" + year + "', Month: '" + month + "'"

class day:
    """
    Gives an overview for a day.
    """
    def GET(self, station, ui, year, month, day):
        if station != config.default_station_name or \
           ui not in valid_uis or \
           month not in month_number:
            raise web.NotFound()

        if ui == 'b':
            return basic_ui.get_day(station, int(year), month_number[month], int(day))

        return "Station: '" + station + "', UI: '" + ui\
               + "', Year: '" + year + "', Month: '" + month \
               + "', Month: '" + day + "'"