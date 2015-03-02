# coding=utf-8
from twisted.enterprise import adbapi
from twisted.internet import reactor, defer
from twisted.python import log
from ..common.util import Event

__author__ = 'david'
import psycopg2
from psycopg2.extras import DictConnection as Psycopg2DictConn
from txpostgres import txpostgres



def dict_connect( *args, **kwargs):
    kwargs['connection_factory'] = Psycopg2DictConn
    return psycopg2.connect(*args, **kwargs)

class DictConnection(txpostgres.Connection):
    connectionFactory = staticmethod(dict_connect)

class WeatherDatabase(object):

    _CONN_CHECK_INTERVAL = 60

    def __init__(self, hostname, dsn):
        """
        """
        self.NewSample = Event()
        self.EndOfSamples = Event()
        self.LiveUpdate = Event()

        self._hostname = hostname
        self._site_id = None
        self._transmitter_ready = False
        self._connection_string = dsn

        self._remote_stations = None

        self._confirmations = []

        self._database_pool = None

        self._processing_confirmations = False
        self._peak_confirmation_queue_length = 0

    def transmitter_ready(self, remote_stations):
        self._remote_stations = remote_stations
        self._transmitter_ready = True
        self._connect()

    def confirm_receipt(self, station_code, time_stamp):
        """
        Confirms that the remote system has received a sample for the specified
        station with the given time stamp.

        :param station_code: Station the sample was for
        :param time_stamp: Timestamp of the sample
        :return:
        """

        self._confirmations.append((station_code, time_stamp))

        if not self._processing_confirmations:
            self._process_confirmations()

    @staticmethod
    def _wh1080_sample_query(ascending):
        query = """
        select s.sample_id as sample_id,
       st.code as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp as download_timestamp,
       s.time_stamp as time_stamp,
       wh.sample_interval as sample_interval,
       wh.record_number,
       wh.last_in_batch,
       wh.invalid_data,
       wh.wind_direction as wh1080_wind_direction,
       wh.total_rain as total_rain,
       wh.rain_overflow
from sample s
inner join station st on st.station_id = s.station_id
inner join wh1080_sample wh on wh.sample_id = s.sample_id
inner join replication_status rs on rs.sample_id = s.sample_id
where st.code = %(station_code)s
  and rs.site_id = %(site_id)s

  -- Grab everything that is pending
  and ((%(pending)s and (rs.status = 'pending' or (
          -- And everything that has been waiting for receipt confirmation for
          -- more than 5 minutes
          rs.status = 'awaiting_confirmation'
          and rs.status_time < NOW() - '10 minutes'::interval)))
        or (not %(pending)s and (rs.status='done'))
order by s.time_stamp {0}
limit %(limit)s
"""
        if ascending:
            return query.format('asc',)
        else:
            return query.format('desc',)

    @staticmethod
    def _davis_sample_query(ascending):
        query = """
select s.sample_id as sample_id,
       st.code as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp as download_timestamp,
       s.time_stamp as time_stamp,

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
       ds.forecast_rule_id as forecast_rule_id
from sample s
inner join station st on st.station_id = s.station_id
inner join davis_sample ds on ds.sample_id = s.sample_id
inner join replication_status rs on rs.sample_id = s.sample_id
where st.code = %(station_code)s
  and rs.site_id = %(site_id)s

  -- Grab everything that is pending
  and ((%(pending)s and (rs.status = 'pending' or (
          -- And everything that has been waiting for receipt confirmation for
          -- more than 5 minutes
          rs.status = 'awaiting_confirmation'
          and rs.status_time < NOW() - '10 minutes'::interval)))
        or (not %(pending)s and (rs.status='done'))
order by s.time_stamp {0}
limit %(limit)s
        """

        if ascending:
            return query.format('asc',)
        else:
            return query.format('desc',)

    @staticmethod
    def _generic_sample_query(ascending):
        query = """
select s.sample_id as sample_id,
       st.code as station_code,
       s.indoor_relative_humidity as indoor_humidity,
       s.indoor_temperature as indoor_temperature,
       s.temperature as temperature,
       s.relative_humidity as humidity,
       s.absolute_pressure as pressure,
       s.average_wind_speed as average_wind_speed,
       s.gust_wind_speed as gust_wind_speed,
       s.wind_direction as wind_direction,
       s.rainfall as rainfall,
       s.download_timestamp as download_timestamp,
       s.time_stamp as time_stamp
from sample s
inner join station st on st.station_id = s.station_id
inner join replication_status rs on rs.sample_id = s.sample_id
where st.code = %(station_code)s
  and rs.site_id = %(site_id)s

  -- Grab everything that is pending
  and ((%(pending)s and (rs.status = 'pending' or (
          -- And everything that has been waiting for receipt confirmation for
          -- more than 5 minutes
          rs.status = 'awaiting_confirmation'
          and rs.status_time < NOW() - '10 minutes'::interval)))
        or (not %(pending)s and (rs.status='done'))
order by s.time_stamp {0}
limit %(limit)s
        """

        if ascending:
            return query.format('asc',)
        else:
            return query.format('desc',)

    @defer.inlineCallbacks
    def get_last_confirmed_sample(self, station_code):
        """
        Returns the most recent sample for the specified station that the
        server is known to have received, decoded and stored successfully.

        :param station_code: Station code to get confirmed sample for
        :type station_code: str
        :return: Sample data
        :rtype: dict
        """

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'FOWH1080':
            query = self._wh1080_sample_query(False)
        elif hw_type == 'DAVIS':
            query = self._davis_sample_query(False)
        else:  # Its GENERIC or something unsupported.
            query = self._generic_sample_query(False)

        parameters = {
            'station_code': station_code,
            'site_id': self._site_id,
            'pending': False,
            'limit': 1
        }

        result = yield self._conn.runQuery(query, parameters)

        defer.returnValue(result)

    @defer.inlineCallbacks
    def _process_confirmations(self):
        self._processing_confirmations = True

        log_interval = 100

        while len(self._confirmations) > 0:

            self._peak_confirmation_queue_length = max(
                len(self._confirmations), self._peak_confirmation_queue_length)

            log_interval -= 1
            if log_interval == 0:
                log.msg("Receipt confirmation queue length: {0} (peak {1})".format(
                    len(self._confirmations),
                    self._peak_confirmation_queue_length))
                log_interval = 100

            confirmation = self._confirmations.pop(0)
            station_code = confirmation[0]
            time_stamp = confirmation[1]

            query = """
            update replication_status ars
            set status = 'done',
            status_time = NOW()
            from replication_status rs
            inner join sample s on s.sample_id = rs.sample_id
            inner join station st on st.station_id = s.station_id
            where s.time_stamp = %s
              and st.code = %s
              and rs.site_id = %s
              and ars.sample_id = rs.sample_id
              and ars.site_id=rs.site_id
            """

            yield self._database_pool.runOperation(query, (time_stamp,
                                                           station_code,
                                                           self._site_id))
        self._processing_confirmations = False

    def _fetch_generic_live(self, station_code):
        """
        Fetches live data and sends it off to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        if not self._transmitter_ready:
            # Client is not connected yet.
            return

        # Don't bother sending live data for stations the remote system doesn't
        # know anything about.
        if station_code not in self._remote_stations:
            return

        query = """
        select s.code as station_code,
           ld.download_timestamp,
           ld.indoor_relative_humidity as indoor_humidity,
           ld.indoor_temperature,
           ld.temperature,
           ld.relative_humidity as humidity,
           ld.absolute_pressure as pressure,
           ld.average_wind_speed,
           ld.gust_wind_speed,
           ld.wind_direction
    from live_data ld
    inner join station s on s.station_id = ld.station_id
    where s.code = %s
        """

        def process_result(result):
            station_data = result[0]
            self.LiveUpdate.fire(station_data, 'GENERIC')

        self._conn.runQuery(query, (station_code,)).addCallback(
            process_result)

    def _fetch_davis_live(self, station_code):
        """
        Fetches live data from Davis weather stations and sends it off to the
        UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        if not self._transmitter_ready:
            # Client is not connected yet.
            return

        # Don't bother sending live data for stations the remote system doesn't
        # know anything about.
        if station_code not in self._remote_stations:
            return

        query = """
        select s.code as station_code,
           ld.download_timestamp,
           ld.indoor_relative_humidity as indoor_humidity,
           ld.indoor_temperature,
           ld.temperature,
           ld.relative_humidity as humidity,
           ld.absolute_pressure as pressure,
           ld.average_wind_speed,
           ld.gust_wind_speed,
           ld.wind_direction,
           dd.bar_trend,
           dd.rain_rate,
           dd.storm_rain,
           dd.current_storm_start_date,
           dd.transmitter_battery,
           dd.console_battery_voltage,
           dd.forecast_icon,
           dd.forecast_rule_id,
           dd.uv_index,
           dd.solar_radiation
    from live_data ld
    inner join station s on s.station_id = ld.station_id
    inner join davis_live_data dd on dd.station_id = ld.station_id
    where s.code = %s
        """

        def process_result(result):
            station_data = result[0]
            self.LiveUpdate.fire(station_data, 'DAVIS')

        self._conn.runQuery(query, (station_code,)).addCallback(
            process_result)

    def _fetch_live(self, station_code):
        """
        Fetches live data for the specified weather station and sends it off
        to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'DAVIS':  # Davis hardware has extra data to send.
            self._fetch_davis_live(station_code)
        else:  # FineOffset and Generic hardware don't send any extra data
            self._fetch_generic_live(station_code)

    def process_samples_result(self, result, hw_type):
        """
        Sends the results from a samples query off to the upload client.
        :param result: Query result
        :param hw_type: Hardware type for the station.
        """

        for data in result:
            self._update_replication_status(data["sample_id"])
            self.NewSample.fire(data, hw_type, True)

        self.EndOfSamples.fire()

    def _update_replication_status(self, sample_id):
        query = """
        update replication_status
        set status = 'awaiting_confirmation',
        status_time = NOW(),
        retries = case when status = 'pending' then 0 else retries + 1 end
        where sample_id = %s and site_id = %s
        """

        self._conn.runOperation(query, (sample_id, self._site_id))

    @defer.inlineCallbacks
    def _fetch_samples(self, station_code):
        log.msg("Fetch samples for {0}".format(station_code))
        if not self._transmitter_ready:
            # Client is not connected yet.
            return

        # Don't bother sending samples for stations the remote system doesn't
        # know anything about.
        if station_code not in self._remote_stations:
            return

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'FOWH1080':
            query = self._wh1080_sample_query(True)
        elif hw_type == 'DAVIS':
            query = self._davis_sample_query(True)
        else:  # Its GENERIC or something unsupported.
            query = self._generic_sample_query(True)

        parameters = {
            'station_code': station_code,
            'site_id': self._site_id,
            'pending': True,
            'limit': 10000
        }

        result = yield self._conn.runQuery(query, parameters)\

        self.process_samples_result(result, hw_type)

    def observer(self, notify):
        """
        Called when ever notifications are received.
        :param notify: The notification.
        """

        if notify.channel == "live_data_updated":
            self._fetch_live(notify.payload)
        elif notify.channel == "new_sample":
            self._fetch_samples(notify.payload)

    def _store_hardware_types(self, result):
        self.station_code_hardware_type = {}

        for row in result:
            self.station_code_hardware_type[row[0]] = row[1]

    def _cache_hardware_types(self):
        query = "select s.code, st.code as hw_code from station s inner join "\
                "station_type st on st.station_type_id = s.station_type_id "
        self._conn.runQuery(query).addCallback(self._store_hardware_types)

    @defer.inlineCallbacks
    def _prepare_sample_queue(self):
        """
        Make sure we the replication_status table is appropriately populated
        for the remote site.
        :return:
        """
        query = "select site_id from remote_site where hostname = %s"

        result = yield self._conn.runQuery(query, (self._hostname,))

        if len(result) == 0:
            log.msg("Remote site is not known. Adding site definition...")
            query = "insert into remote_site(hostname) values(%s) " \
                    "returning site_id"

            result = yield self._conn.runQuery(query, (self._hostname,))

            self._site_id = result[0][0]
            log.msg("Rebuilding replication status for site {0}. This may take "
                    "some time...".format(self._site_id))

            query = """
            insert into replication_status(site_id, sample_id)
            select rs.site_id, sample.sample_id
            from sample, remote_site rs
            where rs.site_id not in (
            select rs.site_id
            from sample as sample_inr
            inner join remote_site rs on rs.site_id = %s
            inner join replication_status as repl on repl.site_id = rs.site_id and repl.sample_id = sample_inr.sample_id
            where sample_inr.sample_id = sample.sample_id
            )
            and rs.site_id = %s
            """

            yield self._conn.runOperation(query, (self._site_id, self._site_id,))
            log.msg("Rebuild complete.")
        else:
            self._site_id = result[0][0]

    def _connect(self):
        """
        Connects to the database and starts sending live data and new samples to
        the supplied upload client.
        :param connection_string: Database connection string
        :type connection_string: str
        """

        log.msg('Connect: {0}'.format(self._connection_string))
        self._conn = DictConnection()
        self._conn_d = self._conn.connect(self._connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self.observer)

        self._conn_d.addCallback(lambda _: self._cache_hardware_types())
        self._conn_d.addCallback(lambda _: self._prepare_sample_queue())
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen live_data_updated"))
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen new_sample"))
        self._conn_d.addCallback(
            lambda _: log.msg('Connected to database. Now waiting for data.'))

        self._database_pool = adbapi.ConnectionPool("psycopg2",
                                                    self._connection_string)

