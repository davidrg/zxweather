"""
A very basic HTML3/4 UI using images for the charts rather than Javascript.
Should be compatible with just about anything.
"""

from baseui import BaseUI
from cache import year_cache_control, month_cache_control
from data.database import year_exists, month_exists,  get_years
from util import month_name

__author__ = 'David Goodwin'

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

        year_cache_control(year)
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

        month_cache_control(year,month)
        return self.render.month(data=data)
