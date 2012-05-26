"""
A very basic HTML3/4 UI using images for the charts rather than Javascript.
Should be compatible with just about anything.
"""

from baseui import BaseUI
from cache import year_cache_control
from data.database import year_exists, get_years

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

