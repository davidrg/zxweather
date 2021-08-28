# coding=utf-8
"""
Database functionality shared by both the client and server
"""
from twisted.python import log

__author__ = 'david'

_PENDING_CLAUSE = """
inner join replication_status rs on rs.sample_id = s.sample_id
where rs.site_id = %(site_id)s

  -- Grab everything that is pending
  and ((%(pending)s and (rs.status = 'pending' or (
          -- And everything that has been waiting for receipt confirmation for
          -- more than 5 minutes
          rs.status = 'awaiting_confirmation'
          and rs.status_time < NOW() - '10 minutes'::interval)))
        or (not %(pending_b)s and (rs.status='done')))
"""

_TIMESTAMP_CLAUSE = """
where s.time_stamp = (%(timestamp)s at time zone 'GMT')
"""

_WHERE_CLAUSES = {
    "": "",
    "pending": _PENDING_CLAUSE,
    "timestamp": _TIMESTAMP_CLAUSE
}


def wh1080_sample_query(ascending, where_clause="pending"):
    """
    Returns the sample query for a WH1080 weather station
    :param ascending: If the query should be sorted by timestamp ascending
                      instead of descending
    :type ascending: bool
    :param where_clause: Where clause to use
    :type where_clause: str
    :returns: Sample query
    :rtype: str
    """
    query = """
       select s.sample_id as sample_id,
       upper(st.code) as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp at time zone 'GMT' as download_timestamp,
       s.time_stamp at time zone 'GMT' as time_stamp,
       wh.sample_interval as sample_interval,
       wh.record_number,
       wh.last_in_batch,
       wh.invalid_data,
       wh.wind_direction as wh1080_wind_direction,
       wh.total_rain as total_rain,
       wh.rain_overflow
from sample s
inner join station st on st.station_id = s.station_id
                         and upper(st.code) = upper(%(station_code)s)
inner join wh1080_sample wh on wh.sample_id = s.sample_id
{where_clause}
order by s.time_stamp {sort_order}
limit %(limit)s
"""

    query_format = {
        "sort_order": 'desc',
        "where_clause": _WHERE_CLAUSES[where_clause]
    }

    if ascending:
        query_format["sort_order"] = 'asc'

    return query.format(**query_format)


def davis_sample_query(ascending, where_clause="pending"):
    """
    Returns the sample query for a Davis weather station
    :param ascending: If the query should be sorted by timestamp ascending
                      instead of descending
    :type ascending: bool
    :param where_clause: Where clause to use
    :type where_clause: str
    :returns: Sample query
    :rtype: str
    """
    query = """
select s.sample_id as sample_id,
       upper(st.code) as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure, -- TODO: remove: not used by davis.
       s.mean_sea_level_pressure as msl_pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp at time zone 'GMT' as download_timestamp,
       s.time_stamp at time zone 'GMT' as time_stamp,

       ds.record_time as record_time,
       ds.record_date as record_date,
       ds.high_temperature as high_temperature,
       ds.low_temperature as low_temperature,
       ds.high_rain_rate as high_rain_rate,
       ds.solar_radiation as solar_radiation,
       ds.wind_sample_count as wind_sample_count,
       ds.gust_wind_direction as gust_wind_direction,
       ds.average_uv_index as average_uv_index,
       ds.evapotranspiration as evapotranspiration,
       ds.high_solar_radiation as high_solar_radiation,
       ds.high_uv_index as high_uv_index,
       ds.forecast_rule_id as forecast_rule_id,
       ds.leaf_wetness_1 as leaf_wetness_1,
       ds.leaf_wetness_2 as leaf_wetness_2,
       ds.leaf_temperature_1 as leaf_temperature_1,
       ds.leaf_temperature_2 as leaf_temperature_2,
       ds.soil_moisture_1 as soil_moisture_1,
       ds.soil_moisture_2 as soil_moisture_2,
       ds.soil_moisture_3 as soil_moisture_3,
       ds.soil_moisture_4 as soil_moisture_4,
       ds.soil_temperature_1 as soil_temperature_1,
       ds.soil_temperature_2 as soil_temperature_2,
       ds.soil_temperature_3 as soil_temperature_3,
       ds.soil_temperature_4 as soil_temperature_4,
       ds.extra_temperature_1 as extra_temperature_1,
       ds.extra_temperature_2 as extra_temperature_2,
       ds.extra_temperature_3 as extra_temperature_3,
       ds.extra_humidity_1 as extra_humidity_1,
       ds.extra_humidity_2 as extra_humidity_2
from sample s
inner join station st on st.station_id = s.station_id
                         and upper(st.code) = upper(%(station_code)s)
inner join davis_sample ds on ds.sample_id = s.sample_id
{where_clause}
order by s.time_stamp {sort_order}
limit %(limit)s
"""

    query_format = {
        "sort_order": 'desc',
        "where_clause": _WHERE_CLAUSES[where_clause]
    }

    if ascending:
        query_format["sort_order"] = 'asc'

    #log.msg(query.format(**query_format))
    return query.format(**query_format)


def generic_sample_query(ascending, where_clause="pending"):
    """
    Returns the sample query for a generic weather station
    :param ascending: If the query should be sorted by timestamp ascending
                      instead of descending
    :type ascending: bool
    :param where_clause: Where clause to use
    :type where_clause: str
    :returns: Sample query
    :rtype: str
    """
    query = """
select s.sample_id as sample_id,
       upper(st.code) as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure,
       s.mean_sea_level_pressure as msl_pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp at time zone 'GMT' as download_timestamp,
       s.time_stamp at time zone 'GMT' as time_stamp
from sample s
inner join station st on st.station_id = s.station_id
                         and upper(st.code) = upper(%(station_code)s)
{where_clause}
order by s.time_stamp {sort_order}
limit %(limit)s
"""

    query_format = {
        "sort_order": 'desc',
        "where_clause": _WHERE_CLAUSES[where_clause]
    }

    if ascending:
        query_format["sort_order"] = 'asc'

    return query.format(**query_format)
