# coding=utf-8
"""
Database listener for live data
"""
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
from tempfile import TemporaryFile
import imghdr
import mimetypes

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


class DatabaseReceiver(object):
    """
    Provides access to live data in a weather database.
    """

    def __init__(self, dsn, station_code):
        """
        """
        self.LiveUpdate = Event()

        self._site_id = None
        self._connection_string = dsn
        self._station_code = station_code
        self._database_pool = None
        self._conn = None
        self._conn_d = None

    def _fetch_davis_live(self, station_code):
        """
        Fetches live data from Davis weather stations and sends it off to the
        UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        # Don't bother sending live data for stations the remote system doesn't
        # know anything about.
        if station_code != self._station_code:
            return

        query = """
        select dd.uv_index,
               dd.solar_radiation
    from live_data ld
    inner join station s on s.station_id = ld.station_id
    inner join davis_live_data dd on dd.station_id = ld.station_id
    where s.code = %s
        """

        def _process_result(result):
            station_data = result[0]
            self.LiveUpdate.fire(station_data["uv_index"],
                                 station_data["solar_radiation"])

        self._conn.runQuery(query, (station_code,)).addCallback(
            _process_result)

    def _fetch_live(self, station_code):
        """
        Fetches live data for the specified weather station and sends it off
        to the UploadClient for uploading.
        :param station_code: Station to get live data for.
        """

        if station_code != self._station_code:
            return  # Not the station we're looking for.

        hw_type = self.station_code_hardware_type[station_code]

        if hw_type == 'DAVIS':  # Davis hardware has extra data to send.
            self._fetch_davis_live(station_code)
        # else its something that doesn't support UV/Solar. And thats the only
        # data we're interested in.

    def observer(self, notify):
        """
        Called when ever notifications are received.
        :param notify: The notification.
        """

        if notify.channel == "live_data_updated":
            self._fetch_live(notify.payload)

    def _store_hardware_types(self, result):
        self.station_code_hardware_type = {}

        for row in result:
            self.station_code_hardware_type[row[0]] = row[1]

    def _cache_hardware_types(self):
        query = "select s.code, st.code as hw_code from station s inner join "\
                "station_type st on st.station_type_id = s.station_type_id "
        self._conn.runQuery(query).addCallback(self._store_hardware_types)

    def connect(self):
        """
        Connects to the database and listening for live data.
        """

        log.msg('Connect: {0}'.format(self._connection_string))
        self._conn = DictConnection()
        self._conn_d = self._conn.connect(self._connection_string)

        # add a NOTIFY observer
        self._conn.addNotifyObserver(self.observer)

        self._conn_d.addCallback(lambda _: self._cache_hardware_types())
        self._conn_d.addCallback(
            lambda _: self._conn.runOperation("listen live_data_updated"))
        self._conn_d.addCallback(
            lambda _: log.msg('Connected to database. Now waiting for data.'))

        self._database_pool = adbapi.ConnectionPool("psycopg2",
                                                    self._connection_string)


class Database(object):
    def __init__(self, dsn, image_source_code):
        self._dsn = dsn
        self._database_pool = None
        self._image_source_code = image_source_code
        self._camera_image_type_id = None
        self._image_source_id = None

    def connect(self):
        self._database_pool = adbapi.ConnectionPool("psycopg2",
                                                    self._dsn)
        self._check_db()

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
            log.msg("*** ERROR: image-logger requires at least database "
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
                "select version_check('IMGLOG',1,0,0)")
            if not result[0][0]:
                result = yield self._database_pool.runQuery(
                    "select minimum_version_string('IMGLOG')")

                log.msg("*** ERROR: This version of image-logger is "
                        "incompatible with the configured database. The "
                        "minimum image-logger (or zxweather) version supported "
                        "by this database is: {0}.".format(result[0][0]))
                try:
                    reactor.stop()
                except ReactorNotRunning:
                    # Don't care. We wanted it stopped, turns out it already is
                    # that or its not yet started in which case there isn't
                    # anything much we can do to terminate yet.
                    pass

        # Database checks ok.

    @inlineCallbacks
    def _get_camera_image_type_id(self):
        if self._camera_image_type_id is not None:
            returnValue(self._camera_image_type_id)
        else:
            result = yield self._database_pool.runQuery(
                    "select image_type_id from image_type where code = 'CAM'")
            self._camera_image_type_id = result[0][0]
            returnValue(self._camera_image_type_id)

    @inlineCallbacks
    def _get_image_source_id(self):
        if self._image_source_id is not None:
            returnValue(self._image_source_id)
        else:
            result = yield self._database_pool.runQuery(
                    "select image_source_id from image_source where code = %s",
                    (self._image_source_code.upper(),))
            if len(result):
                self._image_source_id = result[0][0]
            else:
                raise Exception("Invalid image source {0}".format(
                        self._image_source_code))

            returnValue(self._image_source_id)

    @inlineCallbacks
    def store_image(self, time_stamp, image_data, mime_type=None):

        # Try to guess a MIME type for the image data.
        if mime_type is None:
            with TemporaryFile('rw+b') as f:
                f.write(image_data)
                image_type = imghdr.what(f.name())
                mime_type = \
                    mimetypes.guess_type("foo.{0}".format(image_type))[0]

        query = """
        insert into image(image_type_id, image_source_id, time_stamp,
                          image_data, mime_type)
        values(%(type_id)s, %(source_id)s, %(time_stamp)s, %(data)s, %(mime)s)
        returning image_id
                """

        type_id = yield self._get_camera_image_type_id()
        source_id = yield self._get_image_source_id()

        data = {
            "type_id": type_id,
            "source_id": source_id,
            "time_stamp": time_stamp,
            "data": psycopg2.Binary(image_data),
            "mime": mime_type
        }

        result = yield self._database_pool.runQuery(query, data)

        returnValue(result[0][0])
