import datetime
import web
from web.contrib.template import render_jinja
from baseui import month_name, month_number
from config import db

__author__ = 'David Goodwin'

render = render_jinja('basic_templates',      # Template directory
                      encoding='utf-8')       # Encoding

class index:
    """
    Index page for the station. Should give station records, basic yearly
    overview data, etc.
    """
    @staticmethod
    def GET(station):
        return "Station: '" + station + "'"


def get_year(station, year):
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

    params = dict(year=year)
    yearly_records = db.select('yearly_records',params,
                               where='year_stamp=$year')

    if not len(yearly_records):
        # Bad url or something.
        raise web.NotFound()

    data.records = yearly_records[0]

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

    return render.year(data=data)


def get_month(station, year, month):

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


    params = dict(date='01-{0}-{1}'.format(data.month_stamp, data.year_stamp))
    monthly_records = db.select('monthly_records',params,
                              where='date_stamp = $date' )
    if not len(monthly_records):
        # Bad url or something.
        raise web.NotFound()
    data.records = monthly_records[0]

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
            data.prev_url = '../../' + str(previous_year) + '/' + \
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

    return render.month(data=data)

class indoor_day:
    def GET(self, station, year, month, day):

        class data:
            date_stamp = datetime.date(int(year), month_number[month], int(day))
            current_data = None
            current_data_ts = None

        # Figure out if there is current data to show or if this is a history
        # page
        now = datetime.datetime.now()
        params = dict(date=data.date_stamp)
        if now.day == int(day) and now.month == month_number[month] and now.year == int(year):
            # Fetch the latest data for today
            data.current_data = db.query("""select timetz(time_stamp) as time_stamp,
                indoor_relative_humidity,
                indoor_temperature
            from sample
            where date(time_stamp) = $date
            order by time_stamp desc
            limit 1""",params)[0]
            data.current_data_ts = data.current_data.time_stamp

        return render.indoor_day(data=data)

def get_day(station, year, month, day):
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

    if today:
        # Fetch the latest data for today
        data.current_data = db.query("""select timetz(time_stamp) as time_stamp, relative_humidity,
                temperature,dew_point, wind_chill, apparent_temperature,
                absolute_pressure, average_wind_speed, gust_wind_speed,
                wind_direction, rainfall
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
            data.prev_url = '../../../' + str(previous_day.year) \
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

    return render.day(data=data)

#    return "Station: '" + station \
#           + "', Year: '" + str(year) + "', Month: '" + str(month)\
#           + "', Day: '" + str(day) + "' ThisYear: " + str(now.year)\
#           + ", ThisMon: " + str(now.month) + ", ThisDay: " + str(now.day)