# coding=utf-8
"""
Database functionality used by the WeatherPush server.
"""
import copy
from datetime import datetime

import psycopg2
from psycopg2.extras import DictCursor

from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.defer import returnValue
from twisted.internet.error import ReactorNotRunning
from twisted.python import log

from zxw_push.common.database import wh1080_sample_query
from zxw_push.common.database import davis_sample_query
from zxw_push.common.database import generic_sample_query

__author__ = 'david'


class ServerDatabase(object):
    """
    Database functionality required by the WeatherPush server.
    """

    def __init__(self, dsn):
        self._conn = adbapi.ConnectionPool("psycopg2", dsn,
                                           cursor_factory=DictCursor)
        self._station_code_id = {}
        self._station_code_hw_type = {}
        self._check_db()
        self.schema_version = None

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

        new_row = ServerDatabase._to_real_dict(row)
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

    @staticmethod
    def _flatten_davis_extras_subfield(row):
        new_row = copy.deepcopy(row)

        new_row["leaf_wetness_1"] = new_row["extra_fields"]["leaf_wetness_1"]
        new_row["leaf_wetness_2"] = new_row["extra_fields"]["leaf_wetness_2"]
        new_row["leaf_temperature_1"] = new_row["extra_fields"]["leaf_temperature_1"]
        new_row["leaf_temperature_2"] = new_row["extra_fields"]["leaf_temperature_2"]
        new_row["soil_moisture_1"] = new_row["extra_fields"]["soil_moisture_1"]
        new_row["soil_moisture_2"] = new_row["extra_fields"]["soil_moisture_2"]
        new_row["soil_moisture_3"] = new_row["extra_fields"]["soil_moisture_3"]
        new_row["soil_moisture_4"] = new_row["extra_fields"]["soil_moisture_4"]
        new_row["soil_temperature_1"] = new_row["extra_fields"]["soil_temperature_1"]
        new_row["soil_temperature_2"] = new_row["extra_fields"]["soil_temperature_2"]
        new_row["soil_temperature_3"] = new_row["extra_fields"]["soil_temperature_3"]
        new_row["soil_temperature_4"] = new_row["extra_fields"]["soil_temperature_4"]
        new_row["extra_temperature_1"] = new_row["extra_fields"]["extra_temperature_1"]
        new_row["extra_temperature_2"] = new_row["extra_fields"]["extra_temperature_2"]
        new_row["extra_temperature_3"] = new_row["extra_fields"]["extra_temperature_3"]
        new_row["extra_humidity_1"] = new_row["extra_fields"]["extra_humidity_1"]
        new_row["extra_humidity_2"] = new_row["extra_fields"]["extra_humidity_2"]
        new_row.pop("extra_fields", None)

        return new_row

    @defer.inlineCallbacks
    def _get_schema_version(self):
        result = yield self._conn.runQuery(
            "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")

        if len(result) == 0:
            returnValue(1)

        result = yield self._conn.runQuery(
            "select v::integer from db_info where k = 'DB_VERSION'")

        self.schema_version = result[0][0]

        returnValue(result[0][0])

    @defer.inlineCallbacks
    def _check_db(self):
        """
        Checks the database is compatible
        :return:
        """
        schema_version = yield self._get_schema_version()
        log.msg("Database schema version: {0}".format(schema_version))

        if schema_version < 2:
            log.msg("*** ERROR: WeatherPush requires at least database version "
                    "2 (zxweather 0.2.0). Please upgrade your database.")
            try:
                reactor.stop()
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                pass
        else:
            if schema_version == 2:
                log.msg("*** WARNING: Schema version 2 database (zxweather "
                        "v0.2.x) detected. This configuration is not tested "
                        "often and will be desupported in a future release. You"
                        " may run into issues if you proceed. It is recommended"
                        " you upgrade your database to level 3 (zxweather "
                        "v1.0.0).")

            result = yield self._conn.runQuery(
                "select version_check('WPUSHS',1,0,0)")
            if not result[0][0]:
                result = yield self._conn.runQuery(
                    "select minimum_version_string('WPUSHS')")

                log.msg("*** ERROR: This version of WeatherPush is incompatible"
                        " with the configured database. The minimum WeatherPush "
                        "(or zxweather) version supported by this database is: "
                        "{0}.".format(
                    result[0][0]))
                try:
                    reactor.stop()
                except ReactorNotRunning:
                    # Don't care. We wanted it stopped, turns out it already is
                    # that or its not yet started in which case there isn't
                    # anything much we can do to terminate yet.
                    pass

        # Database checks ok.

    @defer.inlineCallbacks
    def get_station_info(self, authorisation_code):
        """
        Returns a list of stations available in the database.
        :param authorisation_code:
        :return:
        """
        # TODO: filter list of stations to only those the auth code works on

        query = """
        select upper(s.code) as code,
               upper(st.code) as hw_type,
               s.station_id as station_id,
               row_number() OVER () as sid
        from station s
        inner join station_type st on st.station_type_id = s.station_type_id
        -- This should give us a list of stations in the order they were created
        order by s.station_id
        """

        # Note: This query must always return all stations in order for the sid
        # field to be consistent.
        result = yield self._conn.runQuery(query)

        # Cache this information for future reference.
        for record in result:
            code = record[0]
            hw_type = record[1]
            st_id = record[2]
            self._station_code_id[code] = st_id
            self._station_code_hw_type[code] = hw_type

        defer.returnValue(result)

    @defer.inlineCallbacks
    def get_image_type_codes(self):
        """
        Gets a list of all image type codes in the database.
        """
        query = """
        select upper(code) as code
        from image_type
        """

        result = yield self._conn.runQuery(query)

        defer.returnValue(result)

    @defer.inlineCallbacks
    def get_image_sources(self):
        """
        Gets a list of all image sources codes in the database and which station
        code each source code is assocaited with.
        """

        query = """
        select upper(src.code) as source_code,
               upper(stn.code) as station_code
        from image_source src
        inner join station stn on stn.station_id = src.station_id
        """

        result = yield self._conn.runQuery(query)

        defer.returnValue(result)

    @defer.inlineCallbacks
    def store_image(self, source_code, type_code, timestamp, title, description,
                    mime_type, metadata, image_data):

        # Get the type ID
        # TODO: cache this
        query = "select image_type_id from image_type " \
                "where upper(code) = upper(%s)"
        result = yield self._conn.runQuery(query, (type_code, ))
        type_id = result[0][0]

        # Get the source ID
        # TODO: cache this
        query = "select image_source_id from image_source " \
                "where upper(code) = upper(%s)"
        result = yield self._conn.runQuery(query, (source_code, ))
        source_id = result[0][0]

        query = """
        select image_id
        from image
        where image_type_id = %s
          and image_source_id = %s
          and date_trunc('second', time_stamp) = %s at time zone 'GMT'
        """
        result = yield self._conn.runQuery(query, (type_id, source_id,
                                                   timestamp))
        if result is not None and len(result) > 0:
            log.msg("Image {0}/{1}/{2} already exists as {3}. Ignoring.".format(
                type_id, source_id, timestamp, result[0][0]
            ))
            return

        query = """
        insert into image(image_type_id, image_source_id, time_stamp, title,
                          description, image_data, mime_type, metadata)
                    values(%s, %s, %s  at time zone 'GMT', %s, %s, %s, %s, %s)
        """
        yield self._conn.runOperation(query, (type_id, source_id, timestamp,
                                      title, description,
                                      psycopg2.Binary(image_data), mime_type,
                                      metadata))

    @defer.inlineCallbacks
    def store_live_data(self, station_code, live_record):
        """
        Updates live data in the database.
        :param station_code: Station to update live data for
        :type station_code: str
        :param live_record: New live data
        :type live_record: dict
        """

        hardware_type = self._station_code_hw_type[station_code]
        station_id = self._station_code_id[station_code]

        if hardware_type == "GENERIC" or hardware_type == "FOWH1080":
            yield self._store_generic_live(station_id, live_record)
        elif hardware_type == "DAVIS":
            yield self._store_davis_live(station_id, live_record)

    @defer.inlineCallbacks
    def store_sample(self, station_code, sample):
        """
        Stores the supplied sample data in the database.

        :param station_code: Station code the data belongs to
        :type station_code: str
        :param sample: Sample data
        :type sample: dict
        :return:
        """

        hardware_type = self._station_code_hw_type[station_code]
        station_id = self._station_code_id[station_code]

        if hardware_type == "GENERIC":
            yield self._conn.runInteraction(
                ServerDatabase._store_generic_sample_interaction,
                station_id,
                sample)
        elif hardware_type == "FOWH1080":
            yield self._conn.runInteraction(
                ServerDatabase._store_wh1080_sample_interaction,
                station_id,
                sample)
        elif hardware_type == "DAVIS":
            yield self._conn.runInteraction(
                ServerDatabase._store_davis_sample_interaction,
                station_id,
                sample)

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
            query = wh1080_sample_query(False, "")
        elif hw_type == 'DAVIS':
            query = davis_sample_query(False, "")
        else:  # Its GENERIC or something unsupported.
            query = generic_sample_query(False, "")

        parameters = {
            'station_code': station_code,
            'limit': 1
        }

        result = yield self._conn.runQuery(query, parameters)

        if hw_type == 'DAVIS':
            result = self._move_davis_extras_to_subfield(result)

        defer.returnValue(result)

    @defer.inlineCallbacks
    def get_sample(self, station_code, time_stamp, hw_type):
        """
        Fetches the specified sample from the database. If it can not be found
        then None is returned.

        :param station_code: Station to get the sample for
        :type station_code: str
        :param time_stamp: Sample timestamp AT GMT
        :param hw_type: Type of hardware that generated the sample
        :type hw_type: str
        :return: The sample as a dict or None if it doesn't exist
        :rtype: dict or None
        """

        if hw_type == 'FOWH1080':
            # Ascending, no pending clause
            query = wh1080_sample_query(True, "timestamp")
        elif hw_type == 'DAVIS':
            # Ascending, no pending clause
            query = davis_sample_query(True, "timestamp")
        else:  # Its GENERIC or something unsupported.
            # Ascending, no pending clause
            query = generic_sample_query(True, "timestamp")

        parameters = {
            'station_code': station_code,
            'timestamp': time_stamp,
            'limit': 1
        }

        result = yield self._conn.runQuery(query, parameters)

        if len(result) == 0:
            defer.returnValue(None)

        if hw_type == 'DAVIS':
            result = self._move_davis_extras_to_subfield(result)

        defer.returnValue(result[0])

    @defer.inlineCallbacks
    def _store_generic_live(self, station_id, live_record):
        query = """update live_data
            set download_timestamp = %(download_timestamp)s,
                indoor_relative_humidity = %(indoor_humidity)s,
                indoor_temperature = %(indoor_temperature)s,
                relative_humidity = %(humidity)s,
                temperature = %(temperature)s,
                absolute_pressure = %(pressure)s,
                average_wind_speed = %(average_wind_speed)s,
                gust_wind_speed = %(gust_wind_speed)s,
                wind_direction = %(wind_direction)s
            where station_id = %(station_id)s
            """

        live_record["station_id"] = station_id

        if "download_timestamp" not in live_record:
            live_record["download_timestamp"] = datetime.now()

        yield self._conn.runOperation(query, live_record)

    @defer.inlineCallbacks
    def _store_davis_live(self, station_id, live_record):

        # Update the common part first
        yield self._store_generic_live(station_id, live_record)

        live_record = self._flatten_davis_extras_subfield(live_record)

        query = """update davis_live_data
                set bar_trend = %(bar_trend)s,
                    rain_rate = %(rain_rate)s,
                    storm_rain = %(storm_rain)s,
                    current_storm_start_date = %(current_storm_start_date)s,
                    transmitter_battery = %(transmitter_battery)s,
                    console_battery_voltage = %(console_battery_voltage)s,
                    forecast_icon = %(forecast_icon)s,
                    forecast_rule_id = %(forecast_rule_id)s,
                    uv_index = %(uv_index)s,
                    solar_radiation = %(solar_radiation)s,
                    leaf_wetness_1 = %(leaf_wetness_1)s,
                    leaf_wetness_2 = %(leaf_wetness_2)s,
                    leaf_temperature_1 = %(leaf_temperature_1)s,
                    leaf_temperature_2 = %(leaf_temperature_2)s,
                    soil_moisture_1 = %(soil_moisture_1)s,
                    soil_moisture_2 = %(soil_moisture_2)s,
                    soil_moisture_3 = %(soil_moisture_3)s,
                    soil_moisture_4 = %(soil_moisture_4)s,
                    soil_temperature_1 = %(soil_temperature_1)s,
                    soil_temperature_2 = %(soil_temperature_2)s,
                    soil_temperature_3 = %(soil_temperature_3)s,
                    soil_temperature_4 = %(soil_temperature_4)s,
                    extra_temperature_1 = %(extra_temperature_1)s,
                    extra_temperature_2 = %(extra_temperature_2)s,
                    extra_temperature_3 = %(extra_temperature_3)s,
                    extra_humidity_1 = %(extra_humidity_1)s,
                    extra_humidity_2 = %(extra_humidity_2)s
                where station_id = %(station_id)s
                """

        live_record["station_id"] = station_id

        yield self._conn.runOperation(query, live_record)

    @defer.inlineCallbacks
    def _get_sample_id(self, station_id, time_stamp):
        query = "select sample_id from sample " \
                "where station_id = %s and time_stamp = (%s at time zone 'GMT')"
        result = yield self._conn.runQuery(query, (station_id, time_stamp))

        if result is None or len(result) == 0:
            defer.returnValue(None)
        defer.returnValue(result[0][0])

    @staticmethod
    def _get_sample_id_interaction(txn, station_id, time_stamp):
        query = "select sample_id from sample " \
                "where station_id = %s and time_stamp = (%s at time zone 'GMT')"
        txn.execute(query, (station_id, time_stamp))

        result = txn.fetchone()

        if result is None or len(result) == 0:
            return None
        return result[0]

    @staticmethod
    def _store_generic_sample_interaction(txn, station_id, sample_record):

        # See if the sample already exists
        sample_id = ServerDatabase._get_sample_id_interaction(
            txn, station_id, sample_record["time_stamp"])

        if sample_id is not None:
            # Sample already exists. Nothing to do.
            return sample_id

        query = """
        insert into sample(download_timestamp,
                           time_stamp,
                           indoor_relative_humidity,
                           indoor_temperature,
                           relative_humidity,
                           temperature,
                           absolute_pressure,
                           average_wind_speed,
                           gust_wind_speed,
                           wind_direction,
                           rainfall,
                           station_id)
        values(%(download_timestamp)s at time zone 'GMT',
               %(time_stamp)s at time zone 'GMT',
               %(indoor_humidity)s,
               %(indoor_temperature)s,
               %(humidity)s,
               %(temperature)s,
               %(pressure)s,
               %(average_wind_speed)s,
               %(gust_wind_speed)s,
               %(wind_direction)s,
               %(rainfall)s,
               %(station_id)s)
        returning sample_id
        """

        sample_record["station_id"] = station_id

        txn.execute(query, sample_record)
        return txn.fetchone()[0]

    @staticmethod
    def _store_wh1080_sample_interaction(txn, station_id, sample_record):

        # See if the sample already exists
        sample_id = ServerDatabase._get_sample_id_interaction(
            txn, station_id, sample_record["time_stamp"])

        if sample_id is not None:
            # Sample already exists. Nothing to do.
            return sample_id

        sample_id = ServerDatabase._store_generic_sample_interaction(
            txn, station_id, sample_record)

        query = """
        insert into wh1080_sample(sample_id,
                                  sample_interval,
                                  record_number,
                                  last_in_batch,
                                  invalid_data,
                                  total_rain,
                                  rain_overflow,
                                  wind_direction)
        values(%(sample_id)s,
               %(sample_interval)s,
               %(record_number)s,
               %(last_in_batch)s,
               %(invalid_data)s,
               %(total_rain)s,
               %(rain_overflow)s,
               %(wh1080_wind_direction)s)"""

        sample_record["sample_id"] = sample_id

        txn.execute(query, sample_record)

        return sample_id

    @staticmethod
    def _store_davis_sample_interaction(txn, station_id, sample_record):

        sample_id = ServerDatabase._get_sample_id_interaction(
            txn, station_id, sample_record["time_stamp"])

        if sample_id is not None:
            # Sample already exists. Nothing to do.
            return sample_id

        # TODO: Don't do this separately. We need to run it in a single
        # transaction with the rest of this function. Otherwise we risk
        # having part of the insert fail.
        sample_id = ServerDatabase._store_generic_sample_interaction(
            txn, station_id, sample_record)

        sample_record = ServerDatabase._flatten_davis_extras_subfield(sample_record)

        query = """
        insert into davis_sample(sample_id,
                                 record_time,
                                 record_date,
                                 high_temperature,
                                 low_temperature,
                                 high_rain_rate,
                                 solar_radiation,
                                 wind_sample_count,
                                 gust_wind_direction,
                                 average_uv_index,
                                 evapotranspiration,
                                 high_solar_radiation,
                                 high_uv_index,
                                 forecast_rule_id,
                                 leaf_wetness_1,
                                 leaf_wetness_2,
                                 leaf_temperature_1,
                                 leaf_temperature_2,
                                 soil_moisture_1,
                                 soil_moisture_2,
                                 soil_moisture_3,
                                 soil_moisture_4,
                                 soil_temperature_1,
                                 soil_temperature_2,
                                 soil_temperature_3,
                                 soil_temperature_4,
                                 extra_temperature_1,
                                 extra_temperature_2,
                                 extra_temperature_3,
                                 extra_humidity_1,
                                 extra_humidity_2)
        values(%(sample_id)s,
               %(record_time)s,
               %(record_date)s,
               %(high_temperature)s,
               %(low_temperature)s,
               %(high_rain_rate)s,
               %(solar_radiation)s,
               %(wind_sample_count)s,
               %(gust_wind_direction)s,
               %(average_uv_index)s,
               %(evapotranspiration)s,
               %(high_solar_radiation)s,
               %(high_uv_index)s,
               %(forecast_rule_id)s,
               %(leaf_wetness_1)s,
               %(leaf_wetness_2)s,
               %(leaf_temperature_1)s,
               %(leaf_temperature_2)s,
               %(soil_moisture_1)s,
               %(soil_moisture_2)s,
               %(soil_moisture_3)s,
               %(soil_moisture_4)s,
               %(soil_temperature_1)s,
               %(soil_temperature_2)s,
               %(soil_temperature_3)s,
               %(soil_temperature_4)s,
               %(extra_temperature_1)s,
               %(extra_temperature_2)s,
               %(extra_temperature_3)s,
               %(extra_humidity_1)s,
               %(extra_humidity_2)s)"""

        sample_record["sample_id"] = sample_id

        txn.execute(query, sample_record)

        return sample_id
