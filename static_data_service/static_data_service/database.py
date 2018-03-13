# coding=utf-8
"""
Database listener for live data
"""
import json

from datetime import date
from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.error import ReactorNotRunning
from twisted.python import log
from twisted.internet.defer import inlineCallbacks, returnValue
from util import Event
import psycopg2
from psycopg2.extras import DictConnection as Psycopg2DictConn
from txpostgres import txpostgres

__author__ = 'david'


SAMPLE_QUERY = """
select cur.time_stamp,
       round(cur.temperature::numeric, 2) as temperature,
       round(cur.dew_point::numeric, 1) as dew_point,
       round(cur.apparent_temperature::numeric, 1) as apparent_temperature,
       round(cur.wind_chill::numeric,1) as wind_chill,
       cur.relative_humidity,
       round(cur.absolute_pressure::numeric,2) as absolute_pressure,
       round(cur.indoor_temperature::numeric,2) as indoor_temperature,
       cur.indoor_relative_humidity,
       round(cur.rainfall::numeric, 1) as rainfall,
       round(cur.average_wind_speed::numeric,2) as average_wind_speed,
       round(cur.gust_wind_speed::numeric,2) as gust_wind_speed,
       cur.wind_direction,

       ds.average_uv_index as uv_index,
       ds.solar_radiation,
       
       case when %(broadcast_id)s is null then null
       --                                  |----- Max wireless packets calculation -----------------------|
       else round((ds.wind_sample_count / ((s.sample_interval::float) /((41+%(broadcast_id)s-1)::float /16.0 )) * 100)::numeric,1)::float 
       end as reception,
       round(ds.high_temperature::numeric, 2) as high_temperature,
       round(ds.low_temperature::numeric, 2) as low_temperature,
       round(ds.high_rain_rate::numeric, 2) as high_rain_rate,
       ds.gust_wind_direction,
       ds.evapotranspiration,
       ds.high_solar_radiation,
       ds.high_uv_index,
       ds.forecast_rule_id
from sample cur
inner join sample prev on prev.station_id = cur.station_id
        and prev.time_stamp = (
            select max(pm.time_stamp)
            from sample pm
            where pm.time_stamp < cur.time_stamp
            and pm.station_id = cur.station_id)
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id"""


def dict_connect(*args, **kwargs):
    """
    Opens a psycopg2 database connection using the Dict cursor
    :param kwargs:
    :param args:
    :return:
    """
    kwargs['connection_factory'] = Psycopg2DictConn
    return psycopg2.connect(*args, **kwargs)


class DictConnection(txpostgres.Connection):
    """
    Psycopg2 connection subclass using the dict cursor
    """
    connectionFactory = staticmethod(dict_connect)


class DatabaseReceiver(object):
    """
    Provides access to live data in a weather database.
    """

    def __init__(self, dsn):
        """
        """
        self.NewSample = Event()
        self._connection_string = dsn
        #self._database_pool = None
        self._conn = None
        self._conn_d = None
        self._broadcast_ids = {}

    def _fetch_sample(self, sample_info):
        """
        Fetch a new sample from the database
        :param sample_info: Sample information - "station-code:sample-id"
        :type sample_info: str
        :return: 
        """
        # sample_info is "station_code:sample_id"
        bits = sample_info.split(":")
        station_code = bits[0]
        sample_id = int(bits[1])

        broadcast_id = None
        if station_code in self._broadcast_ids:
            broadcast_id = self._broadcast_ids[station_code]

        query = SAMPLE_QUERY + """
where cur.sample_id = %(sample_id)s"""

        def _process_result(result):
            if len(result) > 0:
                self.NewSample.fire(station_code, result[0])

        self._conn.runQuery(query,
                            {"broadcast_id": broadcast_id,
                             "sample_id": sample_id}).addCallback(_process_result)

    def _get_station_config(self):
        query = """
        select s.code, s.station_config, t.code as hw_type
        from station s
        inner join station_type t on t.station_type_id = s.station_type_id
        """

        self._conn.runQuery(query).addCallback(self._store_broadcast_ids)

    def _store_broadcast_ids(self, result):
        self._broadcast_ids = {}
        for row in result:
            station = row[0]
            config = row[1]
            hw_type = row[2]

            if hw_type == 'DAVIS':
                doc = json.loads(config)
                broadcast_id = doc["broadcast_id"]
                self._broadcast_ids[station] = broadcast_id

    def observer(self, notify):
        """
        Called when ever notifications are received.
        :param notify: The notification.
        """

        if notify.channel == "new_sample_id":
            self._fetch_sample(notify.payload)

    def connect(self):
        """
        Connects to the database and listening for live data.
        """

        log.msg('Connect: {0}'.format(self._connection_string))
        #self._conn = DictConnection()
        self._conn = txpostgres.Connection()
        self._conn_d = self._conn.connect(self._connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self.observer)

        self._conn_d.addCallback(lambda _: self._get_station_config())
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen new_sample_id"))
        self._conn_d.addCallback(
            lambda _: log.msg('Connected to database. Now waiting for data.'))
        #
        # self._database_pool = adbapi.ConnectionPool("psycopg2",
        #                                             self._connection_string)

    def reconnect(self):
        self._conn.close()
        self.connect()


