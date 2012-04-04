import datetime
from web.contrib.template import render_jinja
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

class month:
    """
    Gives an overview for a month
    """
    @staticmethod
    def GET(station, year, month):

        return "Station: '" + station\
               + "', Year: '" + year + "', Month: '" + month + "'"


def get_day(station, year, month, day):
    """
    Gives an overview for a day.
    """

    class data:
        date_stamp = datetime.date(year, month, day)
        current_data = None

    params = dict(date=data.date_stamp)
    data.records = db.select('daily_records',params, where='date_stamp = $date' )[0]

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


    return render.day(data=data)

#    return "Station: '" + station \
#           + "', Year: '" + str(year) + "', Month: '" + str(month)\
#           + "', Day: '" + str(day) + "' ThisYear: " + str(now.year)\
#           + ", ThisMon: " + str(now.month) + ", ThisDay: " + str(now.day)