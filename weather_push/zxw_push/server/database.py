# coding=utf-8
from twisted.enterprise import adbapi
from twisted.internet import defer

__author__ = 'david'



class ServerDatabase(object):
    def __init__(self, dsn):
        self._conn = adbapi.ConnectionPool("psycopg2", dsn)

    @defer.inlineCallbacks
    def get_station_info(self, authorisation_code):
        # TODO: filter list of stations to only those the auth code works on

        query = """
        select s.code as code,
               st.code as hw_type
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        """

        result = yield self._conn.runQuery(query)

        defer.returnValue(result)