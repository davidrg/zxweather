# coding=utf-8
"""
Functions for querying the database.
"""
from collections import namedtuple

__author__ = 'david'

from twisted.enterprise import adbapi
dbpool = adbapi.ConnectionPool("psycopg2",
    "host=localhost port=5432 user=zxweather password=password dbname=weather")

StationInfoRecord = namedtuple(
    'StationInfoRecord',
    ('title','description','sample_interval','live_data_available',
     'station_type_code','station_type_title'))

def _get_station_info_dict(result):
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

    deferred = dbpool.runQuery(query,(station_code,))
    deferred.addCallback(_get_station_info_dict)
    return deferred

def get_station_list():
    """
    Gets a list of all stations in the database.
    :returns: A deferred which will supply a list of (code,title) pairs.
    :rtype: Deferred
    """

    query = """select s.code, s.title from station s"""
    deferred = dbpool.runQuery(query)
    return deferred