class Database(object):
    def __init__(self, dsn):
        self._dsn = dsn
        self._database_pool = None

        self.DatabaseReady = Event()

    def connect(self):
        log.msg("Primary database connect...")
        self._database_pool = adbapi.ConnectionPool("psycopg2",
                                                    self._dsn)
        self._check_db()

    def reconnect(self):
        self._database_pool.close()
        self.connect()

    @defer.inlineCallbacks
    def _get_schema_version(self):
        result = yield self._database_pool.runQuery(
            "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")

        if len(result) == 0:
            returnValue(1)

        result = yield self._database_pool.runQuery(
            "select v::integer from db_info where k = 'DB_VERSION'")

        returnValue(result[0][0])

    @defer.inlineCallbacks
    def _check_db(self):
        """
        Checks the database is compatible
        :return:
        """
        schema_version = yield self._get_schema_version()
        log.msg("Database schema version: {0}".format(schema_version))

        if schema_version < 3:
            log.msg("*** ERROR: static data service requires at least database "
                    "version 3 (zxweather 1.0.0). Please upgrade your "
                    "database.")
            try:
                reactor.stop()
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                pass
        else:
            result = yield self._database_pool.runQuery(
                "select version_check('SDSVC',1,0,0)")
            if not result[0][0]:
                result = yield self._database_pool.runQuery(
                    "select minimum_version_string('SDSVC')")

                log.msg("*** ERROR: This version of the static data service is "
                        "incompatible with the configured database. The "
                        "minimum static data service (or zxweather) version "
                        "supported by this database is: {0}.".format(
                            result[0][0]))
                try:
                    reactor.stop()
                except ReactorNotRunning:
                    # Don't care. We wanted it stopped, turns out it already is
                    # that or its not yet started in which case there isn't
                    # anything much we can do to terminate yet.
                    pass

        # Database checks ok.
        self.DatabaseReady.fire()

    @defer.inlineCallbacks
    def get_month_samples(self, station_code, broadcast_id, year, month):
        query = SAMPLE_QUERY + """
where date(date_trunc('month',cur.time_stamp)) = %(date)s
  and s.code = %(station)s"""

        result = yield self._database_pool.runQuery(
            query, {"broadcast_id": broadcast_id, "station": station_code,
                    "date": date(year, month, 1)}
        )

        returnValue(result)

    @defer.inlineCallbacks
    def get_earliest_sample_timestamp(self, station_code):
        query = "select min(time_stamp) from sample s " \
                "inner join station stn on stn.station_id = s.station_id " \
                "where lower(stn.code) = lower(%(station)s)"

        result = yield self._database_pool.runQuery(query,
                                                    {"station": station_code})

        returnValue(result[0][0])

    @inlineCallbacks
    def get_station_codes(self):
        """
        Returns a list of station-code, broadcast-id pairings. Non-wireless and
        non-davis stations will have a None broadcast ID.
        
        :return: 
        """

        query = "select s.code, st.code as hw_type, s.station_config " \
                "from station s " \
                "inner join station_type st on st.station_type_id = s.station_type_id"
        result = yield self._database_pool.runQuery(query)

        stations = []

        for row in result:
            code = row[0]
            hw_type = row[1]
            hw_config = row[2]
            broadcast_id = None
            has_solar = False
            if hw_type.upper() == "DAVIS":
                config_doc = json.loads(hw_config)
                if config_doc["is_wireless"] and "broadcast_id" in config_doc:
                    broadcast_id = config_doc["broadcast_id"]
                if "has_solar_and_uv" in config_doc:
                    has_solar = config_doc["has_solar_and_uv"]

            stations.append((code, broadcast_id, has_solar))

        returnValue(stations)

    @inlineCallbacks
    def get_station_info(self):
        """
        Gets information required by sysconfig.json for all stations in the
        database.
        """

        query = """
select stn.code,
       -- stn.message_timestamp
       stn.station_config,
       st.code,
       st.title,
       stn.title,
       stn.latitude,
       stn.longitude,
       -- range max
       -- range min
       stn.site_title,
       --stn.message,
       stn.description,
       stn.sort_order
from station stn
inner join station_type st on st.station_type_id = stn.station_type_id
        """

        result = yield self._database_pool.runQuery(query)

        returnValue(result)

    @inlineCallbacks
    def get_rain_summary(self):
        summary_query = """
        select periods.period,
               periods.start_time,
               periods.end_time,
               sum(sample.rainfall) as rainfall
        from sample
        join (
        select 'today'      as period, date_trunc('day', NOW())                                                     as start_time, NOW() as end_time union all
        select 'yesterday'  as period, date_trunc('day', NOW() - '1 day'::interval)                                 as start_time, date_trunc('day', NOW()) - '1 microsecond'::interval as end_time union all
        select 'this_week'  as period, current_date - ((extract(dow from current_date))::text || ' days')::interval as start_time, NOW() as end_time union all
        select 'this_month' as period, date_trunc('month', NOW())                                                   as start_time, NOW() as end_time union all
        select 'this_year'  as period, date_trunc('year', NOW())                                                    as start_time, NOW() as end_time
            ) as periods on sample.time_stamp between periods.start_time and periods.end_time
        where sample.station_id = $station
        group by periods.period, periods.start_time, periods.end_time
            """
