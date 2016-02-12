# coding=utf-8
"""
Functions for querying the database.
"""
import json
from collections import namedtuple
from twisted.enterprise import adbapi
from twisted.internet import defer
from twisted.python import log

__author__ = 'david'

# For uploading only - missing sample_id, station_id, dew_point, wind_chill
# and apparent_temperature.
BaseSampleRecord = namedtuple(
    'BaseSampleRecord',
    (
        'station_code', 'temperature', 'humidity', 'indoor_temperature',
        'indoor_humidity', 'pressure', 'average_wind_speed', 'gust_wind_speed',
        'wind_direction', 'rainfall', 'download_timestamp', 'time_stamp'
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
        'high_solar_radiation', 'high_uv_index', 'forecast_rule_id'
    )
)

# For uploading only - missing sample_id
BaseLiveRecord = namedtuple(
    'BaseLiveRecord',
    (
        'station_code', 'download_timestamp', 'indoor_humidity',
        'indoor_temperature', 'temperature', 'humidity', 'pressure',
        'average_wind_speed', 'gust_wind_speed', 'wind_direction'
    )
)

# For uploading only - missing sample_id
DavisLiveRecord = namedtuple(
    'DavisLiveRecord',
    (
        'bar_trend', 'rain_rate', 'storm_rain',
        'current_storm_start_date', 'transmitter_battery',
        'console_battery_voltage', 'forecast_icon', 'forecast_rule_id',
        'uv_index', 'solar_radiation'
    )
)

StationInfoRecord = namedtuple(
    'StationInfoRecord',
    ('title','description','sample_interval','live_data_available',
     'station_type_code','station_type_title', 'station_config'))

def database_connect(conn_str):
    """
    Connects to the database.
    :param conn_str: Database connection string
    :type conn_str: str
    """
    global database_pool
    database_pool = adbapi.ConnectionPool("psycopg2", conn_str)

    _prepare_caches()


def _init_station_id_cache(data):
    global station_code_id

    station_code_id = {}

    for row in data:
        station_code_id[row[0]] = row[1]


def _init_hw_cache(data):
    global station_code_hardware_type

    station_code_hardware_type = {}

    for row in data:
        station_code_hardware_type[row[0]] = row[1]

def _prepare_caches():
    """
    Pre-caches station IDs, codes and hardware types types
    :return:
    """
    global database_pool

    query = "select code, station_id from station"
    database_pool.runQuery(query).addCallback(_init_station_id_cache)

    query = "select s.code, st.code as hw_code from station s inner join " \
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
       st.code as station_type_code,
       st.title as station_type_title,
       s.station_config
from station s
inner join station_type st on st.station_type_id = s.station_type_id
where s.code = %s
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

    query = """select s.code, s.title from station s"""
    deferred = database_pool.runQuery(query)
    return deferred

def get_live_csv(station_code):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    base_query = """
select coalesce(round(ld.temperature::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(round(ld.dew_point::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(round(ld.apparent_temperature::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(round(ld.wind_chill::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(ld.relative_humidity::varchar, 'None') || ',' ||
       coalesce(round(ld.indoor_temperature::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(ld.indoor_relative_humidity::varchar, 'None') || ',' ||
       coalesce(ld.absolute_pressure::varchar, 'None') || ',' ||
       coalesce(round(ld.average_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(ld.wind_direction::varchar, 'None') {ext_columns}
from live_data ld {ext_joins}
where ld.station_id = %s
    """

    ext_columns = ''
    ext_joins = ''

    if get_station_hw_type(station_code) == 'DAVIS':
        # Davis hardware stores some additional live data in a different table.
        # We need to join onto that and pull in the columns.
        ext_columns = """ || ',' ||
coalesce(dd.bar_trend::varchar, 'None') || ',' ||
coalesce(dd.rain_rate::varchar, 'None') || ',' ||
coalesce(dd.storm_rain::varchar, 'None') || ',' ||
coalesce(dd.current_storm_start_date::varchar, 'None') || ',' ||
coalesce(dd.transmitter_battery::varchar, 'None') || ',' ||
coalesce(round(dd.console_battery_voltage::numeric, 2)::varchar, 'None') ||','||
coalesce(dd.forecast_icon::varchar, 'None') || ',' ||
coalesce(dd.forecast_rule_id::varchar, 'None') || ',' ||
coalesce(dd.uv_index::varchar, 'None') || ',' ||
coalesce(dd.solar_radiation::varchar, 'None')
        """
        ext_joins = """
inner join davis_live_data dd on dd.station_id = ld.station_id"""

    query = base_query.format(ext_columns=ext_columns, ext_joins=ext_joins)

    return database_pool.runQuery(query, (station_code_id[station_code],))

