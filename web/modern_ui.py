"""
Modern HTML5/CSS/Javascript UI.
"""

from datetime import datetime, date, timedelta
from baseui import BaseUI
from data import live_data, get_years
from data.database import day_exists, month_exists, year_exists, total_rainfall_in_last_7_days, get_daily_records
from util import relative_url

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

class ModernUI(BaseUI):
    """
    Class for the basic UI - plain old HTML 4ish output.
    """

    def __init__(self):
        BaseUI.__init__(self, 'modern_templates')

    @staticmethod
    def ui_code():
        """
        :return: URL code for the UI.
        """
        return 's'
    @staticmethod
    def ui_name():
        """
        :return: Name of the UI
        """
        return 'Standard Modern'
    @staticmethod
    def ui_description():
        """
        :return: Description of the UI.
        """
        return 'Standard interface. Not compatible with older browsers or computers.'

    @staticmethod
    def get_nav_urls(station, current_url):
        """
        Gets a dict containing standard navigation URLs relative to the
        current location in the site.
        :param station: The station we are currently working with.
        :type station: str, unicode
        :param current_url: Where in the site we currently are
        :type current_url: str
        """

        now = datetime.now().date()
        yesterday = now - timedelta(1)

        home = '/s/' + station + '/'
        yesterday = home + str(yesterday.year) + '/' + \
                    month_name[yesterday.month] + '/' + \
                    str(yesterday.day) + '/'
        this_month = home + str(now.year) + '/' + month_name[now.month] + '/'
        this_year = home + str(now.year) + '/'
        about = home + 'about.html'

        urls = {
            'home': relative_url(current_url, home),
            'yesterday': relative_url(current_url,yesterday),
            'this_month': relative_url(current_url,this_month),
            'this_year': relative_url(current_url,this_year),
            'about': relative_url(current_url, about),
        }

        return urls

    @staticmethod
    def get_day_data_urls(station, day, overview_page=False):
        """
        Gets data URLs for a day page or the station overview page.
        :param station: Station to get URLs for
        :type station: str, unicode
        :param day: Day to get URLs for
        :type day: date
        :param overview_page: If the URLs should be relative to the overview
                              page instead of the day page
        :type overview_page: bool
        """

        base_url = '/data/{0}/{1}/{2}/{3}/'.format(station,
                                                   day.year,
                                                   day.month,
                                                   day.day)
        if overview_page:
            current_url = '/s/{0}/'.format(station)
        else:
            current_url = '/s/{0}/{1}/{2}/{3}/'.format(station,
                                                       day.year,
                                                       month_name[day.month],
                                                       day.day)
        urls = {
            'samples': relative_url(current_url,
                                    base_url + 'datatable/samples.json'),
            '7day_samples': relative_url(current_url,
                                         base_url + 'datatable/7day_30m_avg_samples.json'),
            'rainfall': relative_url(current_url,
                                     base_url + 'datatable/hourly_rainfall.json'),
            '7day_rainfall': relative_url(current_url,
                                          base_url + 'datatable/7day_hourly_rainfall.json'),
            'records': relative_url(current_url, base_url + 'records.json'),
            'rainfall_totals': relative_url(current_url, base_url + 'rainfall.json'),
        }

        return urls

    def get_station(self,station):
        """
        Index page for the weather station. Should give station records and
        perhaps some basic yearly overview data.
        :param station: Name of the station to show info for.
        :return: View data.
        """

        current_location = '/s/' + station + '/'

        now = datetime.now()

        class data:
            """ Data required by the view """
            records = get_daily_records(now.date())
            years = get_years()
            year = now.year
            month_s = month_name[now.month]
            yesterday = now - timedelta(1)
            yesterday_month_s = month_name[yesterday.month]
            rainfall_7days_total = total_rainfall_in_last_7_days(now.date())

        if not day_exists(data.yesterday):
            data.yesterday = None
            data.yesterday_month_s = None

        BaseUI.day_cache_control(None, now.year, now.month, now.day)
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        data_urls = ModernUI.get_day_data_urls(station, now, True)
        return self.render.station(nav=nav_urls,
                                   data_urls=data_urls,
                                   data=data,
                                   station=station)

    def get_year(self,station, year):
        """
        Gives an overview for a year.
        :param station: Station to get data for.  Only used for building
        URLs at the moment
        :type station: string
        :param year: Page year
        :type year: integer
        :return: View data
        """
        current_location = '/s/' + station + '/' + str(year) + '/'

        class data:
            """ Data required by the view """
            this_year = year
            prev_url = None
            next_url = None

            next_year = None
            prev_year = None

            # A list of months in the year that have data
            months = BaseUI.get_year_months(year)

            # Min/max values for the year.
            records = BaseUI.get_yearly_records(year)

        # Figure out any URLs the page needs to know.
        class urls:
            """Various URLs required by the view"""
            root = '../../../'
            ui_root = '../../'
            data_base = root + 'data/{0}/{1}/'.format(station,year)
            daily_records = data_base + 'datatable/daily_records.json'

        # Figure out navigation links
        data.prev_year = year - 1
        data.next_year = year + 1
        data.year_stamp = year

        # See if any data exists for the previous and next months (no point
        # showing a navigation link if there is no data)
        if year_exists(data.prev_year):
            urls.prev_url = '../' + str(data.prev_year)

        if year_exists(data.next_year):
            urls.next_url = '../' + str(data.next_year)

        BaseUI.year_cache_control(year)
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        return self.render.year(nav=nav_urls,data=data,urls=urls)

    def get_month(self,station, year, month):
        """
        Gives an overview for a month.
        :param station: Station to get data for.  Only used for building
        URLs at the moment
        :type station: string
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :return: View data
        """
        current_location = '/s/' + station + '/' + str(year) + '/' + \
                           month_name[month] + '/'

        class data:
            """ Data required by the view """
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
                data.prev_url = '../../' + str(previous_year) + '/' +\
                                month_name[previous_month] + '/'
            else:
                data.prev_url = '../' + month_name[previous_month] + '/'

        if month_exists(next_year, next_month):
            if next_year != year:
                data.next_url = '../../' + str(next_year) + '/' +\
                                month_name[next_month] + '/'
            else:
                data.next_url = '../' + month_name[next_month] + '/'

        class urls:
            """Various URLs required by the view"""
            root = '../../../../'
            data_base = root + 'data/{0}/{1}/{2}/'.format(station,year,month)
            samples = data_base + 'datatable/samples.json'
            samples_30m_avg = data_base + 'datatable/30m_avg_samples.json'
            daily_records = data_base + 'datatable/daily_records.json'

        BaseUI.month_cache_control(year, month)
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        return self.render.month(nav=nav_urls, data=data,dataurls=urls)

    def get_indoor_day(self, station, year, month, day):
        """
        Gets a page showing temperature and humidity at the base station.

        :param station: The station to get data for.  Only used for building
        URLs at the moment
        :type station: string
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :param day: Page day
        :type day: integer
        :return: View data
        """
        current_location = '/s/' + station + '/' + str(year) + '/' + \
                           month_name[month] + '/' + str(day) + '/indoor.html'

        class data:
            """ Data required by the view """
            date_stamp = date(year, month, day)
            current_data = None
            current_data_ts = None

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.now()
        today = now.day == day and now.month == month and now.year == year

        if today:
            data.current_data_ts, data.current_data = BaseUI.get_live_indoor_data()

        class urls:
            """Various URLs required by the view"""
            data_base_url = '../../../../../data/{0}/{1}/{2}/{3}/'\
            .format(station,year,month,day)
            samples = data_base_url + 'datatable/indoor_samples.json'
            samples_7day = data_base_url + 'datatable/7day_indoor_samples.json'
            samples_7day_30mavg = data_base_url + 'datatable/7day_30m_avg_indoor_samples.json'

        BaseUI.day_cache_control(data.current_data_ts, year, month, day)
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        return self.render.indoor_day(nav=nav_urls,data=data,dataurls=urls)

    def get_day(self,station, year, month, day):
        """
        Gives an overview for a day. If the day is today then current weather
        conditions are also shown.
        :param station: Station to get the page for. Only used for building
        URLs at the moment
        :type station: string
        :param year: Page year
        :type year: integer
        :param month: Page month
        :type month: integer
        :param day: Page day
        :type day: integer
        :return: View data
        """
        current_location = '/s/' + station + '/' + str(year) + '/' +\
                           month_name[month] + '/' + str(day) + '/'

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.now()
        today = now.day == day and now.month == month and now.year == year

        class data:
            """ Data required by the view. """
            date_stamp = date(year, month, day)
            current_data = None

            prev_url = None
            prev_date = None
            next_url = None
            next_date = None
            this_month = month_name[month]
            records = get_daily_records(date_stamp)
            rainfall_7days_total = total_rainfall_in_last_7_days(date_stamp)


        # Get live data if the page is for today.
        data_age = None
        if today:
            data.current_data_ts, data.current_data = live_data.get_live_data()
            data_age = datetime.combine(date(year,month,day), data.current_data_ts)

        # Figure out the URL for the previous day
        data.prev_date = data.date_stamp - timedelta(1)

        # Only calculate previous days data if there is any.
        if day_exists(data.prev_date):

            abs_url = '/s/{0}/{1}/{2}/{3}/'.format(station,
                                                   data.prev_date.year,
                                                   month_name[data.prev_date.month],
                                                   data.prev_date.day)
            data.prev_url = relative_url(current_location, abs_url)

        # Only calculate the URL for tomorrow if there is tomorrow in the database.
        data.next_date = data.date_stamp + timedelta(1)

        if day_exists(data.next_date):
            abs_url = '/s/{0}/{1}/{2}/{3}/'.format(station,
                                                   data.next_date.year,
                                                   month_name[data.next_date.month],
                                                   data.next_date.day)
            data.next_url = relative_url(current_location, abs_url)

        BaseUI.day_cache_control(data_age, year, month, day)
        nav_urls = ModernUI.get_nav_urls(station, current_location)
        data_urls = ModernUI.get_day_data_urls(station, data.date_stamp, False)
        return self.render.day(nav=nav_urls,
                               data_urls=data_urls,
                               data=data)
