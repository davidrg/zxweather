# coding=utf-8
from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.python import log
from zxw_push.common.database import wh1080_sample_query
from zxw_push.common.database import davis_sample_query
from zxw_push.common.database import generic_sample_query

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

    def store_live_data(self, station_code, live_record, hardware_type):
        # TODO: this
        log.msg(live_record)

    def store_sample(self, station_code, sample, hardware_type):
        # TODO: this
        log.msg(sample)

    @defer.inlineCallbacks
    def get_latest_sample(self, station_code, hw_type):
        """
        Gets the most recent sample for the specified station
        :param station_code: Station to get the sample from
        :type station_code: str
        :param hw_type: Station hardware type
        :type hw_type: str
        """

        if hw_type == 'FOWH1080':
            query = wh1080_sample_query(False, False)
        elif hw_type == 'DAVIS':
            query = davis_sample_query(False, False)
        else:  # Its GENERIC or something unsupported.
            query = generic_sample_query(False, False)

        parameters = {
            'station_code': station_code,
            'limit': 1
        }

        result = yield self._conn.runQuery(query, parameters)

        defer.returnValue(result)