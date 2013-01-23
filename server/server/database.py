# coding=utf-8
"""
Functions for querying the database.
"""
from collections import namedtuple
from twisted.enterprise import adbapi

__author__ = 'david'

def database_connect(conn_str):
    """
    Connects to the database.
    :param conn_str: Database connection string
    :type conn_str: str
    """
    global database_pool
    database_pool = adbapi.ConnectionPool("psycopg2", conn_str)

    _prepare_caches()


def _init_station_id_cache(data):
    global station_code_id

    station_code_id = {}

    for row in data:
        station_code_id[row[0]] = row[1]


def _init_hw_cache(data):
    global station_code_hardware_type

    station_code_hardware_type = {}

    for row in data:
        station_code_hardware_type[row[0]] = row[1]

def _prepare_caches():
    """
    Pre-caches station IDs, codes and hardware types types
    :return:
    """
    global database_pool

    query = "select code, station_id from station"
    database_pool.runQuery(query).addCallback(_init_station_id_cache)

    query = "select s.code, st.code as hw_code from station s inner join " \
            "station_type st on st.station_type_id = s.station_type_id "
    database_pool.runQuery(query).addCallback(_init_hw_cache)


StationInfoRecord = namedtuple(
    'StationInfoRecord',
    ('title','description','sample_interval','live_data_available',
     'station_type_code','station_type_title'))

def _get_station_info_dict(result):

    if result is None or len(result) == 0:
        return None

    first = result[0]

    result = StationInfoRecord(
        title=first[0],
        description=first[1],
        sample_interval=first[2],
        live_data_available=first[3],
        station_type_code=first[4],
        station_type_title=first[5]
    )

    return result


def get_station_info(station_code):
    """
    Gets information about the specified station
    :param station_code: Station code
    :type station_code: str
    :return: Station information
    :rtype: Deferred
    """

    query = """
select s.title,
       s.description,
       s.sample_interval,
       s.live_data_available,
       st.code as station_type_code,
       st.title as station_type_title
from station s
inner join station_type st on st.station_type_id = s.station_type_id
where s.code = %s
        """

    deferred = database_pool.runQuery(query,(station_code,))
    deferred.addCallback(_get_station_info_dict)
    return deferred

def get_station_list():
    """
    Gets a list of all stations in the database.
    :returns: A deferred which will supply a list of (code,title) pairs.
    :rtype: Deferred
    """

    query = """select s.code, s.title from station s"""
    deferred = database_pool.runQuery(query)
    return deferred

def get_live_csv(station_code):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    # TODO: Handle Nulls somehow. At the moment they just result in an empty CSV row
    # TODO: convert to fixed-point data types (no point returning 2.33333333)

    query = """
select temperature || ',' ||
       dew_point || ',' ||
       apparent_temperature || ',' ||
       wind_chill || ',' ||
       relative_humidity || ',' ||
       indoor_temperature || ',' ||
       indoor_relative_humidity || ',' ||
       absolute_pressure || ',' ||
       average_wind_speed || ',' ||
       gust_wind_speed || ',' ||
       wind_direction
from live_data
where station_id = %s
    """

    return database_pool.runQuery(query, (station_code_id[station_code],))

def get_sample_csv(station_code, start_time, end_time=None):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :param start_time: Obtain all records after this timestamp. If None then
    fetch the latest sample only
    :type start_time: datetime or None.
    :param end_time: Don't get any samples from this point onwards. If None
    then this parameter is ignored.
    :type end_time: datetime or None
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    # TODO: Handle Nulls somehow. At the moment they just result in an empty CSV row
    # TODO: convert to fixed-point data types (no point returning 2.33333333)

    query_cols = """
select time_stamp,
       temperature || ',' ||
       dew_point || ',' ||
       apparent_temperature || ',' ||
       wind_chill || ',' ||
       relative_humidity || ',' ||
       indoor_temperature || ',' ||
       indoor_relative_humidity || ',' ||
       absolute_pressure || ',' ||
       average_wind_speed || ',' ||
       gust_wind_speed || ',' ||
       wind_direction || ',' ||
       rainfall
from sample
where station_id = %s
        """
    query_date = """
and time_stamp > %s
and (%s is null OR time_stamp < %s)
order by time_stamp asc
        """
    query_top = """
order by time_stamp desc
fetch first 1 rows only
    """

    if start_time is not None:
        return database_pool.runQuery(query_cols + query_date,
            (station_code_id[station_code], start_time, end_time, end_time))
    else:
        return database_pool.runQuery(query_cols + query_top,
            (station_code_id[station_code], ))

def get_station_hw_type(code):
    """
    Gets the hardware type for the specified station
    :param code: Station code
    :type code: str
    :return: The stations hardware type code
    :rtype: str
    """
    global station_code_hardware_type
    return station_code_hardware_type[code]