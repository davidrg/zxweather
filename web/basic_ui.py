import datetime
import web
from web.contrib.template import render_jinja
from baseui import month_name
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

class year:
    """
    Gives an overview for a year
    """
    @staticmethod
    def GET(station, year):

        return "Station: '" + station \
               + "', Year: '" + str(year) + "'"


def get_month(station, year, month):

    class data:
        this_month = month_name[month]
        this_year = year

        records = None

        prev_url = None
        prev_month = None
        next_url = None
        next_month = None

    return render.month(data=data)

def get_day(station, year, month, day):
    """
    Gives an overview for a day. If the day is today then current weather
    conditions are also shown.
    """

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
        raise web.NotFound()
    data.records = daily_records[0]

    # Figure out if there is current data to show or if this is a history
    # page
    now = datetime.datetime.now()
    if now.day == day and now.month == month and now.year == year:
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