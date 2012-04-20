import datetime
import web
from baseui import BaseUI
from config import db, live_data_available

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
        return 's'
    @staticmethod
    def ui_name():
        return 'Standard Modern'
    @staticmethod
    def ui_description():
        return 'Standard interface.'

    def get_station(self,station):
        """
        Index page for the weather station. Should give station records and
        perhaps some basic yearly overview data.
        :param station:
        :return:
        """

        years_result = db.query("select distinct extract(year from time_stamp) as year from sample order by year desc")
        years = []
        for record in years_result:
            years.append(int(record.year))

        return self.render.station(years=years,station=station)

    def get_year(self,station, year):
        """
        Gives an overview for a year.
        :param station:
        :param year:
        :return:
        """

        # TODO: Set the Expires header to now + interval if this is a live day.
        # TODO: Set the Last-Modified header to the timestamp of the most recent sample on the page.

        class data:
            this_year = year
            prev_url = None
            next_url = None

            next_year = None
            prev_year = None

            months = []

        params = dict(year=year)
        yearly_records = db.select('yearly_records',params,
                                   where='year_stamp=$year')

        if not len(yearly_records):
            # Bad url or something.
            raise web.NotFound()

        data.records = yearly_records[0]

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.month_stamp::integer from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp
    from sample
    group by year_stamp, month_stamp) as md where md.year_stamp = $year""", dict(year=year))


        if not len(month_data):
            # If there are no months in this year then there can't be any data.
            raise web.NotFound()

        for month in month_data:
            the_month_name = month_name[month.month_stamp]
            data.months.append(the_month_name)

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
            data.prev_url = '../' + str(data.prev_year)

        data_check = db.query("""select 'foo' from sample
        where extract(year from time_stamp) = $year limit 1""",
                              dict(year=data.next_year))
        if len(data_check):
            data.next_url = '../' + str(data.next_year)

        return self.render.year(data=data)

    def get_month(self,station, year, month):

        """
        Gives an overview for a month.
        """

        # TODO: Set the Expires header to now + interval if this is a live day.
        # TODO: Set the Last-Modified header to the timestamp of the most recent sample on the page.

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.datetime.now()

        class data:
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

            days = []


        params = dict(date='01-{0}-{1}'.format(data.month_stamp, data.year_stamp))
        monthly_records = db.select('monthly_records',params,
                                    where='date_stamp = $date' )
        if not len(monthly_records):
            # Bad url or something.
            raise web.NotFound()
        data.records = monthly_records[0]

        # Grab a list of all months for which there is data for this year.
        month_data = db.query("""select md.day_stamp from (select extract(year from time_stamp) as year_stamp,
           extract(month from time_stamp) as month_stamp,
           extract(day from time_stamp) as day_stamp
    from sample
    group by year_stamp, month_stamp, day_stamp) as md where md.year_stamp = $year and md.month_stamp = $month
    order by day_stamp""", dict(year=year, month=month))

        if not len(month_data):
            # If there are no months in this year then there can't be any data.
            raise web.NotFound()

        for day in month_data:
            day = int(day.day_stamp)
            data.days.append(day)

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

        return self.render.month(data=data)

    def get_indoor_day(self, station, year, month, day):

        class data:
            date_stamp = datetime.date(year, month, day)
            current_data = None
            current_data_ts = None

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.datetime.now()
        params = dict(date=data.date_stamp)
        today = False
        if now.day == day and now.month == month and now.year == year:
            today = True
        if today and live_data_available:
            data.current_data = db.query("""select timetz(download_timestamp) as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
                from live_data""")[0]
            data.current_data_ts = data.current_data.time_stamp
        elif today:
            # Fetch the latest data for today
            data.current_data = db.query("""select timetz(time_stamp) as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
            from sample
            where date(time_stamp) = $date
            order by time_stamp desc
            limit 1""",params)[0]
            data.current_data_ts = data.current_data.time_stamp

        return self.render.indoor_day(data=data)

    def get_day(self,station, year, month, day):
        """
        Gives an overview for a day. If the day is today then current weather
        conditions are also shown.
        """

        # TODO: Set the Expires header to now + interval if this is a live day.
        # TODO: Set the Last-Modified header to the timestamp of the most recent sample on the page.

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.datetime.now()
        today = False
        if now.day == day and now.month == month and now.year == year:
            today = True

        class data:
            date_stamp = datetime.date(year, month, day)
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

        if today and live_data_available:
            # No need to filter or anything - live_data only contains one record.
            data.current_data = db.query("""select timetz(download_timestamp) as time_stamp,
                    invalid_data, relative_humidity, temperature, dew_point,
                    wind_chill, apparent_temperature, absolute_pressure,
                    average_wind_speed, gust_wind_speed, wind_direction
                    from live_data""")[0]
            data.current_data_ts = data.current_data.time_stamp
        elif today:
            # Fetch the latest data for today
            data.current_data = db.query("""select timetz(time_stamp) as time_stamp, relative_humidity,
                    temperature,dew_point, wind_chill, apparent_temperature,
                    absolute_pressure, average_wind_speed, gust_wind_speed,
                    wind_direction, invalid_data
                from sample
                where date(time_stamp) = $date
                order by time_stamp desc
                limit 1""",params)[0]
            data.current_data_ts = data.current_data.time_stamp

        # Figure out the URL for the previous day
        previous_day = data.date_stamp - datetime.timedelta(1)
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
        next_day = data.date_stamp + datetime.timedelta(1)
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
            base_url = '../../../../../data/{0}/{1}/{2}/{3}/'\
                       .format(station,year,month,day)
            samples = base_url + 'datatable/samples.json'

        return self.render.day(data=data,dataurls=urls)
