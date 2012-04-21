"""
Provides access to zxweather monthly data over HTTP in a number of formats.
Used for generating charts in JavaScript, etc.
"""

from datetime import date, datetime, timedelta
import web
from config import db
import config
from data.util import rfcformat, outdoor_sample_result_to_datatable

__author__ = 'David Goodwin'

### Data URLs:
# This file provides URLs to access raw data in json format.
#
# /data/{year}/{month}/datatable/samples.json
#       All samples for the month.
# /data/{year}/{month}/datatable/30m_avg_samples.json
#       30 minute averages for the month.

# TODO: round temperatures, etc.

class datatable_json:
    """
    Gets data for a particular month in Googles DataTable format.
    """

    def GET(self, station, year, month, dataset):
        if station != config.default_station_name:
            raise web.NotFound()

        year = int(year)
        month = int(month)

        if dataset == 'samples':
            return get_month_samples_datatable(year,month)
        elif dataset == '30m_avg_samples':
            return get_30m_avg_month_samples_datatable(year,month)
        else:
            raise web.NotFound()

def monthly_cache_control_headers(year,month,age):
    """
    Sets cache control headers for a daily data files.
    :param year: Year of the data file
    :type year: int
    :param month: Month of the data file
    :type month: int
    :param age: Timestamp of the last record in the data file
    :type age: datetime
    """

    now = datetime.now()
    # HTTP-1.1 Cache Control
    if year == now.year and month == now.month:
        # We should be getting a new sample every sample_interval seconds if
        # the requested day is today.
        web.header('Cache-Control', 'max-age=' + str(config.sample_interval))
        web.header('Expires',
                   rfcformat(age + timedelta(0, config.sample_interval)))
    else:
        # Old data. Never expires.
        web.header('Expires',
                   rfcformat(now + timedelta(60, 0))) # Age + 60 days
    web.header('Last-Modified', rfcformat(age))

def get_30m_avg_month_samples_datatable(year, month):

    params = dict(date = date(year,month,01))
    query_data = db.query("""select min(iq.time_stamp) as time_stamp,
       avg(iq.temperature) as temperature,
       avg(iq.dew_point) as dew_point,
       avg(iq.apparent_temperature) as apparent_temperature,
       avg(wind_chill) as wind_chill,
       avg(relative_humidity)::integer as relative_humidity,
       avg(absolute_pressure) as absolute_pressure,
       min(prev_sample_time) as prev_sample_time,
       bool_or(gap) as gap
from (
        select cur.time_stamp,
               (extract(epoch from cur.time_stamp) / 1800)::integer AS quadrant,
               cur.temperature,
               cur.dew_point,
               cur.apparent_temperature,
               cur.wind_chill,
               cur.relative_humidity,
               cur.absolute_pressure,
               cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
               CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
                  true
               else
                  false
               end as gap
        from sample cur, sample prev
        where date(date_trunc('month',cur.time_stamp)) = date(date_trunc('month',now()))
          and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
        order by cur.time_stamp asc) as iq
where date_trunc('month',iq.time_stamp) = date_trunc('month', now())
group by iq.quadrant
order by iq.quadrant asc""", params)

    data, data_age = outdoor_sample_result_to_datatable(query_data)

    monthly_cache_control_headers(year,month,data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data

def get_month_samples_datatable(year,month):

    params = dict(date = date(year,month,01))
    query_data = db.query("""select cur.time_stamp,
       cur.temperature,
       cur.dew_point,
       cur.apparent_temperature,
       cur.wind_chill,
       cur.relative_humidity,
       cur.absolute_pressure,
       cur.time_stamp - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap
from sample cur, sample prev
where date(date_trunc('month',cur.time_stamp)) = $date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
order by cur.time_stamp asc""", params)

    data, data_age = outdoor_sample_result_to_datatable(query_data)

    monthly_cache_control_headers(year,month,data_age)

    web.header('Content-Type','application/json')
    web.header('Content-Length', str(len(data)))
    return data
