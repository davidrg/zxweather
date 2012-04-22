"""
Modern HTML5/CSS/Javascript UI.
"""

from datetime import datetime, date, timedelta
import web
from baseui import BaseUI
from config import db

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

    def get_station(self,station):
        """
        Index page for the weather station. Should give station records and
        perhaps some basic yearly overview data.
        :param station: Name of the station to show info for.
        :return: View data.
        """

        years_result = db.query("select distinct extract(year from time_stamp) as year from sample order by year desc")
        years = []
        for record in years_result:
            years.append(int(record.year))

        return self.render.station(years=years,station=station)

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
        data_check = db.query("""select 'foo' from sample
        where extract(year from time_stamp) = $year limit 1""",
                              dict(year=data.prev_year))
        if len(data_check):
            urls.prev_url = '../' + str(data.prev_year)

        data_check = db.query("""select 'foo' from sample
        where extract(year from time_stamp) = $year limit 1""",
                              dict(year=data.next_year))
        if len(data_check):
            urls.next_url = '../' + str(data.next_year)

        BaseUI.year_cache_control(year)
        return self.render.year(data=data,urls=urls)



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
        data_check = db.query("""select 'foo' from sample
        where date(date_trunc('month',time_stamp)) = $date limit 1""",
                              dict(date='01-{0}-{1}'.format(previous_month,previous_year)))
        if len(data_check):
            if previous_year != year:
                data.prev_url = '../../' + str(previous_year) + '/' +\
                                month_name[previous_month] + '/'
            else:
                data.prev_url = '../' + month_name[previous_month] + '/'

        data_check = db.query("""select 'foo' from sample
            where date(date_trunc('month',time_stamp)) = $date limit 1""",
                              dict(date='01-{0}-{1}'.format(next_month,next_year)))
        if len(data_check):
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
        return self.render.month(data=data,dataurls=urls)

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
        return self.render.indoor_day(data=data,dataurls=urls)

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
        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.now()
        today = False
        if now.day == day and now.month == month and now.year == year:
            today = True

        class data:
            """ Data required by the view. """
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
            data.current_data_ts, data.current_data = BaseUI.get_live_data()
            data_age = datetime.combine(date(year,month,day), data.current_data_ts)

        # Figure out the URL for the previous day
        previous_day = data.date_stamp - timedelta(1)
        data.prev_date = previous_day
        prev_days_data = db.query("""select temperature
            from sample where date(time_stamp) = $date limit 1""",
                                  dict(date=previous_day))
        # Only calculate previous days data if there is any.
        if len(prev_days_data):
            if previous_day.year != year:
                data.prev_url = '../../../' + str(previous_day.year)\
                                + '/' + month_name[previous_day.month] + '/'
            elif previous_day.month != month:
                data.prev_url = '../../' + month_name[previous_day.month] + '/'
            else:
                data.prev_url = '../'
            data.prev_url += str(previous_day.day) + '/'

        # Only calculate the URL for tomorrow if there is tomorrow in the database.
        next_day = data.date_stamp + timedelta(1)
        data.next_date = next_day
        next_days_data = db.query("""select temperature
            from sample where date(time_stamp) = $date limit 1""",
                                  dict(date=next_day))
        if len(next_days_data):
            if next_day.year != year:
                data.next_url = '../../../' + str(next_day.year)\
                                + '/' + month_name[next_day.month] + '/'
            elif next_day.month != month:
                data.next_url = '../../' + month_name[next_day.month] + '/'
            else:
                data.next_url = '../'
            data.next_url += str(next_day.day) + '/'

        class urls:
            """
            Various URLs required by the view.
            """
            base_url = '../../../../../data/{0}/{1}/{2}/{3}/'\
                       .format(station,year,month,day)
            samples = base_url + 'datatable/samples.json'
            samples_7day = base_url + 'datatable/7day_samples.json'
            samples_7day_30mavg = base_url + 'datatable/7day_30m_avg_samples.json'

        BaseUI.day_cache_control(data_age, year, month, day)
        return self.render.day(data=data,dataurls=urls)