def get_sample_csv(station_code, start_time, end_time=None):
    """
    Gets live data for the specified station in CSV format.
    :param station_code: The station code
    :type station_code: str
    :param start_time: Obtain all records after this timestamp. If None then
    fetch the latest sample only
    :type start_time: datetime or None.
    :param end_time: Don't get any samples from this point onwards. If None
    then this parameter is ignored.
    :type end_time: datetime or None
    :returns: A deferred which will supply the data
    :rtype: Deferred
    """

    query_cols = """
select time_stamp,
       coalesce(round(temperature::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(round(dew_point::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(round(apparent_temperature::numeric, 2)::varchar, 'None')||','||
       coalesce(round(wind_chill::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(relative_humidity::varchar, 'None') || ',' ||
       coalesce(round(indoor_temperature::numeric, 2)::varchar, 'None') ||','||
       coalesce(indoor_relative_humidity::varchar, 'None') || ',' ||
       coalesce(absolute_pressure::varchar, 'None') || ',' ||
       coalesce(round(average_wind_speed::numeric, 2)::varchar, 'None') || ','||
       coalesce(round(gust_wind_speed::numeric, 2)::varchar, 'None') || ',' ||
       coalesce(wind_direction::varchar, 'None') || ',' ||
       coalesce(rainfall::varchar, 'None')
from sample
where station_id = %s
        """
    query_date = """
and time_stamp > %s
and (%s is null OR time_stamp < %s)
order by time_stamp asc
        """
    query_top = """
order by time_stamp desc
fetch first 1 rows only
    """

    if start_time is not None:
        return database_pool.runQuery(query_cols + query_date,
            (station_code_id[station_code], start_time, end_time, end_time))
    else:
        return database_pool.runQuery(query_cols + query_top,
            (station_code_id[station_code], ))

def get_station_hw_type(code):
    """
    Gets the hardware type for the specified station
    :param code: Station code
    :type code: str
    :return: The stations hardware type code
    :rtype: str
    """
    global station_code_hardware_type
    return station_code_hardware_type[code]

def get_station_id(code):
    """
    Gets the database ID for the specified station
    :param code: Station code
    :type code: str
    :return: The stations ID
    :rtype: int
    """
    global station_code_id
    return station_code_id[code]

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
            forecast_rule_id)
        values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"""

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
                    davis.forecast_rule_id
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
            results.append("# WARN-001: Duplicate sample {0} {1}".format(station_code,
                                                               time_stamp))
        else:
            if hw_type == 'FOWH1080':
                _insert_wh1080_sample_int(txn, base, sample[3], station_id)
            elif hw_type == 'DAVIS':
                _insert_davis_sample_int(txn, base, sample[3], station_id)
            elif hw_type == 'GENERIC':
                _insert_generic_sample_int(txn, base, station_id)

        results.append("CONFIRM {0}\t{1}".format(station_code, time_stamp))

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
        station_code_id[base_data.station_code])

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
        station_code_id[base_data.station_code])

def insert_generic_sample(sample):
    return database_pool.runInteraction(
        _insert_generic_sample_int, sample,
        station_code_id[sample.station_code]
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

    station_id = station_code_id[base_data.station_code]

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
                    solar_radiation = %s
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
        station_code_id[base_data.station_code]
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
    return station_code in station_code_id


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
select st.code, latest.time_stamp, latest.record_number
from station st
left outer join (
select st.code,
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
order by s.time_stamp desc) latest on latest.code = st.code
    """

    deferred = database_pool.runQuery(query)
    deferred.addCallback(convert_to_dict)
    return deferred
