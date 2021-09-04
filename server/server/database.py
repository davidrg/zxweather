# coding=utf-8
"""
Functions for querying the database.
"""
import json
from collections import namedtuple
from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.defer import returnValue
from twisted.internet.error import ReactorNotRunning
from twisted.python import log

__author__ = 'david'

# For uploading only - missing sample_id, station_id, dew_point, wind_chill
# and apparent_temperature.
BaseSampleRecord = namedtuple(
    'BaseSampleRecord',
    (
        'station_code', 'temperature', 'humidity', 'indoor_temperature',
        'indoor_humidity', 'pressure', 'msl_pressure', 'average_wind_speed',
        'gust_wind_speed', 'wind_direction', 'rainfall', 'download_timestamp',
        'time_stamp'
    )
)

# For uploading only - missing sample_id
WH1080SampleRecord = namedtuple(
    'WH1080SampleRecord',
    (
        'sample_interval', 'record_number', 'last_in_batch', 'invalid_data',
        'wind_direction', 'total_rain', 'rain_overflow'
    )
)

# For uploading only - missing sample_id
DavisSampleRecord = namedtuple(
    'DavisSampleRecord',
    (
        'record_time', 'record_date', 'high_temperature', 'low_temperature',
        'high_rain_rate', 'solar_radiation', 'wind_sample_count',
        'gust_wind_direction', 'average_uv_index', 'evapotranspiration',
        'high_solar_radiation', 'high_uv_index', 'forecast_rule_id',
        'leaf_wetness_1', 'leaf_wetness_2', 'leaf_temperature_1',
        'leaf_temperature_2', 'soil_moisture_1', 'soil_moisture_2',
        'soil_moisture_3', 'soil_moisture_4', 'soil_temperature_1',
        'soil_temperature_2', 'soil_temperature_3', 'soil_temperature_4',
        'extra_temperature_1', 'extra_temperature_2', 'extra_temperature_3',
        'extra_humidity_1', 'extra_humidity_2'
    )
)

# For uploading only - missing sample_id
BaseLiveRecord = namedtuple(
    'BaseLiveRecord',
    (
        'station_code', 'download_timestamp', 'indoor_humidity',
        'indoor_temperature', 'temperature', 'humidity', 'pressure',
        'msl_pressure', 'average_wind_speed', 'gust_wind_speed',
        'wind_direction'
    )
)

# For uploading only - missing sample_id
DavisLiveRecord = namedtuple(
    'DavisLiveRecord',
    (
        'bar_trend', 'rain_rate', 'storm_rain',
        'current_storm_start_date', 'transmitter_battery',
        'console_battery_voltage', 'forecast_icon', 'forecast_rule_id',
        'uv_index', 'solar_radiation', 'average_wind_speed_10m',
        'average_wind_speed_2m', 'gust_wind_speed_10m', 'gust_wind_direction_10m',
        'heat_index', 'thsw_index', 'altimeter_setting', 'leaf_wetness_1',
        'leaf_wetness_2', 'leaf_temperature_1', 'leaf_temperature_2',
        'soil_moisture_1', 'soil_moisture_2', 'soil_moisture_3',
        'soil_moisture_4', 'soil_temperature_1', 'soil_temperature_2',
        'soil_temperature_3', 'soil_temperature_4', 'extra_temperature_1',
        'extra_temperature_2', 'extra_temperature_3', 'extra_humidity_1',
        'extra_humidity_2'
    )
)

StationInfoRecord = namedtuple(
    'StationInfoRecord',
    ('title', 'description', 'sample_interval', 'live_data_available',
     'station_type_code', 'station_type_title', 'station_config'))

database_pool = None


def database_connect(conn_str):
    """
    Connects to the database.
    :param conn_str: Database connection string
    :type conn_str: str
    """
    global database_pool
    database_pool = adbapi.ConnectionPool("psycopg2", conn_str)

    _check_db()


@defer.inlineCallbacks
def _get_schema_version():
    result = yield database_pool.runQuery(
        "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")

    if len(result) == 0:
        returnValue(1)

    result = yield database_pool.runQuery(
        "select v::integer from db_info where k = 'DB_VERSION'")

    returnValue(result[0][0])


