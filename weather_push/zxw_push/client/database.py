# coding=utf-8
"""
Client-side database functionality
"""
import copy

from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.defer import returnValue
from twisted.python import log
from ..common.util import Event
from zxw_push.common.database import wh1080_sample_query, davis_sample_query, \
    generic_sample_query
import psycopg2
from psycopg2.extras import DictConnection as Psycopg2DictConn
from txpostgres import txpostgres

__author__ = 'david'

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


# This just defines the public API the client weather database exports.
# Here for clarity only really.
class BaseClientDatabase(object):
    def transmitter_ready(self, remote_stations):
        raise NotImplementedError

    def confirm_receipt(self, station_code, time_stamp):
        raise NotImplementedError

    @defer.inlineCallbacks
    def confirm_image_receipt(self, image_source_code, image_type_code,
                              time_stamp, resized):
        raise NotImplementedError

    @defer.inlineCallbacks
    def get_last_confirmed_sample(self, station_code):
        raise NotImplementedError

    def process_samples_result(self, result, hw_type):
        raise NotImplementedError


class WeatherDatabase(BaseClientDatabase):
    """
    Client-side functionality relating to the weather database.
    """

    _CONN_CHECK_INTERVAL = 60

    def __init__(self, hostname, dsn):
        """
        """
        self.NewSample = Event()
        self.NewImage = Event()
        self.EndOfSamples = Event()
        self.LiveUpdate = Event()

        self._hostname = hostname
        self._site_id = None
        self._transmitter_ready = False
        self._connection_string = dsn

        self._remote_stations = None
        self._local_stations = []

        self._confirmations = []

        self._database_pool = None

        self._processing_confirmations = False
        self._peak_confirmation_queue_length = 0

        self._database_ready = False

    @staticmethod
    def _to_real_dict(value):
        result = {}

        for key in value.keys():
            result[key] = value[key]

        return result

    @staticmethod
    def _move_davis_extras_to_subfield(row):
        """
        Moves the davis extra fields into their subfield
        :param row: Row to move subfields in
        :return: Modified version of row with subfields moved.
        """

        if row is None:
            return row

        new_row = WeatherDatabase._to_real_dict(row)

        new_row.pop("leaf_wetness_1", None)
        new_row.pop("leaf_wetness_2", None)
        new_row.pop("leaf_temperature_1", None)
        new_row.pop("leaf_temperature_2", None)
        new_row.pop("soil_moisture_1", None)
        new_row.pop("soil_moisture_2", None)
        new_row.pop("soil_moisture_3", None)
        new_row.pop("soil_moisture_4", None)
        new_row.pop("soil_temperature_1", None)
        new_row.pop("soil_temperature_2", None)
        new_row.pop("soil_temperature_3", None)
        new_row.pop("soil_temperature_4", None)
        new_row.pop("extra_temperature_1", None)
        new_row.pop("extra_temperature_2", None)
        new_row.pop("extra_temperature_3", None)
        new_row.pop("extra_humidity_1", None)
        new_row.pop("extra_humidity_2", None)

        new_row["extra_fields"] = {
            "leaf_wetness_1": row["leaf_wetness_1"],
            "leaf_wetness_2": row["leaf_wetness_2"],
            "leaf_temperature_1": row["leaf_temperature_1"],
            "leaf_temperature_2": row["leaf_temperature_2"],
            "soil_moisture_1": row["soil_moisture_1"],
            "soil_moisture_2": row["soil_moisture_2"],
            "soil_moisture_3": row["soil_moisture_3"],
            "soil_moisture_4": row["soil_moisture_4"],
            "soil_temperature_1": row["soil_temperature_1"],
            "soil_temperature_2": row["soil_temperature_2"],
            "soil_temperature_3": row["soil_temperature_3"],
            "soil_temperature_4": row["soil_temperature_4"],
            "extra_temperature_1": row["extra_temperature_1"],
            "extra_temperature_2": row["extra_temperature_2"],
            "extra_temperature_3": row["extra_temperature_3"],
            "extra_humidity_1": row["extra_humidity_1"],
            "extra_humidity_2": row["extra_humidity_2"],
        }

        return new_row

    def transmitter_ready(self, remote_stations):
        """
        Called when the client is ready to transmit data.
        :param remote_stations: List of stations it can transmit data for
        :return:
        """

        log.msg("Transmitter ready for stations: {0}".format(
            ", ".join(remote_stations)))

        self._remote_stations = [rs.upper() for rs in remote_stations]
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

        if self._database_ready and not self._processing_confirmations:
            self._process_confirmations()

    @defer.inlineCallbacks
    def confirm_image_receipt(self, image_source_code, image_type_code,
                              time_stamp, resized):
        """
        Confirms that the remote system has received an image from the specified
        image source with the specified timestamp.

        Assuming there are never more than 256 image sources and 256 image types
        identify images by their source, type and timestamp combined takes up
        6 bytes and already has to be present in the original transmission of
        the Image:
          - Cost to send: 0
          - Cost to receive: 6
        Identifying them by the image ID would require the image ID to be
        included in the original transmission where where it previously wasnt
        required. Assuming there are never more than ~4 billion images:
          - Cost to send: 4
          - Cost to receive: 4
        So identifying by source, type and timestamp is two bytes cheaper.

        :param image_source_code: Source the image came from
        :param image_type_code: The type of the image
        :param time_stamp: Timestamp of the image
        :param resized: If the image was originally transmitted resized instead
                        of verbatim.
        """
        query = """
            update image_replication_status iars
            set status = %(status)s,
            status_time = NOW()
            from image_replication_status irs
            inner join image i on i.image_id = irs.image_id
            inner join image_source isrc on isrc.image_source_id = i.image_source_id
            inner join image_type it on it.image_type_id = i.image_type_id
            where date_trunc('second', i.time_stamp) = %(time_stamp)s::timestamp at time zone 'GMT'
              and upper(isrc.code) = upper(%(source_code)s)
              and upper(it.code) = upper(%(type_code)s)
              and irs.site_id = %(site_id)s
              and iars.image_id = irs.image_id
              and iars.site_id=irs.site_id
            """

        status = 'done'
        if resized:
            status = 'done_resize'

        parameters = {
            'time_stamp': time_stamp,
            'source_code': image_source_code,
            'type_code': image_type_code,
            'site_id': self._site_id,
            'status': status
        }

        yield self._database_pool.runOperation(query, parameters)

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

        if not self._database_ready:
            defer.returnValue(None)

        hw_type = self.station_code_hardware_type[station_code]

        # Default clause for all queries is pending
        if hw_type == 'FOWH1080':
            query = wh1080_sample_query(False)
        elif hw_type == 'DAVIS':
            query = davis_sample_query(False)
        else:  # Its GENERIC or something unsupported.
            query = generic_sample_query(False)

        parameters = {
            'station_code': station_code,
            'site_id': self._site_id,
            'pending': False,
            'pending_b': False,
            'limit': 1
        }

        results = yield self._conn.runQuery(query, parameters)

        result = None

        if len(results) > 0:
            result = results[0]

        if hw_type == 'DAVIS':
            result = self._move_davis_extras_to_subfield(result)

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
                log.msg("Receipt confirmation queue length: {0} (peak {1})"
                        .format(len(self._confirmations),
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
            where s.time_stamp = (%(time_stamp)s::timestamp at time zone 'GMT')
              and upper(st.code) = upper(%(code)s)
              and rs.site_id = %(site_id)s
              and ars.sample_id = rs.sample_id
              and ars.site_id=rs.site_id
            """

            parameters = {
                'time_stamp': time_stamp,
                'code': station_code,
                'site_id': self._site_id
            }

            yield self._database_pool.runOperation(query, parameters)

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
        select upper(s.code) as station_code,
           ld.download_timestamp at time zone 'GMT',
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
    where upper(s.code) = upper(%s)
        """

        def _process_result(result):
            station_data = result[0]

            # While the live record may just be generic, the station hardware
            # type may not be and as such it could have extra data for samples
            # which will be encoded based on whatever hardware type we report
            # here.
            self.LiveUpdate.fire(
                station_data,
                self.station_code_hardware_type[station_data["station_code"]])

        self._conn.runQuery(query, (station_code,)).addCallback(
            _process_result)

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
        select upper(s.code) as station_code,
           ld.download_timestamp at time zone 'GMT',
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
           dd.solar_radiation,
           dd.leaf_wetness_1,
           dd.leaf_wetness_2,
           dd.leaf_temperature_1,
           dd.leaf_temperature_2,
           dd.soil_moisture_1,
           dd.soil_moisture_2,
           dd.soil_moisture_3,
           dd.soil_moisture_4,
           dd.soil_temperature_1,
           dd.soil_temperature_2,
           dd.soil_temperature_3,
           dd.soil_temperature_4,
           dd.extra_temperature_1,
           dd.extra_temperature_2,
           dd.extra_temperature_3,
           dd.extra_humidity_1,
           dd.extra_humidity_2
    from live_data ld
    inner join station s on s.station_id = ld.station_id
    inner join davis_live_data dd on dd.station_id = ld.station_id
    where upper(s.code) = upper(%s)
        """

        def _process_result(result):
            station_data = result[0]

            result = self._move_davis_extras_to_subfield(station_data)

            self.LiveUpdate.fire(result, 'DAVIS')

        self._conn.runQuery(query, (station_code,)).addCallback(
            _process_result)

    @defer.inlineCallbacks
    def _check_station(self, station_code):
        """
        Checks if a station is not archived before processing data for it.
        :param station_code: Station code to check
        :return: True if the station is known and not archived, False if there is a problem and data should not be transmitted.
        """
        if station_code.upper() not in self._local_stations:
            result = yield self._conn.runQuery("select archived from station "
                                               "where upper(code) = upper(%s)", (station_code,))

            if len(result) == 0:
                log.msg("Warning: Received data for unknown station {0} but "
                        "station could not be found in the database. Ignoring.".format(station_code))
                defer.returnValue(False)
            else:
                is_archived = result[0][0]
                if is_archived:
                    log.msg("Warning: received data for archived station {0}. "
                            "Archived stations can not transmit new data. Ignoring.".format(station_code))
                    defer.returnValue(False)
                else:
                    log.msg("New station detected - {0}!".format(station_code))
                    self._local_stations.append(station_code.upper())
        defer.returnValue(True)

    @defer.inlineCallbacks
    def _fetch_live(self, station_code):
        """
        Fetches live data for the specified weather station and sends it off
        to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        station_ok = yield self._check_station(station_code)
        if not station_ok:
            return

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
            if hw_type == 'DAVIS':
                data = self._move_davis_extras_to_subfield(data)
            self._update_replication_status(data["sample_id"])
            self.NewSample.fire(data, hw_type, True)

        self.EndOfSamples.fire()

    def _update_replication_status(self, sample_id):
        query = """
        update replication_status
        set status = 'awaiting_confirmation',
        status_time = NOW(),
        retries = case when status = 'pending' then 0 else retries + 1 end
        where sample_id = %s and site_id = %s and status <> 'done'
        """

        self._conn.runOperation(query, (sample_id, self._site_id))

    def _update_image_replication_status(self, image_id):
        query = """
        update image_replication_status
        set status = 'awaiting_confirmation',
        status_time = NOW(),
        retries = case when status = 'pending' then 0 else retries + 1 end
        where image_id = %s and site_id = %s
        """

        self._conn.runOperation(query, (image_id, self._site_id))

    @defer.inlineCallbacks
    def _fetch_samples(self, station_code):
        station_code = station_code.upper()

        if not self._transmitter_ready:
            # Client is not connected yet.
            log.msg("Transmitter not yet ready - skip sample fetch for station {0}",format(station_code))
            return

        station_ok = yield self._check_station(station_code)
        if not station_ok:
            return

        # Don't bother sending samples for stations the remote system doesn't
        # know anything about.
        if station_code not in self._remote_stations:
            log.msg("Station {0} not known to remote system - ignoring new samples.".format(station_code))
            return

        log.msg("Fetch samples for {0}".format(station_code))

        hw_type = self.station_code_hardware_type[station_code]

        # default where clause for all queries is "pending"
        if hw_type == 'FOWH1080':
            query = wh1080_sample_query(True)
        elif hw_type == 'DAVIS':
            query = davis_sample_query(True)
        else:  # Its GENERIC or something unsupported.
            query = generic_sample_query(True)

        parameters = {
            'station_code': station_code,
            'site_id': self._site_id,
            'pending': True,
            'pending_b': True,
            'limit': 1500  # This is about the max records we could send in 5 min
        }

        result = yield self._conn.runQuery(query, parameters)

        self.process_samples_result(result, hw_type)

    @defer.inlineCallbacks
    def _fetch_station_images(self, station_code):
        log.msg("Fetching images for {0}...".format(station_code))

        query = """
        select i.image_id,
               upper(it.code) as image_type_code,
               upper(isrc.code) as image_source_code,
               i.time_stamp at time zone 'GMT' as time_stamp,
               i.title,
               i.description,
               i.mime_type,
               i.metadata,
               i.image_data
        from image i
        inner join image_type it on it.image_type_id = i.image_type_id
        inner join image_source isrc on isrc.image_source_id = i.image_source_id
        inner join image_replication_status irs on irs.image_id = i.image_id
        inner join station s on s.station_id = isrc.station_id
        where upper(s.code) = upper(%(station_code)s)
          and irs.site_id = %(site_id)s
          -- Grab everything that is pending
          and (irs.status = 'pending' or (
                  -- And everything that has been waiting for receipt confirmation for
                  -- more than 5 minutes
                  irs.status = 'awaiting_confirmation'
                  and irs.retries < 5
                  and irs.status_time < NOW() - '10 minutes'::interval))
        limit 5 -- images could take up a lot of ram.
        """

        parameters = {
            'station_code': station_code,
            'site_id': self._site_id,
        }

        result = yield self._conn.runQuery(query, parameters)

        for data in result:
            self._update_image_replication_status(data["image_id"])
            self.NewImage.fire(data)

    @defer.inlineCallbacks
    def _fetch_images(self):
        if not self._transmitter_ready:
            # Client is not connected yet.
            return

        for station in self._remote_stations:
            yield self._fetch_station_images(station)

    def _observer(self, notify):
        """
        Called when ever notifications are received.
        :param notify: The notification.
        """

        if notify.channel == "live_data_updated":
            self._fetch_live(notify.payload.upper())
        elif notify.channel == "new_sample":
            self._fetch_samples(notify.payload.upper())
        elif notify.channel == "new_image":
            self._fetch_images()  # The payload is the ID of the new image

    def _store_hardware_types(self, result):
        self.station_code_hardware_type = {}

        for row in result:
            self.station_code_hardware_type[row[0]] = row[1]

    def _cache_hardware_types(self):
        query = "select upper(s.code) as code, upper(st.code) as hw_code " \
                "from station s inner join "\
                "station_type st on st.station_type_id = s.station_type_id "
        self._conn.runQuery(query).addCallback(self._store_hardware_types)

    @defer.inlineCallbacks
    def _prepare_sample_queue(self):
        """
        Make sure we the replication status tables is appropriately populated
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
            log.msg("Rebuilding sample replication status for site {0}. This "
                    "may take some time...".format(self._site_id))

            query = """
            insert into replication_status(site_id, sample_id)
            select rs.site_id, sample.sample_id
            from sample, remote_site rs
            where rs.site_id not in (
            select rs.site_id
            from sample as sample_inr
            inner join remote_site rs on rs.site_id = %s
            inner join replication_status as repl on repl.site_id = rs.site_id
                   and repl.sample_id = sample_inr.sample_id
            where sample_inr.sample_id = sample.sample_id
            )
            and rs.site_id = %s
            """

            yield self._conn.runOperation(query, (self._site_id,
                                                  self._site_id,))

            log.msg("Rebuilding image replication status for site {0}. This "
                    "may take some time...".format(self._site_id))

            query = """
            insert into image_replication_status(site_id, image_id)
            select rs.site_id, image.image_id
            from image, remote_site rs
            where rs.site_id not in (
            select rs.site_id
            from image as image_inr
            inner join remote_site rs on rs.site_id = %s
            inner join image_replication_status as repl on repl.site_id = rs.site_id
                   and repl.image_id = image_inr.image_id
            where image_inr.image_id = image.image_id
            )
            and rs.site_id = %s
            """

            yield self._conn.runOperation(query, (self._site_id,
                                                  self._site_id,))

            log.msg("Rebuild complete.")
        else:
            self._site_id = result[0][0]

    @defer.inlineCallbacks
    def _get_schema_version(self):
        result = yield self._conn.runQuery(
            "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")

        if len(result) == 0:
            returnValue(1)

        result = yield self._conn.runQuery(
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
            log.msg("*** ERROR: WeatherPush requires at least database version "
                    "3 (zxweather 1.0.0). Please upgrade your database.")
            reactor.stop()

        result = yield self._conn.runQuery(
            "select version_check('WPUSHC',1,0,0)")
        if not result[0][0]:
            result = yield self._conn.runQuery(
                "select minimum_version_string('WPUSHC')")

            log.msg("*** ERROR: This version of WeatherPush is incompatible"
                    " with the configured database. The minimum WeatherPush "
                    "(or zxweather) version supported by this database is: "
                    "{0}.".format(
                result[0][0]))
            reactor.stop()

        result = yield self._conn.runQuery("select upper(code) from station where not archived")
        for row in result:
            self._local_stations.append(row[0])
        log.msg("Found stations: {0}".format(self._local_stations))

        # Database checks ok.

        self._cache_hardware_types()
        self._prepare_sample_queue()
        yield self._conn.runOperation("listen live_data_updated")
        yield self._conn.runOperation("listen new_sample")
        yield self._conn.runOperation("listen new_image")
        self._database_ready = True
        log.msg('Connected to database. Now waiting for data.')

    def _connect(self):
        """
        Connects to the database and starts sending live data and new samples to
        the supplied upload client.
        """

        if self._database_ready:
            # We're already connected. Nothing to do.
            return

        log.msg('Connect: {0}'.format(self._connection_string))
        self._conn = DictConnection()
        self._conn_d = self._conn.connect(self._connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self._observer)

        self._conn_d.addCallback(lambda _: self._check_db())

        self._database_pool = adbapi.ConnectionPool("psycopg2",
                                                    self._connection_string)

