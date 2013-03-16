# coding=utf-8
from twisted.internet import reactor
from twisted.python import log

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

    def __init__(self, upload_client):
        """
        :param upload_client: The UploadClient instance to use for pushing data
        out to the remote system
        :type upload_client: UploadClient
        :return:
        """
        self._upload_client = upload_client
        self._latest_ts = {}

    def _fetch_generic_live(self, station_code):
        """
        Fetches live data and sends it off to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        if self._upload_client.latestSampleInfo is None:
            # Client is not connected yet.
            return

        # Don't bother sending live data for stations the remote system doesn't
        # know anything about.
        if station_code not in self._upload_client.latestSampleInfo:
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

        def process_result(result, upload_client):
            station_data = result[0]
            upload_client.sendLive(station_data, 'GENERIC')

        self._conn.runQuery(query, (station_code,)).addCallback(
            process_result, self._upload_client)

    def _fetch_davis_live(self, station_code):
        """
        Fetches live data from Davis weather stations and sends it off to the
        UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        if self._upload_client.latestSampleInfo is None:
            # Client is not connected yet.
            return

        # Don't bother sending live data for stations the remote system doesn't
        # know anything about.
        if station_code not in self._upload_client.latestSampleInfo:
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
           dd.forecast_rule_id
    from live_data ld
    inner join station s on s.station_id = ld.station_id
    inner join davis_live_data dd on dd.station_id = ld.station_id
    where s.code = %s
        """

        def process_result(result, upload_client):
            station_data = result[0]
            upload_client.sendLive(station_data, 'DAVIS')

        self._conn.runQuery(query, (station_code,)).addCallback(
            process_result, self._upload_client)

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

    def _get_latest_ts(self, station_code):
        if station_code not in self._latest_ts:
            self._latest_ts[station_code] = self._upload_client.latestSampleInfo[station_code]['timestamp']
        return self._latest_ts[station_code]

    def process_samples_result(self, result, hw_type):
        """
        Sends the results from a samples query off to the upload client.
        :param result: Query result
        :param hw_type: Hardware type for the station.
        """
        for data in result:
            self._upload_client.sendSample(data, hw_type, True)
            self._latest_ts[data['station_code']] = data['time_stamp']

        self._upload_client.flushSamples()

    # TODO: merge the three samples queries.

    def fetch_wh1080_samples(self, station_code, latest_ts):
        query = """
select st.code as station_code,
       coalesce(s.indoor_relative_humidity::varchar, 'None') as indoor_humidity,
       coalesce(s.indoor_temperature::varchar, 'None') as indoor_temperature,
       coalesce(s.temperature::varchar, 'None') as temperature,
       coalesce(s.relative_humidity::varchar, 'None') as humidity,
       coalesce(s.absolute_pressure::varchar, 'None') as pressure,
       coalesce(s.average_wind_speed::varchar, 'None') as average_wind_speed,
       coalesce(s.gust_wind_speed::varchar, 'None') as gust_wind_speed,
       coalesce(s.wind_direction::varchar, 'None') as wind_direction,
       coalesce(s.rainfall::varchar, 'None') as rainfall,
       coalesce(s.download_timestamp::varchar, 'None') as download_timestamp,
       coalesce(s.time_stamp::varchar, 'None') as time_stamp,
       coalesce(wh.sample_interval::varchar, 'None') as sample_interval,
       wh.record_number,
       case when wh.last_in_batch then 'True'
            when wh.last_in_batch is null then 'None'
            else 'False' end as last_in_batch,
       case when wh.invalid_data then 'True'
            else 'False' end as invalid_data,
       coalesce(wh.wind_direction::varchar, 'None') as wh1080_wind_direction,
       coalesce(wh.total_rain::varchar, 'None') as total_rain,
       case when wh.rain_overflow then 'True'
            when wh.rain_overflow is null then 'None'
            else 'False' end as rain_overflow
from sample s
inner join station st on st.station_id = s.station_id
inner join wh1080_sample wh on wh.sample_id = s.sample_id
where st.code = %s
        """
        params = (station_code,)

        # If there is no latest timestamp then the remote database is probably
        # empty so we'll send everything.
        if latest_ts is not None:
            query += "  and s.time_stamp > %s"
            params = (station_code, latest_ts)

        query += """
order by s.time_stamp asc
        """

        self._conn.runQuery(query, params).addCallback(
            self.process_samples_result, 'FOWH1080')

    def fetch_generic_samples(self, station_code, latest_ts):
        query = """
select st.code as station_code,
       coalesce(s.indoor_relative_humidity::varchar, 'None') as indoor_humidity,
       coalesce(s.indoor_temperature::varchar, 'None') as indoor_temperature,
       coalesce(s.temperature::varchar, 'None') as temperature,
       coalesce(s.relative_humidity::varchar, 'None') as humidity,
       coalesce(s.absolute_pressure::varchar, 'None') as pressure,
       coalesce(s.average_wind_speed::varchar, 'None') as average_wind_speed,
       coalesce(s.gust_wind_speed::varchar, 'None') as gust_wind_speed,
       coalesce(s.wind_direction::varchar, 'None') as wind_direction,
       coalesce(s.rainfall::varchar, 'None') as rainfall,
       coalesce(s.download_timestamp::varchar, 'None') as download_timestamp,
       coalesce(s.time_stamp::varchar, 'None') as time_stamp