@defer.inlineCallbacks
def _check_db():
    """
    Checks the database is compatible
    :return:
    """
    schema_version = yield _get_schema_version()
    log.msg("Database schema version: {0}".format(schema_version))

    if schema_version < 3:
        log.msg("*** ERROR: zxweatherd requires at least database version "
                "3 (zxweather 1.0.0). Please upgrade your database.")
        try:
            reactor.stop()
            return
        except ReactorNotRunning:
            # Don't care. We wanted it stopped, turns out it already is
            # that or its not yet started in which case there isn't
            # anything much we can do to terminate yet.
            return
    else:
        result = yield database_pool.runQuery(
            "select version_check('WSERVER',1,0,0)")
        if not result[0][0]:
            result = yield database_pool.runQuery(
                "select minimum_version_string('WSERVER')")

            log.msg("*** ERROR: This version of zxweatherd is incompatible"
                    " with the configured database. The minimum zxweatherd "
                    "(or zxweather) version supported by this database is: "
                    "{0}.".format(
                result[0][0]))
            try:
                reactor.stop()
                return
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                return

    # Database checks ok.
    _prepare_caches()


def _init_station_id_cache(data):
    global station_code_id

    station_code_id = {}

    for row in data:
        station_code_id[row[0].lower()] = row[1]


def _init_hw_cache(data):
    global station_code_hardware_type

    station_code_hardware_type = {}

    for row in data:
        station_code_hardware_type[row[0].lower()] = row[1].upper()


def _prepare_caches():
    """
    Pre-caches station IDs, codes and hardware types types
    :return:
    """
    global database_pool

    query = "select lower(code), station_id from station"
    database_pool.runQuery(query).addCallback(_init_station_id_cache)

    # Hardware type codes are always expected to be uppercase.
    query = "select lower(s.code) as code, upper(st.code) as hw_code from station s inner join " \
            "station_type st on st.station_type_id = s.station_type_id "
    database_pool.runQuery(query).addCallback(_init_hw_cache)


