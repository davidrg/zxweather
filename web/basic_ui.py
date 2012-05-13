"""
A very basic HTML3/4 UI using images for the charts rather than Javascript.
Should be compatible with just about anything.
"""

from datetime import datetime, date, timedelta
import web
from baseui import BaseUI
from config import db
from data import live_data, get_years
from data.database import year_exists, month_exists, day_exists

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

class BasicUI(BaseUI):
    """
    Class for the basic UI - plain old HTML 4ish output.
    """

    def __init__(self):
        BaseUI.__init__(self, 'basic_templates')
        pass

    @staticmethod
    def ui_code():
        """
        :return: URL code for the UI.
        """
        return 'b'
    @staticmethod
    def ui_name():
        """
        :return: Name of the UI.
        """
        return 'Basic HTML'
    @staticmethod
    def ui_description():
        """
        :return: A description of the UI.
        """
        return 'Very basic HTML UI. No JavaScript or CSS.'

    def get_station(self,station):
        """
        Index page for the weather station. Should give station records and
        perhaps some basic yearly overview data.
        :param station:
        :return:
        """

        return self.render.station(years=get_years(),station=station)

    def get_year(self,station, year):
        """
        Gives an overview for a year.
        :param station: Name of the station to get data for. Not currently used.
        :type station: string
        :param year: Page year
        :type year: integer
        :return: View data
        """

        class data:
            """ Data required by the view. """
            this_year = year
            prev_url = None
            next_url = None

            next_year = None
            prev_year = None

            # A list of months in the year that have data
            months = BaseUI.get_year_months(year)

            # Min/max values for the year.
            records = BaseUI.get_yearly_records(year)

        # Figure out navigation links
        data.prev_year = year - 1
        data.next_year = year + 1
        data.year_stamp = year

        # See if any data exists for the previous and next months (no point
        # showing a navigation link if there is no data)
        if year_exists(data.prev_year):
            data.prev_url = '../' + str(data.prev_year)

        if year_exists(data.next_year):
            data.next_url = '../' + str(data.next_year)

        BaseUI.year_cache_control(year)
        return self.render.year(data=data)

    def get_month(self,station, year, month):
        """
        Gives an overview for a month.
        :param station: Station to get data for. Not currently used.
        :type station: string
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :return: view data.
        """

        class data:
            """ Data required by the view. """
            year_stamp = year
            month_stamp = month
            current_data = None

            prev_url = None
            next_url = None

            next_month = None
            next_year = None

            prev_month = None
            prev_year = None

            this_month = month_name[month]

            # List of days in the month that have data
            days = BaseUI.get_month_days(year,month)

            # Min/Max values for the month
            records = BaseUI.get_monthly_records(year,month)


        # Figure out the previous year and month
        previous_month = data.month_stamp - 1
        previous_year = data.year_stamp

        if not previous_month:
            previous_month = 12
            previous_year -= 1

        # And the next year and month
        next_month = data.month_stamp + 1
        next_year = data.year_stamp

        if next_month > 12:
            next_month = 1
            next_year += 1

        data.prev_month = month_name[previous_month]
        data.next_month = month_name[next_month]
        data.prev_year = previous_year
        data.next_year = next_year
        data.this_year = year

        # See if any data exists for the previous and next months (no point
        # showing a navigation link if there is no data)
        if month_exists(previous_year, previous_month):
            if previous_year != year:
                data.prev_url = '../../' + str(previous_year) + '/' + \
                                month_name[previous_month] + '/'
            else:
                data.prev_url = '../' + month_name[previous_month] + '/'

        if month_exists(next_year, next_month):
            if next_year != year:
                data.next_url = '../../' + str(next_year) + '/' +\
                                month_name[next_month] + '/'
            else:
                data.next_url = '../' + month_name[next_month] + '/'

        BaseUI.month_cache_control(year,month)
        return self.render.month(data=data)

    def get_indoor_day(self, station, year, month, day):
        """
        Gets a page showing temperature and humidity at the base station.
        :param station: Station to get the page for. Not currently used.
        :type station: string
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :param day: Page day
        :type day: integer
        :return: View data
        """

        class data:
            """ Data required by the view. """
            date_stamp = date(year, month, day)
            current_data = None
            current_data_ts = None

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.now()
        today = now.day == day and now.month == month and now.year == year

        if today:
            data.current_data_ts, data.current_data = BaseUI.get_live_indoor_data()

        BaseUI.day_cache_control(data.current_data_ts,year,month,day)
        return self.render.indoor_day(data=data)

    def get_day(self,station, year, month, day):
        """
        Gives an overview for a day. If the day is today then current weather
        conditions are also shown.
        :param station: The station to get the page for. Not currently used.
        :type station: string
        :param year: The pages year
        :type year: integer
        :param month: The pages month
        :type month: integer
        :param day: The pages day
        :type day: integer
        """

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.now()
        today = False
        if now.day == day and now.month == month and now.year == year:
            today = True

        class data:
            """
            Data to be passed to the view.
            """
            date_stamp = date(year, month, day)
            current_data = None

            prev_url = None
            prev_date = None
            next_url = None
            next_date = None
            this_month = month_name[month]


        params = dict(date=data.date_stamp)
        daily_records = db.select('daily_records',params, where='date_stamp = $date' )
        if not len(daily_records):
            if today:
                # We just don't have any records yet.
                return "Sorry! There isn't any data for today yet. Check back in a few minutes."
            else:
                # Bad url or something.
                raise web.NotFound()
        data.records = daily_records[0]

        # Get live data if the page is for today.
        data_age = None
        if today:
            data.current_data_ts, data.current_data = live_data.get_live_data()
            data_age = datetime.combine(date(year,month,day), data.current_data_ts)

        # Figure out the URL for the previous day
        data.prev_date = data.date_stamp - timedelta(1)
        data.next_date = data.date_stamp + timedelta(1)

        # Only calculate previous days data if there is any.
        if day_exists(data.prev_date):
            if data.prev_date.year != year:
                data.prev_url = '../../../' + str(data.prev_date.year) \
                                + '/' + month_name[data.prev_date.month] + '/'
            elif data.prev_date.month != month:
                data.prev_url = '../../' + month_name[data.prev_date.month] + '/'
            else:
                data.prev_url = '../'
            data.prev_url += str(data.prev_date.day) + '/'

        # Only calculate the URL for tomorrow if there is tomorrow in the database.
        if day_exists(data.next_date):
            if data.next_date.year != year:
                data.next_url = '../../../' + str(data.next_date.year)\
                                + '/' + month_name[data.next_date.month] + '/'
            elif data.next_date.month != month:
                data.next_url = '../../' + month_name[data.next_date.month] + '/'
            else:
                data.next_url = '../'
            data.next_url += str(data.next_date.day) + '/'

        BaseUI.day_cache_control(data_age, year, month, day)
        return self.render.day(data=data)