from sample s
inner join station st on st.station_id = s.station_id
where st.code = %
        """
        params = (station_code,)

        # If there is no latest timestamp then the remote database is probably
        # empty so we'll send everything.
        if latest_ts is not None:
            query += "  and s.time_stamp > %s"
            params = (station_code, latest_ts)

        query += """
order by s.time_stamp asc
        """

        self._conn.runQuery(query, params).addCallback(
            self.process_samples_result, 'GENERIC')

    def _fetch_davis_samples(self, station_code, latest_ts):
        query = """
select st.code as station_code,
       coalesce(s.indoor_relative_humidity::varchar, 'None') as indoor_humidity,
       coalesce(s.indoor_temperature::varchar, 'None') as indoor_temperature,
       coalesce(s.temperature::varchar, 'None') as temperature,
       coalesce(s.relative_humidity::varchar, 'None') as humidity,
       coalesce(s.absolute_pressure::varchar, 'None') as pressure,
       coalesce(s.average_wind_speed::varchar, 'None') as average_wind_speed,
       coalesce(s.gust_wind_speed::varchar, 'None') as gust_wind_speed,
       coalesce(s.wind_direction::varchar, 'None') as wind_direction,
       coalesce(s.rainfall::varchar, 'None') as rainfall,
       coalesce(s.download_timestamp::varchar, 'None') as download_timestamp,
       coalesce(s.time_stamp::varchar, 'None') as time_stamp,

       ds.record_time::varchar as record_time,
       ds.record_date::varchar as record_date,
       coalesce(ds.high_temperature::varchar, 'None') as high_temperature,
       coalesce(ds.low_temperature::varchar, 'None') as low_temperature,
       coalesce(ds.high_rain_rate::varchar, 'None') as high_rain_rate,
       coalesce(ds.solar_radiation::varchar, 'None') as solar_radiation,
       coalesce(ds.wind_sample_count::varchar, 'None') as wind_sample_count,
       coalesce(ds.gust_wind_direction::varchar, 'None') as gust_wind_direction,
       coalesce(ds.average_uv_index::varchar, 'None') as average_uv_index,
       coalesce(ds.evapotranspiration::varchar, 'None') as evapotranspiration,
       coalesce(ds.high_solar_radiation::varchar, 'None') as high_solar_radiation,
       coalesce(ds.high_uv_index::varchar, 'None') as high_uv_index,
       coalesce(ds.forecast_rule_id::varchar, 'None') as forecast_rule_id
from sample s
inner join station st on st.station_id = s.station_id
inner join davis_sample ds on ds.sample_id = s.sample_id
where st.code = %s
        """
        params = (station_code,)

        # If there is no latest timestamp then the remote database is probably
        # empty so we'll send everything.
        if latest_ts is not None:
            query += "  and s.time_stamp > %s"
            params = (station_code, latest_ts)

        query += """
order by s.time_stamp asc
        """

        self._conn.runQuery(query, params).addCallback(
            self.process_samples_result, 'DAVIS')

    def _fetch_samples(self, station_code):

        if self._upload_client.latestSampleInfo is None:
            # Client is not connected yet.
            return

        # Don't bother sending samples for stations the remote system doesn't
        # know anything about.
        if station_code not in self._upload_client.latestSampleInfo:
            return

        latest_ts = self._get_latest_ts(station_code)

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'FOWH1080':
            self.fetch_wh1080_samples(station_code, latest_ts)
        elif hw_type == 'DAVIS':
            self._fetch_davis_samples(station_code, latest_ts)
        else:  # Its GENERIC or something unsupported.
            self.fetch_generic_samples(station_code, latest_ts)


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

    def _reconnect(self):
        log.msg('Connect: {0}'.format(self._connection_string))
        self._conn = DictConnection()
        self._conn_d = self._conn.connect(self._connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self.observer)

        self._conn_d.addCallback(lambda _:self._cache_hardware_types())
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen live_data_updated"))
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen new_sample"))
        self._conn_d.addCallback(
            lambda _: log.msg('Connected to database. Now waiting for data.'))

    def connect(self, connection_string):
        """
        Connects to the database and starts sending live data and new samples to
        the supplied upload client.
        :param connection_string: Database connection string
        :type connection_string: str
        """

        # connect to the database
        self._connection_string = connection_string

        self._reconnect()

        reactor.callLater(self._CONN_CHECK_INTERVAL, self._conn_check)

    def _conn_check(self):
        """
        This is a bit of a nasty way to tell if the DB connection has failed.
        But as txpostgres doesn't report a lost connection I can't think of
        any other way to detect it.
        """

        def _conn_check_failed(failure):
            failure.trap(psycopg2.OperationalError)

            log.msg('DB connection presumed dead. Actual error was: {0}'.format(
                failure.getErrorMessage()))
            log.msg('Attempting reconnect')
            self._reconnect()


        self._conn.runQuery("select 42").addErrback(_conn_check_failed)

        reactor.callLater(self._CONN_CHECK_INTERVAL, self._conn_check)