def _get_station_info_dict(result):

    if result is None or len(result) == 0:
        return None

    first = result[0]

    station_config = first[6]
    config = dict()
    if station_config is not None:
        config = json.loads(station_config)

    result = StationInfoRecord(
        title=first[0],
        description=first[1],
        sample_interval=first[2],
        live_data_available=first[3],
        station_type_code=first[4],
        station_type_title=first[5],
        station_config=config
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
       upper(st.code) as station_type_code,
       st.title as station_type_title,
       s.station_config
from station s
inner join station_type st on st.station_type_id = s.station_type_id
where upper(s.code) = upper(%s)
        """

    deferred = database_pool.runQuery(query,(station_code,))
    deferred.addCallback(_get_station_info_dict)
    return deferred


def get_station_list():
    """
    Gets a list of all stations in the database.
    :returns: A deferred which will supply a list of (code,title) pairs.
    :rtype: Deferred
    """

    query = """select lower(s.code) as code, s.title from station s"""
    deferred = database_pool.runQuery(query)
    return deferred


def get_live_csv(station_code, format):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :param format: CSV foromat to get
    :type format: int
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    return database_pool.runQuery(
        "select get_live_text_record(%s, %s);",
        (station_code_id[station_code.lower()], format))


def get_sample_csv(station_code, record_format, start_time, end_time=None, sample_id=None):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :param record_format: The format of records to return
    :type record_format: int
    :param start_time: Obtain all records after this timestamp. If None then
    fetch the latest sample only
    :type start_time: datetime or None.
    :param end_time: Don't get any samples from this point onwards. If None
    then this parameter is ignored.
    :type end_time: datetime or None
    :param sample_id: The sample to fetch
    :type sample_id: int or None
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    query = "select * from get_sample_text_record(%s, %s, %s, %s, %s);"

    return database_pool.runQuery(
        query,
        (station_code_id[station_code.lower()], record_format, sample_id,
         start_time, end_time))


def get_image_csv(image_id):
    """
    Fetches metadata for the specified image that could be used to locate the
    image in other databases where the image may go by a different ID.
    :param image_id:
    :return:
    """

    # This data needs to be sufficient to either:
    #   - Fetch the image from the database without knowing its ID
    #   - Fetch the image by generating a URL for the Web interface
    #
    # The data to uniquely identify the image would be:
    #   - The station Code
    #   - Image source code
    #   - The image type code
    #   - Timestamp

    query = """
    select lower(stn.code) as station_code,
           lower(src.code) as source_code,
           lower(typ.code) as image_type_code,
           img.time_stamp as image_timestamp,
           img.mime_type as mime_type,
           img.image_id as id
    from image img
    inner join image_type typ on typ.image_type_id = img.image_type_id
    inner join image_source src on src.image_source_id = img.image_source_id
    inner join station stn on stn.station_id = src.station_id
    where img.image_id = %s
    """

    return database_pool.runQuery(query, (image_id,))


def get_station_hw_type(code):
    """
    Gets the hardware type for the specified station
    :param code: Station code
    :type code: str
    :return: The stations hardware type code
    :rtype: str
    """
    global station_code_hardware_type
    return station_code_hardware_type[code.lower()].upper()

def get_station_id(code):
    """
    Gets the database ID for the specified station
    :param code: Station code
    :type code: str
    :return: The stations ID
    :rtype: int
    """
    global station_code_id
    return station_code_id[code.lower()]

def _sample_exists(txn, station_id, time_stamp):
    query = "select sample_id from sample " \
            "where station_id = %s and time_stamp = %s::timestamp at time zone 'gmt'"
    txn.execute(query, (station_id, time_stamp))

    result = txn.fetchone()

    if result is None:
        return False
    return True


def _insert_wh1080_sample_int(txn, base, wh1080, station_id):
    """
    Inserts a new sample for WH1080-type stations. This includes a record in
    the sample table and a record in the wh1080_sample table.

    This must be run as a database interaction as we have to insert two
    records in a single transaction.

    :param txn: Database transaction cursor thing
    :param base: Data for the Sample table.
    :type base: BaseSampleRecord
    :param wh1080: Data for the wh1080_sample table
    :type wh1080: WH1080SampleRecord
    :param station_id: The ID of the station we are inserting the record for
    :type station_id: int
    """

    # Insert the base sample record.
    query = """
        insert into sample(download_timestamp, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, station_id)
        values(%s::timestamp at time zone 'gmt',
               %s::timestamp at time zone 'gmt',
               %s, %s, %s, %s, %s, %s, %s, %s, %s)
        returning sample_id
        """
    txn.execute(
        query,
        (
            base.download_timestamp,
            base.time_stamp,
            base.indoor_humidity,
            base.indoor_temperature,
            base.humidity,
            base.temperature,
            base.pressure,
            base.average_wind_speed,
            base.gust_wind_speed,
            base.wind_direction,
            station_id,
        )
    )

    sample_id = txn.fetchone()[0]

    # Now insert the WH1080 record
    query = """
        insert into wh1080_sample(sample_id, sample_interval, record_number,
            last_in_batch, invalid_data, total_rain, rain_overflow,
            wind_direction)
        values(%s, %s, %s, %s, %s, %s, %s, %s)"""

    txn.execute(query,
    (
        sample_id,
        wh1080.sample_interval,
        wh1080.record_number,
        wh1080.last_in_batch,
        wh1080.invalid_data,
        wh1080.total_rain,
        wh1080.rain_overflow,
        wh1080.wind_direction,
    ))

    # Done!
    return None


def _insert_davis_sample_int(txn, base, davis, station_id):
    """
    Inserts a new sample for Davis-type stations. This includes a record in
    the sample table and a record in the davis_sample table.

    This must be run as a database interaction as we have to insert two
    records in a single transaction.

    :param txn: Database transaction cursor thing
    :param base: Data for the Sample table.
    :type base: BaseSampleRecord
    :param davis: Data for the wh1080_sample table
    :type davis: DavisSampleRecord
    :param station_id: The ID of the station we are inserting the record for
    :type station_id: int
    """

    # Insert the base sample record.
    query = """
        insert into sample(download_timestamp, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, rainfall, station_id)
        values(%s::timestamp at time zone 'gmt',
               %s::timestamp at time zone 'gmt',
               %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
        returning sample_id
        """
    txn.execute(
        query,
        (
            base.download_timestamp,
            base.time_stamp,
            base.indoor_humidity,
            base.indoor_temperature,
            base.humidity,
            base.temperature,
            base.pressure,
            base.average_wind_speed,
            base.gust_wind_speed,
            base.wind_direction,
            base.rainfall,
            station_id,
        )
    )

    sample_id = txn.fetchone()[0]

    # Now insert the Davis record
    query = """
        insert into davis_sample(sample_id, record_time, record_date,
            high_temperature, low_temperature, high_rain_rate, solar_radiation,
            wind_sample_count, gust_wind_direction, average_uv_index,
            evapotranspiration, high_solar_radiation, high_uv_index,
            forecast_rule_id, leaf_wetness_1, leaf_wetness_2, 
            leaf_temperature_1, leaf_temperature_2, soil_moisture_1,
            soil_moisture_2, soil_moisture_3, soil_moisture_4, 
            soil_temperature_1, soil_temperature_2, soil_temperature_3,
            soil_temperature_4, extra_temperature_1, extra_temperature_2,
            extra_temperature_3, extra_humidity_1, extra_humidity_2)
        values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, 
               %s, %s, %s, %s, %s, %s, %s, %s,%s, %s, %s, %s, %s, %s, %s)"""

    txn.execute(query,
                (
                    sample_id,
                    davis.record_time,
                    davis.record_date,
                    davis.high_temperature,
                    davis.low_temperature,
                    davis.high_rain_rate,
                    davis.solar_radiation,
                    davis.wind_sample_count,
                    davis.gust_wind_direction,
                    davis.average_uv_index,
                    davis.evapotranspiration,
                    davis.high_solar_radiation,
                    davis.high_uv_index,
                    davis.forecast_rule_id,
                    davis.leaf_wetness_1,
                    davis.leaf_wetness_2,
                    davis.leaf_temperature_1,
                    davis.leaf_temperature_2,
                    davis.soil_moisture_1,
                    davis.soil_moisture_2,
                    davis.soil_moisture_3,
                    davis.soil_moisture_4,
                    davis.soil_temperature_1,
                    davis.soil_temperature_2,
                    davis.soil_temperature_3,
                    davis.soil_temperature_4,
                    davis.extra_temperature_1,
                    davis.extra_temperature_2,
                    davis.extra_temperature_3,
                    davis.extra_humidity_1,
                    davis.extra_humidity_2
                ))

    # Done!
    return None


def _insert_generic_sample_int(txn, data, station_id):
    """
    Inserts a sample for GENERIC hardware (no hardware-specific tables).
    :param data: Sample data
    """
    query = """
        insert into sample(download_timestamp, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, station_id)
        values(%s::timestamp at time zone 'gmt',
               %s::timestamp at time zone 'gmt',
               %s, %s, %s, %s, %s, %s, %s, %s, %s)
        """

    return txn.execute(
        query,
        (
            data.download_timestamp,
            data.time_stamp,
            data.indoor_humidity,
            data.indoor_temperature,
            data.humidity,
            data.temperature,
            data.pressure,
            data.average_wind_speed,
            data.gust_wind_speed,
            data.wind_direction,
            station_id,
        )
    )


def _insert_samples_int(txn, samples):

    results = []

    for sample in samples:
        hw_type = sample[0]
        station_id = sample[1]
        base = sample[2]

        station_code = base.station_code
        time_stamp = base.time_stamp

        if _sample_exists(txn, station_id, base.time_stamp):
            # Sample already exists. Don't bother trying to insert - it will
            # just fail.
            results.append("# WARN-001: Duplicate sample {0} {1}".format(station_code.lower(),
                                                               time_stamp))
        else:
            if hw_type == 'FOWH1080':
                _insert_wh1080_sample_int(txn, base, sample[3], station_id)
            elif hw_type == 'DAVIS':
                _insert_davis_sample_int(txn, base, sample[3], station_id)
            elif hw_type == 'GENERIC':
                _insert_generic_sample_int(txn, base, station_id)

        results.append("CONFIRM {0}\t{1}".format(station_code.lower(), time_stamp))

    return results


def insert_samples(samples):
    """
    Inserts one or more samples for any supported station type.
    :param samples: List of sample tuples. First item must be the base data,
    any additional items are hardware-specific data
    :type samples: list
    :return: Deferred
    """

    return database_pool.runInteraction(
        _insert_samples_int, samples
    )


def insert_wh1080_sample(base_data, wh1080_data):
    """
    Inserts a new sample for WH1080-type stations. This includes a record in
    the sample table and a record in the wh1080_sample table.

    :param base_data: Data for the Sample table.
    :type base_data: BaseSampleRecord
    :param wh1080_data: Data for the wh1080_sample table
    :type wh1080_data: WH1080SampleRecord
    """
    return database_pool.runInteraction(
        _insert_wh1080_sample_int, base_data, wh1080_data,
        station_code_id[base_data.station_code.lower()])

def insert_davis_sample(base_data, davis_data):
    """
    Inserts a new sample for Davis-type stations. This includes a record in
    the sample table and a record in the davis_sample table.

    :param base_data: Data for the Sample table.
    :type base_data: BaseSampleRecord
    :param davis_data: Data for the davis_sample table
    :type davis_data: DavisSampleRecord
    """
    return database_pool.runInteraction(
        _insert_davis_sample_int, base_data, davis_data,
        station_code_id[base_data.station_code.lower()])

def insert_generic_sample(sample):
    return database_pool.runInteraction(
        _insert_generic_sample_int, sample,
        station_code_id[sample.station_code.lower()]
    )


def update_base_live(base_data):
    """
    Updates a basic live data record using the supplied values.
    :param base_data: Live data
    :type base_data: BaseLiveRecord
    """

    def _live_data_err(failure):
        failure.trap(Exception)
        return "# ERR-006: " + failure.getErrorMessage()

    query = """update live_data
                set download_timestamp = %s,
                    indoor_relative_humidity = %s,
                    indoor_temperature = %s,
                    relative_humidity = %s,
                    temperature = %s,
                    absolute_pressure = %s,
                    average_wind_speed = %s,
                    gust_wind_speed = %s,
                    wind_direction = %s
                where station_id = %s
                """

    station_id = station_code_id[base_data.station_code.lower()]

    try:
        return database_pool.runOperation(
            query,
            (
                base_data.download_timestamp,
                base_data.indoor_humidity,
                base_data.indoor_temperature,
                base_data.humidity,
                base_data.temperature,
                base_data.pressure,
                base_data.average_wind_speed,
                base_data.gust_wind_speed,
                base_data.wind_direction,
                station_id
            )
        ).addCallback(lambda _: None).addErrback(_live_data_err)
    except Exception as e:
        return defer.succeed("# ERR-006: " + e.message)


def _update_davis_live_int(txn, base_data, davis_data, station_id):
    """

    :param txn:
    :param base_data:
    :param davis_data:
    :type davis_data: DavisLiveRecord
    :param station_id:
    :return:
    """

    query = """update live_data
                set download_timestamp = %s,
                    indoor_relative_humidity = %s,
                    indoor_temperature = %s,
                    relative_humidity = %s,
                    temperature = %s,
                    absolute_pressure = %s,
                    average_wind_speed = %s,
                    gust_wind_speed = %s,
                    wind_direction = %s
                where station_id = %s
                """

    try:
        txn.execute(
            query,
            (
                base_data.download_timestamp,
                base_data.indoor_humidity,
                base_data.indoor_temperature,
                base_data.humidity,
                base_data.temperature,
                base_data.pressure,
                base_data.average_wind_speed,
                base_data.gust_wind_speed,
                base_data.wind_direction,
                station_id
            )
        )
    except Exception as e:
        return "# ERR-006: " + e.message

    query = """update davis_live_data
                set bar_trend = %s,
                    rain_rate = %s,
                    storm_rain = %s,
                    current_storm_start_date = %s,
                    transmitter_battery = %s,
                    console_battery_voltage = %s,
                    forecast_icon = %s,
                    forecast_rule_id = %s,
                    uv_index = %s,
                    solar_radiation = %s,
                    leaf_wetness_1 = %s,
                    leaf_wetness_2 = %s,
                    leaf_temperature_1 = %s,
                    leaf_temperature_2 = %s,
                    soil_moisture_1 = %s,
                    soil_moisture_2 = %s,
                    soil_moisture_3 = %s,
                    soil_moisture_4 = %s,
                    soil_temperature_1 = %s,
                    soil_temperature_2 = %s,
                    soil_temperature_3 = %s,
                    soil_temperature_4 = %s,
                    extra_temperature_1 = %s,
                    extra_temperature_2 = %s,
                    extra_temperature_3 = %s,
                    extra_humidity_1 = %s,
                    extra_humidity_2 = %s
                where station_id = %s
                """

    try:
        txn.execute(
            query,
            (
                davis_data.bar_trend,
                davis_data.rain_rate,
                davis_data.storm_rain,
                davis_data.current_storm_start_date,
                davis_data.transmitter_battery,
                davis_data.console_battery_voltage,
                davis_data.forecast_icon,
                davis_data.forecast_rule_id,
                davis_data.uv_index,
                davis_data.solar_radiation,
                davis_data.leaf_wetness_1,
                davis_data.leaf_wetness_2,
                davis_data.leaf_temperature_1,
                davis_data.leaf_temperature_2,
                davis_data.soil_moisture_1,
                davis_data.soil_moisture_2,
                davis_data.soil_moisture_3,
                davis_data.soil_moisture_4,
                davis_data.soil_temperature_1,
                davis_data.soil_temperature_2,
                davis_data.soil_temperature_3,
                davis_data.soil_temperature_4,
                davis_data.extra_temperature_1,
                davis_data.extra_temperature_2,
                davis_data.extra_temperature_3,
                davis_data.extra_humidity_1,
                davis_data.extra_humidity_2,
                station_id
            )
        )
    except Exception as e:
        return"# ERR-009: " + e.message

    return None


def update_davis_live(base_data, davis_data):
    """
    Updates live data for a Davis weather station.
    :param base_data: Base live data.
    :type base_data: BaseLiveRecord
    :param davis_data: Davis-specific live data
    :type davis_data: DavisLiveRecord
    :return:
    """
    return database_pool.runInteraction(
        _update_davis_live_int, base_data, davis_data,
        station_code_id[base_data.station_code.lower()]
    )

def station_exists(station_code):
    """
    Returns true if the supplied station code is valid (if it exists in the
    database)
    :param station_code: Station code to validate
    :type station_code: str
    :return: True if the station code is present in the database
    :rtype: bool
    """

    global station_code_id
    return station_code.lower() in station_code_id


def get_latest_sample_info():
    """
    Gets important information about the latest sample required for uploading
    data. What this is depends on what exactly the stations hardware is.
    :returns: Information about the most recent sample wrapped in a Deferred
    :rtype: Deferred
    """

    def convert_to_dict(data):
        """ Converts the query result to a list of dicts and returns it"""

        result = []

        for row in data:
            station = row[0]
            timestamp = row[1]
            wh1080_record_number = row[2]

            value = {
                'station': station,
                'timestamp': timestamp
            }
            if wh1080_record_number is not None:
                value['wh1080_record_number'] = wh1080_record_number

            result.append(value)

        return result

    query = """
select lower(st.code) as code, latest.time_stamp, latest.record_number
from station st
left outer join (
select lower(st.code) as code,
       s.time_stamp::varchar,
       w.record_number
from sample s
inner join station st on st.station_id = s.station_id
inner join (
    -- This will get the timestamp of the most recent record for each station.
    select station_id, max(time_stamp) as max_ts
    from sample
    group by station_id
) as latest on latest.station_id = s.station_id and latest.max_ts = s.time_stamp
left outer join wh1080_sample w on w.sample_id = s.sample_id
order by s.time_stamp desc) latest on lower(latest.code) = lower(st.code)
    """

    deferred = database_pool.runQuery(query)
    deferred.addCallback(convert_to_dict)
    return deferred
