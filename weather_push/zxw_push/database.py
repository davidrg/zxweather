# coding=utf-8
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

    def __init__(self, upload_client):
        """
        :param upload_client: The UploadClient instance to use for pushing data
        out to the remote system
        :type upload_client: UploadClient
        :return:
        """
        self._upload_client = upload_client
        self._latest_ts = {}

    def fetch_live(self, station_code):
        """
        Fetches live data and sends it off to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

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
            upload_client.sendLive(station_data)

        self._conn.runQuery(query, (station_code,)).addCallback(
            process_result, self._upload_client)

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
            self._upload_client.sendSample(data, hw_type)
            self._latest_ts[data['station_code']] = data['time_stamp']

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
  and s.time_stamp > %s
        """

        self._conn.runQuery(query, (station_code, latest_ts,)).addCallback(
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
where st.code = %s
  and s.time_stamp > %s
        """

        self._conn.runQuery(query, (station_code, latest_ts,)).addCallback(
            self.process_samples_result, 'GENERIC')

    def _fetch_samples(self, station_code):
        # Don't bother sending samples for stations the remote system doesn't
        # know anything about.
        if station_code not in self._upload_client.latestSampleInfo:
            return

        latest_ts = self._get_latest_ts(station_code)

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'FOWH1080':
            self.fetch_wh1080_samples(station_code, latest_ts)
        elif hw_type == 'GENERIC':
            self.fetch_generic_samples(station_code, latest_ts)

    def observer(self, notify):
        """
        Called when ever notifications are received.
        :param notify: The notification.
        """
        log.msg('Notification on channel {0}: {1}'.format(
            notify.channel, notify.payload))

        print 'Notification on channel {0}: {1}'.format(
            notify.channel, notify.payload)

        if notify.channel == "live_data_updated":
            self.fetch_live(notify.payload)
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

    def connect(self, connection_string):
        """
        Connects to the database and starts sending live data and new samples to
        the supplied upload client.
        :param connection_string: Database connection string
        :type connection_string: str
        """
        log.msg('Connect: {0}'.format(connection_string))
        # connect to the database
        self._conn = DictConnection()
        self._conn_d = self._conn.connect(connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self.observer)

        self._conn_d.addCallback(lambda _:self._cache_hardware_types())
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen live_data_updated"))
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen new_sample"))
        log.msg('Connected to database. Now waiting for data.')