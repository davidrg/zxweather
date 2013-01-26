# coding=utf-8
"""
Functions for querying the database.
"""
from collections import namedtuple
from twisted.enterprise import adbapi
from twisted.internet import defer

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
BaseLiveRecord = namedtuple(
    'BaseLiveRecord',
    (
        'station_code', 'download_timestamp', 'indoor_humidity',
        'indoor_temperature', 'temperature', 'humidity', 'pressure',
        'average_wind_speed', 'gust_wind_speed', 'wind_direction'
    )
)

StationInfoRecord = namedtuple(
    'StationInfoRecord',
    ('title','description','sample_interval','live_data_available',
     'station_type_code','station_type_title'))

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

    result = StationInfoRecord(
        title=first[0],
        description=first[1],
        sample_interval=first[2],
        live_data_available=first[3],
        station_type_code=first[4],
        station_type_title=first[5]
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
       st.title as station_type_title
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

    # TODO: convert to fixed-point data types (no point returning 2.33333333)

    query = """
select coalesce(temperature::varchar, 'None') || ',' ||
       coalesce(dew_point::varchar, 'None') || ',' ||
       coalesce(apparent_temperature::varchar, 'None') || ',' ||
       coalesce(wind_chill::varchar, 'None') || ',' ||
       coalesce(relative_humidity::varchar, 'None') || ',' ||
       coalesce(indoor_temperature::varchar, 'None') || ',' ||
       coalesce(indoor_relative_humidity::varchar, 'None') || ',' ||
       coalesce(absolute_pressure::varchar, 'None') || ',' ||
       coalesce(average_wind_speed::varchar, 'None') || ',' ||
       coalesce(gust_wind_speed::varchar, 'None') || ',' ||
       coalesce(wind_direction::varchar, 'None')
from live_data
where station_id = %s
    """

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

    # TODO: convert to fixed-point data types (no point returning 2.33333333)

    query_cols = """
select time_stamp,
       coalesce(temperature::varchar, 'None') || ',' ||
       coalesce(dew_point::varchar, 'None') || ',' ||
       coalesce(apparent_temperature::varchar, 'None') || ',' ||
       coalesce(wind_chill::varchar, 'None') || ',' ||
       coalesce(relative_humidity::varchar, 'None') || ',' ||
       coalesce(indoor_temperature::varchar, 'None') || ',' ||
       coalesce(indoor_relative_humidity::varchar, 'None') || ',' ||
       coalesce(absolute_pressure::varchar, 'None') || ',' ||
       coalesce(average_wind_speed::varchar, 'None') || ',' ||
       coalesce(gust_wind_speed::varchar, 'None') || ',' ||
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
        values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
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

def insert_generic_sample(data):
    """
    Inserts a sample for GENERIC hardware (no hardware-specific tables).
    :param data: Sample data
    """
    query = """
        insert into sample(download_timestamp, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, station_id)
        values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
        """

    station_id = station_code_id[data.station_code]

    database_pool.runOperation(
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


def insert_base_live(base_data):
    """
    Inserts a new basic live data record using the supplied values.
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
order by s.time_stamp desc
    """

    deferred = database_pool.runQuery(query)
    deferred.addCallback(convert_to_dict)
    return deferred
