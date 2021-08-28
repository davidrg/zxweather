# coding=utf-8
"""
Functions for encoding and decoding weather data to be sent over the wire
"""
import calendar
import copy
import struct
import datetime

from twisted.python import log

__author__ = 'david'


def timestamp_encode(time_stamp):
    """
    NOTE: supplied time stamp must be at UTC. Fractional seconds are discarded.

    >>> timestamp_encode(datetime.datetime(2015, 9, 13, 9, 31, 11, 836000))
    1442136671
    >>>
    :param time_stamp: Timestamp at UTC
    :type time_stamp: datetime.datetime
    """
    return calendar.timegm(time_stamp.timetuple())


def timestamp_decode(value):
    """
    Decodes the supplied timestamp value and returns it as a timestamp at UTC.

    >>> timestamp_decode(1442136671)
    datetime.datetime(2015, 9, 13, 9, 31, 11)
    >>>

    :param value: timestamp value
    :type value: int
    :rtype: datetime.datetime
    """
    return datetime.datetime.utcfromtimestamp(value)


def _date_encode(date):
    """
    Encodes the date as a single 16bit integer. None is encoded as 0xFFFF

    >>> _date_encode(datetime.date(2015, 9, 13))
    7981
    >>> _date_encode(None)
    65535
    >>>

    :param date: Date to encode. None is encoded as 0xFFFF.
    :type date: datetime.date or None
    :return: Date encoded as a 16bit integer.
    :rtype: int
    """
    if date is None:
        return 0xFFFF

    year = date.year
    year -= 2000
    month = date.month
    day = date.day

    value = (year << 9) + (month << 5) + day

    return value


def _date_decode(date):
    """
    Decodes a date encoded with _encode_date(). This takes a single 16 bit
    integer as input producing a date. Out of range values give None.

    >>> _date_decode(7981)
    datetime.date(2015, 9, 13)
    >>> _date_decode(65535) is None
    True
    >>> _date_decode(66000) is None
    True
    >>> _date_decode(-1) is None
    True
    >>> _date_decode(0) is None
    True
    >>>

    :param date: Input date encoded as a 16 bit integer
    :type date: int
    :return: Decoded date or None if input was 0xFFFF
    :rtype: datetime.date or None
    """

    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    # | YEAR                      | MONTH         | DAY               |
    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    #    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0

    if date >= 0xFFFF or date <= 0:
        return None

    year_mask = 0xFE00
    month_mask = 0x01E0
    day_mask = 0x001F

    year = 2000 + ((date & year_mask) >> 9)
    month = (date & month_mask) >> 5
    day = (date & day_mask)

    return datetime.date(year=year, month=month, day=day)


def _time_encode(record_time):
    """
    Encodes the supplied time as a 16bit integer in the format used by DMP
    records. None is encoded as 0xFFFF. Seconds are discarded.

    >>> _time_encode(None)
    65535
    >>> _time_encode(datetime.time(9, 50))
    950
    >>>

    :param record_time: Time to encode
    :type record_time: datetime.time or None
    :return: Encoded time
    :rtype: int
    """

    if record_time is None:
        return 0xFFFF

    return record_time.hour * 100 + record_time.minute


def _time_decode(binary_val):
    """
    Decodes times encoded as a 16bit integer by _time_encode(). Out of range
    values are decoded to None.

    >>> _time_decode(-1) is None
    True
    >>> _time_decode(0) is None
    True
    >>> _time_decode(65535) is None
    True
    >>> _time_decode(950)
    datetime.time(9, 50)
    >>>

    :param binary_val: Time value (short integer)
    :type binary_val: int
    :return: Decoded time
    :rtype: datetime.time
    """

    if binary_val >= 0xFFFF or binary_val <= 0:
        return None

    hour = binary_val / 100
    minute = binary_val - (hour * 100)

    return datetime.time(hour=hour, minute=minute)


def _float_encode(value):
    """
    Encodes a float as an integer, rounded to 1 decimal place

    >>> _float_encode(15.389)
    154
    >>>

    :param value: Input float
    :type value: float
    :return: An integer representing the float to 1dp
    :rtype: int
    """

    return int(round(value, 1)*10)


def _float_decode(value):
    """
    Decodes a float encoded with _float_encode().

    >>> _float_decode(154)
    15.4
    >>>

    :param value: integer value representing float to 1dp
    :type value: int
    :return: Float value
    :rtype: float
    """

    return value / 10.0


def _float_encode_2dp(value):
    """
    Encodes a float as an integer, rounded to 2 decimal places

    >>> _float_encode_2dp(15.388)
    1539
    >>>

    :param value: Input float
    :type value: float
    :return: An integer representing the float to 2dp
    :rtype: int
    """
    return int(round(value, 2)*100)


def _float_decode_2dp(value):
    """
    Decodes a float encoded with _float_encode_2dp().

    >>> _float_decode_2dp(1539)
    15.39
    >>>

    :param value: integer value representing float to 2dp
    :type value: int
    :return: Float value
    :rtype: float
    """
    return value / 100.0


def _sample_et_encode(value):
    """
    Encodes evapotranspiration from a davis weather station sample as a one byte
    integer.

    >>> _sample_et_encode(5.4102)
    213
    >>>

    :param value: Input float
    :type value: float
    :return: An integer representing the evapotranspiration
    :rtype: int
    """

    # Even though zxweather uses metric here we're encoding it as inches*1000.
    # This is only because that's how the Davis console/envoy packs ET data into
    # a single byte. This makes 0.255 inches the maximum value we could expect
    # to encounter and also the maximum precision we'll ever encounter.
    return int(round((value / 25.4)*1000, 0))


def _sample_et_decode(value):
    """
    Decodes evapotranspiration from a davis weather station that was encoded as
    a one byte integer

    >>> _sample_et_decode(213)
    5.4102
    >>>

    :param value: integer value representing evapotranspiration
    :type value: int
    :return: Float value
    :rtype: float
    """
    return (value / 1000.0) * 25.4

_INT_8 = "b"
_U_INT_8 = "B"
_INT_16 = "h"
_U_INT_16 = "H"
_INT_32 = "l"
_U_INT_32 = "L"
_BOOL = "?"
_SUB_FIELDS = "!!SUBFIELDS!!"

# These values represent NULL/None/no-value
_INT_8_NULL = -128
_U_INT_8_NULL = 255
_INT_16_NULL = -32768  # For float this is -3276.8 @ 1dp or -327.68 @ 2dp
_U_INT_16_NULL = 65535  # For float this is 6553.5 @ 1dp or 655.35 @ 2dp
_U_INT_32_NULL = 4294967295

# Weather data field definitions
# ==============================
# Each hardware type has its own field set for live and sample data. The field set consists of a description of each
# field with a field description consisting of:
#   * ID (0-31)
#   * Name
#   * Data type
#   * Encode function
#   * Decode function
#   * Null constant
#
# Valid data types are:
#   _INT_8          8 bit signed integer
#   _U_INT_8        8 bit unsigned integer
#   _INT_16         16 bit signed integer
#   _U_INT_16       16 bit unsigned integer
#   _INT_32         32bit signed integer
#   _U_INT_32       31bit unsigned integer
#   _BOOL           true/false (8 bits - rather wasteful)
#   _SUB_FIELDS     Variable length set of up to 32 sub fields.
#
# The _SUB_FIELDS data type is special. Fields of this type consist of a four byte field bitmap followed by up to 32
# sub-fields. A subfield can not be of type _SUB_FIELDS (you cant nest subfields arbitrarily deep). Due to the four byte
# penalty associated with using subfields they should really only be used for sensor values that change rarely.
#
# Field of type _SUB_FIELDS have a slightly different description:
#   * ID (0-31)
#   * Name
#   * Data type
#   * Sub-field set
# Where Sub-field set is a list of field descriptions for all fields the subfields value can contain. Note that a
# subfield can not contain further subfields (the _SUB_FIELDS data type is only valid for the top level field set)

# This defines all of the extra-sensor fields. Most stations won't be
# transmitting this data as it requires fairly expensive extra-sensor
# transmitters.
# TODO: see if we can cram the temperature sensors into one an _INT_8: the station
#   logs in whole fahrenheit so the only reason we're dealing with floats here rather
#   than integers is because of the F->C conversion. If we transmit in whole fahrenheit
#   here it would save one byte per field without any loss in accuracy.
#     Only real down-side to this is support for the WeatherLink Live: This provides
#     higher resolution data for these extra sensors which we'd have to us a 16bit
#     field to transmit.
_davis_extra_fields = [
    # Num, name, type, encode conversion function, decode conversion function
    (0, None, None, None, None, None),
    (1, "leaf_wetness_1", _INT_8, None, None, _INT_8_NULL),  # values 0-15 in steps of 1
    (2, "leaf_wetness_2", _INT_8, None, None, _INT_8_NULL),
    (3, "leaf_temperature_1", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (4, "leaf_temperature_2", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (5, "soil_moisture_1", _U_INT_8, None, None, _U_INT_8_NULL),  # values 0-200 in steps of 1
    (6, "soil_moisture_2", _U_INT_8, None, None, _U_INT_8_NULL),
    (7, "soil_moisture_3", _U_INT_8, None, None, _U_INT_8_NULL),
    (8, "soil_moisture_4", _U_INT_8, None, None, _U_INT_8_NULL),
    (9, "soil_temperature_1", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (10, "soil_temperature_2", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (11, "soil_temperature_3", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (12, "soil_temperature_4", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (13, "extra_temperature_1", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (14, "extra_temperature_2", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (15, "extra_temperature_3", _INT_16, _float_encode, _float_decode, _INT_16_NULL),
    (16, "extra_humidity_1", _INT_8, None, None, _INT_8_NULL),  # Values 0-100 in steps of 1
    (17, "extra_humidity_2", _INT_8, None, None, _INT_8_NULL),
    (18, None, None, None, None, None),
    (19, None, None, None, None, None),
    (20, None, None, None, None, None),
    (21, None, None, None, None, None),
    (22, None, None, None, None, None),
    (23, None, None, None, None, None),
    (24, None, None, None, None, None),
    (25, None, None, None, None, None),
    (26, None, None, None, None, None),
    (27, None, None, None, None, None),
    (28, None, None, None, None, None),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, None, None, None, None, None),
]

# Live fields in the order they appear in binary encodings.
# Tuple values are:
#   0 - field ID
#   1 - field name
#   2 - field packing type
#       If type is a regular field:                             If type is _U_SUB_FIELDS:
#   3 - optional function to adjust data before packing         Field definitions
#   4 - optional function to adjust data after unpacking        None - not used.
#   5 - null value                                              None - reserved.
_generic_live_fields = [
    # Num, name, type, encode conversion function, decode conversion function
    (0, "live_diff_sequence", _U_INT_16, None, None, None),
    (1, "sample_diff_timestamp", _U_INT_32, timestamp_encode, timestamp_decode,
     None),
    (2, "indoor_humidity", _U_INT_8, None, None, _U_INT_8_NULL),
    (3, "indoor_temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (4, "temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (5, "humidity", _U_INT_8, None, None, _U_INT_8_NULL),
    (6, "pressure", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (7, "msl_pressure", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (8, "average_wind_speed", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (9, "gust_wind_speed", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (10, "wind_direction", _U_INT_16, None, None, _U_INT_16_NULL),
    (11, None, None, None, None, None),  # RESERVED for future timestamp field
    (12, None, None, None, None, None),  # hardware: davis
    (13, None, None, None, None, None),  # hardware: davis
    (14, None, None, None, None, None),  # hardware: davis
    (15, None, None, None, None, None),  # hardware: davis
    (16, None, None, None, None, None),  # hardware: davis
    (17, None, None, None, None, None),  # hardware: davis
    (18, None, None, None, None, None),  # hardware: davis
    (19, None, None, None, None, None),  # hardware: davis
    (20, None, None, None, None, None),  # hardware: davis
    (21, None, None, None, None, None),  # hardware: davis
    (22, None, None, None, None, None),  # hardware: davis
    (23, None, None, None, None, None),  # hardware: davis
    (24, None, None, None, None, None),  # hardware: davis
    (25, None, None, None, None, None),  # hardware: davis
    (26, None, None, None, None, None),  # hardware: davis
    (27, None, None, None, None, None),  # hardware: davis
    (28, None, None, None, None, None),  # hardware: davis
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, None, None, None, None, None),  # hardware: davis (subfield)
]

# TODO: hide away the subfield header size stuff somewhere to make it easier to
#   change to 16 bits. The davis extra fields thing only needs 16 subfields so
#   switching to 16 bits would save us two bytes per record containing one or
#   more subfield values

# TODO: For stations not equipped with any extra sensor transmitters its a bit
#   wasteful to include the extra_fields subfield set in any full live or sample
#   record.
#   A live record would end up sending 21 bytes of null values in every full live
#   transmission. In a thirty day month we're transmitting 1,036,800 live records.
#   At the moment every 30th record is transmitted in full so we're transmitting
#   around 34,560 full records. This means we're wasting 725,760 bytes (708.75KiB)
#   per 30 day month just to send nothing.
#   For samples the waste should be less severe as in practice it should be
#   pretty rare for a sample to be transmitted in full and stations without any
#   extra sensor transmitters should have that subfield set excluded from all
#   sample-diff encoded records.

_davis_live_fields = [                                      # Update Frequency
    _generic_live_fields[0],  # Live diff sequence
    _generic_live_fields[1],  # sample diff timestamp
    _generic_live_fields[2],  # indoor humidity
    _generic_live_fields[3],  # indoor temperature
    _generic_live_fields[4],  # temperature                   10 seconds
    _generic_live_fields[5],  # humidity                      50 seconds
    _generic_live_fields[6],  # absolute pressure. Comes from loop2 packets only.
    _generic_live_fields[7],  # mean sea level pressure
    _generic_live_fields[8],  # average wind speed            2.5 seconds
    _generic_live_fields[9],  # gust wind speed               Not used by Davis stations.
    _generic_live_fields[10],  # wind direction               2.5 seconds
    _generic_live_fields[11],  # reserved for future timestamp field
    (12, "bar_trend", _INT_8, None, None, _INT_8_NULL),
    (13, "rain_rate", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (14, "storm_rain", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),        # Every 10 seconds?
    (15, "current_storm_start_date", _U_INT_16, _date_encode, _date_decode,
        _U_INT_16_NULL),
    (16, "transmitter_battery", _U_INT_8, None, None, _U_INT_8_NULL),
    (17, "console_battery_voltage", _U_INT_16, _float_encode, _float_decode,
        _U_INT_16_NULL),
    (18, "forecast_icon", _U_INT_8, None, None, _U_INT_8_NULL),
    (19, "forecast_rule_id", _U_INT_8, None, None, _U_INT_8_NULL),
    (20, "uv_index", _U_INT_8, _float_encode, _float_decode, _U_INT_8_NULL),            # Every 50 seconds
    (21, "solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),                     # Every 50 seconds
    (22, "average_wind_speed_2m", _U_INT_16, _float_encode, _float_decode,      # Loop2
        _U_INT_16_NULL),
    (23, "average_wind_speed_10m", _U_INT_16, _float_encode, _float_decode,
        _U_INT_16_NULL),
    (24, "gust_wind_speed_10m", _U_INT_16, _float_encode, _float_decode,        # Loop2
        _U_INT_16_NULL),
    (25, "gust_wind_direction_10m", _U_INT_16, None, None, _U_INT_16_NULL),     # Loop2
    (26, "heat_index", _INT_16, _float_encode_2dp, _float_decode_2dp,           # Loop2
        _INT_16_NULL),
    (27, "thsw_index", _INT_16, _float_encode_2dp, _float_decode_2dp,           # Loop2
        _INT_16_NULL),
    (28, "altimeter_setting", _U_INT_16, _float_encode, _float_decode,          # Loop2
        _U_INT_16_NULL),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, "extra_fields", _SUB_FIELDS, _davis_extra_fields, None, None),
]

# Davis live fields not currently transmitted:
#   Loop1:  Next Record     Month Rain      Year Rain       Month ET
#           Year ET         Inside Alarms   Rain Alarms     Outside Alarms
#           Ext. TH Alarms  L&S Alarms      Sunrise Time    Sunset Time
#   Loop2:  Dew Point       Wind Chill      Last 15m Rain   Last 1h Rain
#           Last 24h Rain   Barometer reduction method      User barometer offset
#           Barometer calibration number    Barometer sensor raw reading
#
# Of these, there is little point in transmitting:
#   * Dew Point, Wind Chill - easily calculated
#   * Barometer reduction method - this is a constant value. 2 for a VP2.
#   * User barometer offset - this is a constant value set by the user
#   * Barometric calibration number - another constant number
#   * Next record - only really useful internally to the data logger to know
#                   when new data is available
#
# The 7 Rain and evapotranspiration totals might be useful to transmit some day
#   as they're a bit of a pain to keep up-to-date in a live data setting.
#   Problem is any component relying on them would then be davis-specific and
#   additionally the values are maintained by the console so may not always
#   agree with the database.
#
# The alarms could be transmitted if there was a scenario where the data was
#   useful for some purpose. But probably it would be best if zxweather had
#   its own alarms functionality rather than relying on the console.
#
# There is currently space for:
#       2 additional fields in the base live data record
#      14 additional fields in the extra_fields subfield


_live_fields = {
    "GENERIC": _generic_live_fields,
    "FOWH1080": _generic_live_fields,
    "DAVIS": _davis_live_fields
}

# List of all field IDs for each station type. This does not include the diff
# id fields.
all_live_field_ids = {
    "GENERIC": (range(2, 11), None),
    "FOWH1080": (range(2, 11), None),
    "DAVIS": (
        # Skip field 11 - its not currently used.
        range(2, 11) + range(12, 29) + [31],
        {
            "extra_fields": range(1, 18)
        }
    )
}

_generic_sample_fields = [
    # Num, name, type, encode conversion function, decode conversion function
    (0, None, None, None, None, None),
    _generic_live_fields[1],  # sample diff timestamp
    _generic_live_fields[2],  # indoor humidity
    _generic_live_fields[3],  # indoor temperature
    _generic_live_fields[4],  # temperature
    _generic_live_fields[5],  # humidity
    _generic_live_fields[6],  # pressure
    _generic_live_fields[7],  # mean sea level pressure
    _generic_live_fields[8],  # average wind speed
    _generic_live_fields[9],  # gust wind speed
    _generic_live_fields[10],  # wind direction
    (11, "rainfall", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (12, None, None, None, None, None),  # hardware: davis, wh1080
    (13, None, None, None, None, None),  # hardware: davis, wh1080
    (14, None, None, None, None, None),  # hardware: davis, wh1080
    (15, None, None, None, None, None),  # hardware: davis, wh1080
    (16, None, None, None, None, None),  # hardware: davis, wh1080
    (17, None, None, None, None, None),  # hardware: davis, wh1080
    (18, None, None, None, None, None),  # hardware: davis
    (19, None, None, None, None, None),  # hardware: davis
    (20, None, None, None, None, None),  # hardware: davis
    (21, None, None, None, None, None),  # hardware: davis
    (22, None, None, None, None, None),  # hardware: davis
    (23, None, None, None, None, None),  # hardware: davis
    (24, None, None, None, None, None),
    (25, None, None, None, None, None),
    (26, None, None, None, None, None),
    (27, None, None, None, None, None),
    (28, None, None, None, None, None),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, None, None, None, None, None),
]

_wh1080_sample_fields = [
    # Num, name, type, encode conversion function, decode conversion function
    (0, None, None, None, None, None),
    _generic_sample_fields[1],  # sample diff timestamp
    _generic_sample_fields[2],  # indoor humidity
    _generic_sample_fields[3],  # indoor temperature
    _generic_sample_fields[4],  # temperature
    _generic_sample_fields[5],  # humidity
    _generic_sample_fields[6],  # pressure
    _generic_sample_fields[7],  # mean sea level pressure
    _generic_sample_fields[8],  # average wind speed
    _generic_sample_fields[9],  # gust wind speed
    _generic_sample_fields[10],  # wind direction
    _generic_sample_fields[11],  # rainfall
    (12, "sample_interval", _U_INT_8, None, None, _U_INT_8_NULL),  # minutes
    (13, "record_number", _U_INT_16, None, None, _U_INT_16_NULL),
    (14, "last_in_batch", _BOOL, None, None, None),
    (15, "invalid_data", _BOOL, None, None, None),
    (16, "wh1080_wind_direction", "3s", None, None, '\xFF\xFF\xFF'),
    (17, "total_rain", _U_INT_32, _float_encode, _float_decode, _U_INT_32_NULL),
    (18, "rain_overflow", _BOOL, None, None, None),
    (19, None, None, None, None, None),
    (20, None, None, None, None, None),
    (21, None, None, None, None, None),
    (22, None, None, None, None, None),
    (23, None, None, None, None, None),
    (24, None, None, None, None, None),
    (25, None, None, None, None, None),
    (26, None, None, None, None, None),
    (27, None, None, None, None, None),
    (28, None, None, None, None, None),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, None, None, None, None, None),
]

_davis_sample_fields = [
    # Standard field:
    #   Num, name, type, encode conversion function, decode conversion function, null representation
    # Subfield:
    #   Num, name, type, subfield definition, None, None
    (0, None, None, None, None, None),
    _generic_sample_fields[1],  # sample diff timestamp
    _generic_sample_fields[2],  # indoor humidity
    _generic_sample_fields[3],  # indoor temperature
    _generic_sample_fields[4],  # temperature
    _generic_sample_fields[5],  # humidity
    _generic_sample_fields[6],  # pressure
    _generic_sample_fields[7],  # mean sea level pressure
    _generic_sample_fields[8],  # average wind speed
    _generic_sample_fields[9],  # gust wind speed
    _generic_sample_fields[10],  # wind direction
    _generic_sample_fields[11],  # rainfall
    (12, "record_time", _U_INT_16, None, None, None),
    (13, "record_date", _U_INT_16, None, None, None),
    (14, "high_temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (15, "low_temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (16, "high_rain_rate", _INT_16, _float_encode, _float_decode,
     _INT_16_NULL),
    (17, "solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),
    (18, "wind_sample_count", _U_INT_8, None, None, _U_INT_8_NULL),
    (19, "gust_wind_direction", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (20, "average_uv_index", _U_INT_8, _float_encode, _float_decode,
     _U_INT_8_NULL),
    (21, "evapotranspiration", _U_INT_8, _sample_et_encode, _sample_et_decode,
        None),  # Evapotranspiration should never be null - is either 0 or some value
    (22, "high_solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),
    (23, "high_uv_index", _U_INT_8, _float_encode, _float_decode,
     _U_INT_8_NULL),
    (24, "forecast_rule_id", _U_INT_8, None, None, _U_INT_8_NULL),
    (25, None, None, None, None, None),
    (26, None, None, None, None, None),
    (27, None, None, None, None, None),
    (28, None, None, None, None, None),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, "extra_fields", _SUB_FIELDS, _davis_extra_fields, None, None),
]

_sample_fields = {
    "GENERIC": _generic_sample_fields,
    "FOWH1080": _wh1080_sample_fields,
    "DAVIS": _davis_sample_fields
}

# List of all sample field IDs for each hardware type. This does not include
# the diff id field
all_sample_field_ids = {
    "GENERIC": (range(2, 12), None),
    "FOWH1080": (range(2, 19), None),
    "DAVIS": (
        range(2, 25) + [31],
        {
            "extra_fields": range(1, 18)
        }
    )
}

# These are fields that both live and sample records share in common. Same
# field ID, same field name, same data type, etc. Its used for sending live
# records as a set of differences against a sample.
common_live_sample_field_ids = {
    "GENERIC": (range(1, 11), None),
    "FOWH1080": (range(1, 11), None),
    "DAVIS": (
        range(1, 11) + [31],
        {
            "extra_fields": range(1, 18)
        }
    )
}


def get_sample_field_definitions(hw_type):
    return _sample_fields[hw_type.upper()]


def get_live_field_definitions(hw_type):
    return _live_fields[hw_type.upper()]


def _encode_dict(data_dict, field_definitions, field_ids, subfield_ids):
    """
    Encodes a dictionary of values into a byte string using the supplied
    field definitions and list of fields which should be encoded.

    :param data_dict: Data to be encoded
    :type data_dict: dict
    :param field_definitions: List of field definitions. Each field definition
    specifies the fields ID, name, data type and functions to encode/decode the
    data.
    :type: list
    :param field_ids: List of Field IDs which should be included in the output.
    :type: list
    :param subfield_ids: Dictionary of subfield name to list of IDs to encode. If a subfield is being encoded by this
                         _encode_dict call then this parameter should be None as nested subfields are not allowed.
    :type subfield_ids: dict or None
    :return: A byte array representing the input data selected by the list of
    field IDs.
    :rtype: bytearray
    """
    result = bytearray()

    for field in field_definitions:
        # Field definition values:
        field_number = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_name is None:
            continue  # Unused or reserved field

        if field_number not in field_ids:
            continue  # We're not including this field in the output.

        if field_type == _SUB_FIELDS:
            # This field is a collection of additional fields with its own field set header. This means a recursive
            # call to encode the sub-dictionary

            assert subfield_ids is not None, \
                "Subfields not allowed here (subfields nested within subfields?). " \
                "Field: {0}".format(field_name)

            sub_fields = field[3]       # The field definitions for the sub-field set
            #not_used = field[4]
            sub_field_ids = subfield_ids[field_name]

            encoded_value = _encode_dict(data_dict[field_name], sub_fields, sub_field_ids, None)

            # Calculate a header for the sub-field set
            header = struct.pack("!L", set_field_ids(sub_field_ids))

            result += header + encoded_value
        else:
            # A regular scalar field.

            # More field definition value
            field_type = "!" + field_type  # Encode big-endian (! = network/big)
            encode_function = field[3]
            # decode_function = field[4] -- don't need it here
            null_value = field[5]

            if encode_function is None:
                encode_function = lambda x: x  # No special encoding required.

            unencoded_value = data_dict[field_name]

            # If these two are equal then we've got a bug! The null values in the
            # field definition tables should never occur in real data.
            assert unencoded_value != null_value, \
                "Value to encode collides with fields null value. Field {0}, value {1}, null value {2}"\
                .format(field_name, unencoded_value, null_value)

            # The field doesn't have a null value defined and we've got a null value
            # for it! We've encountered data we can't encode. This would be a bug.
            assert not(unencoded_value is None and null_value is None), \
                "Attempted to encode null value for not null field. Field {0}".format(field_name)

            if unencoded_value is None:
                field_value = null_value
            else:
                field_value = encode_function(unencoded_value)

            try:
                encoded_value = struct.pack(field_type, field_value)
            except struct.error as e:
                log.msg("Failed to encode value {0} for field {1} (type {2}) - {3}".format(
                    field_value, field_name, field_type, e.message
                ))
                log.msg(repr(data_dict))

                print("Failed to encode value {0} for field {1} (type {2}) - {3}".format(
                    field_value, field_name, field_type, e.message
                ))
                print(repr(data_dict))
                raise e

            result += encoded_value

    return result


def _decode_dict(encoded_data, field_definitions, field_ids):
    result = {}

    offset = 0

    for field in field_definitions:
        # Num, name, type, encode conversion function, decode conversion
        # function
        field_number = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_number not in field_ids:
            continue

        # If a field is in the field list that means it should have encoded
        # data. Nameless fields don't don't support encoding or decoding so
        # if the field name is None then either the list of field definitions
        # don't match what the encoder used or the list of field IDs is wrong.
        # Either way its a bug.
        assert field_name is not None, \
            "Reserved/unused field included in field list."

        if field_type == _SUB_FIELDS:
            # Here we need to:
            #   1) Calculate how big the subfield set is. This will have to be
            #       done by looking at its header and adding up all the fields
            #       present then adding on the size of the header
            #   2) Fetch out the subfield in its entirety
            #   3) Split the header into a list of field IDs
            #   4) Pass all of this into _decode_dict
            #   5) Merge the resulting dict into result
            #

            subfield_definitions = field[3]

            subfields_header = struct.unpack_from("!L", encoded_data, offset)[0]
            offset += 4  # Skip over the header now we've decoded it.

            # These are the fields we can expect to find:
            subfield_ids_present = get_field_ids_set(subfields_header)

            # Now we figure out how big the subfield set is
            subfields_size = _calculate_encoded_size(subfield_definitions, subfield_ids_present, None)

            # copy that adata out of the encoded data
            subfields_data = encoded_data[offset:offset+subfields_size]
            offset += subfields_size

            # and decode it
            result[field_name] = _decode_dict(subfields_data, subfield_definitions, subfield_ids_present)

        else:
            field_type = "!" + field_type
            # encode_function = field[3]
            decode_function = field[4]
            null_value = field[5]

            field_size = struct.calcsize(field_type)

            if decode_function is None:
                decode_function = lambda x: x

            value = struct.unpack_from(field_type, encoded_data, offset)[0]

            if value == null_value:
                result[field_name] = None
            else:
                result[field_name] = decode_function(value)

            offset += field_size

    return result


def _find_subfield_ids(encoded_data, field_definitions, field_ids):
    """
    This function is similar to _decode_dict in the way it works but instead of
    decoding an encoded dict it only looks to see what subfield ids are present.

    If encoded_data does not fully cover all specified field IDs then None will
    be returned. Call again with more data.

    :param encoded_data: Encoded data for the fields specified in field_ids
    :param field_definitions: Field definitions for the specified field_ids
    :param field_ids: Field_ids known to be present in encoded_data
    :return: List of subfield IDs present or None if insufficient data was supplied
        to complete the search.
    """
    result = {}

    offset = 0

    for field in field_definitions:
        # Num, name, type, encode conversion function, decode conversion
        # function
        field_number = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_number not in field_ids:
            continue

        # If a field is in the field list that means it should have encoded
        # data. Nameless fields don't don't support encoding or decoding so
        # if the field name is None then either the list of field definitions
        # don't match what the encoder used or the list of field IDs is wrong.
        # Either way its a bug.
        assert field_name is not None, \
            "Reserved/unused field included in field list."

        if field_type == _SUB_FIELDS:
            subfield_definitions = field[3]

            if offset+4 > len(encoded_data):
                # Insufficient data to complete search for subfields. Return failure.
                return None

            subfields_header = struct.unpack_from("!L", encoded_data, offset)[0]
            offset += 4  # Skip over the header now we've decoded it.

            # These are the fields we can expect to find:
            subfield_ids_present = get_field_ids_set(subfields_header)

            # Now we figure out how big the subfield set is and skip over it
            subfields_size = _calculate_encoded_size(subfield_definitions, subfield_ids_present, None)
            offset += subfields_size

            # and make a note of the subfield IDs present
            result[field_name] = subfield_ids_present

        else:
            # Skip over the field
            field_type = "!" + field_type
            field_size = struct.calcsize(field_type)
            offset += field_size

        if offset > len(encoded_data):
            # Insufficient data to complete search for subfields. Return failure.
            return None

    return result


def find_live_subfield_ids(encoded_data, field_ids, hw_type):
    return _find_subfield_ids(encoded_data, _live_fields[hw_type.upper()], field_ids)


def find_sample_subfield_ids(encoded_data, field_ids, hw_type):
    return _find_subfield_ids(encoded_data, _sample_fields[hw_type.upper()], field_ids)


def encode_live_data(data_dict, hardware_type, field_ids, subfield_ids):
    """
    Converts a dictionary containing live data to binary

    :param data_dict: Live data dictionary
    :type data_dict: dict
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :param subfield_ids: Dictionary of subfield IDs to encode (key is subfield name, value is list of subfield IDs)
    :type subfield_ids: dict[str,list[int]]
    :return: Encoded data
    :rtype: bytearray
    """
    return _encode_dict(data_dict,
                        _live_fields[hardware_type.upper()],
                        field_ids, subfield_ids)


def decode_live_data(encoded_data, hardware_type, field_ids):
    """
    Converts a binary string containing live data to a dictionary

    :param encoded_data: Live data binary string
    :type encoded_data: bytearray
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :return: Dictionary containing live data
    :rtype: dict
    """
    return _decode_dict(encoded_data,
                        _live_fields[hardware_type.upper()],
                        field_ids)


def encode_sample_data(data_dict, hardware_type, field_ids, subfield_ids):
    """
    Converts a dictionary containing sample data to binary

    :param data_dict: Sample data dictionary
    :type data_dict: dict
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :param subfield_ids: Dictionary of subfield IDs to encode (key is subfield name, value is list of subfield IDs)
    :type subfield_ids: dict[str,list[int]]
    :return: Encoded data
    :rtype: bytearray
    """
    return _encode_dict(data_dict,
                        _sample_fields[hardware_type.upper()],
                        field_ids, subfield_ids)


def decode_sample_data(encoded_data, hardware_type, field_ids):
    """
    Converts a binary string containing sample data to a dictionary

    :param encoded_data: Sample data binary string
    :type encoded_data: bytearray
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :return: Dictionary containing sample data
    :rtype: dict
    """
    return _decode_dict(encoded_data,
                        _sample_fields[hardware_type.upper()],
                        field_ids)


def _calculate_encoded_size(field_definitions, field_ids, subfield_ids):
    """
    Calculates the number of bytes required to encode the specified fields and subfields

    :param field_definitions: Field definitions
    :param field_ids: Fields being encoded
    :param subfield_ids: Subfields being encoded
    :return: Number of bytes to encode specified fields and subfields
    :rtype: int
    """
    total_size = 0
    for field in field_definitions:
        field_id = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_id not in field_ids:
            continue  # Field is not being encoded so has no size

        if field_name is None:
            continue  # Reserved field has no size

        # Field is a subfield set - recursive call to calculate its size
        if field_type == _SUB_FIELDS and subfield_ids is not None:
            sub_field_ids = subfield_ids[field_name]
            subfield_definitions = field[3]

            total_size += 4  # For the header
            total_size += _calculate_encoded_size(subfield_definitions, sub_field_ids, None)
        elif field_type == _SUB_FIELDS:
            raise Exception("Unable to calcualte encoded size: field definitions include one or more subfields but no "
                            "subfield IDs have been specified.")
        else:   # Regular scalar field - size depends on type
            total_size += struct.calcsize("!" + field_type)

    return total_size


def calculate_encoded_size(field_ids, subfield_ids, hardware_type, is_live):
    """
    Calculates the size required to encode the supplied field IDs for the
    specified hardware type and record type (live or sample)

    :param field_ids: Field IDs to encode
    :type field_ids: list[int]
    :param subfield_ids: Dictionary of subfield IDs to encode (key is subfield name, value is list of subfield IDs).
                         If the field set being encoded is itself a subfield then this parameter should be None as
                         nested subfields are not supported.
    :type subfield_ids: dict[str,list[int]] or None
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param is_live: If the data is live data instead of samples
    :type is_live: bool
    :return: Number of bytes required to store the specified field IDs
    :rtype: int
    """
    if is_live:
        field_definitions = _live_fields[hardware_type.upper()]
    else:
        field_definitions = _sample_fields[hardware_type.upper()]

    return _calculate_encoded_size(field_definitions, field_ids, subfield_ids)


def _build_field_id_list(base_record, target_record, field_definitions, ignore_fields):
    """
       Returns a list of field IDs corresponding to the fields in target_record
       that differ from the equivalent field in base_record. Both base_record
       and target_record must be the same type of record (live or sample).

       The output of this function becomes the list of fields that must be transmitted with
       all remaining fields being filled in from the base record by the receiver.

       :param base_record: The record the receiving party already has which is used
                           as a source for the values of some fields
       :type base_record: dict
       :param target_record: The record which is being sent to the receiving party
       :type target_record: dict
       :param field_definitions: Definitions for the fields that could be present
       :type field_definitions: List
       :param ignore_fields: List of fields to ignore. These will never be included in the result
                             even if they differ.
       :type ignore_fields: List[int]
       :return: list of field IDs that need to be sent and a list of subfield IDs that need to be sent
       :rtype: list[int], dict[string, list[int]]
       """
    result = []
    subfields_result = dict()

    for field in field_definitions:
        field_id = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_name is None:
            continue  # Unused field

        if field_id in ignore_fields:
            continue  # Special non-data field.

        if field_name not in base_record or field_name not in target_record:
            # Field is missing from one of the records. This should never happen because by
            # definition the two records are of the same type. But there is nothing much we
            # can really do for now besides ignore it.
            continue

        base_value = base_record[field_name]
        target_value = target_record[field_name]

        if field_type == _SUB_FIELDS:
            # This is a subfields field. Special attention required.
            subfield_definitions = field[3]

            r, sfr = _build_field_id_list(base_value, target_value, subfield_definitions, [])

            subfields_result[field_name] = r

            if len(subfields_result[field_name]) > 0:
                # One or more subfields are different so we need to include the subfield set header
                result.append(field_id)

        elif base_value != target_value:
            result.append(field_id)

    return result, subfields_result


def build_field_id_list(base_record, target_record, hardware_type, is_live):
    """
    Returns a list of field IDs corresponding to the fields in target_record
    that differ from the equivalent field in base_record. Both base_record
    and target_record must be the same type of record (live or sample).

    :param base_record: The record the receiving party already has which is used
                        as a source for the values of some fields
    :type base_record: dict
    :param target_record: The record which is being sent to the receiving party
    :type target_record: dict
    :param hardware_type: Type of hardware that generated both records
    :type hardware_type: str
    :param is_live: If both records are live data
    :type is_live: bool
    :return: list of field IDs that need to be sent and a list of subfield IDs that need to be sent
    :rtype: list[int], dict[string, list[int]]
    """

    if is_live:
        field_definitions = _live_fields[hardware_type.upper()]
        ignore_fields = [0, 1]  # Live diff ID and sample diff ID
    else:
        field_definitions = _sample_fields[hardware_type.upper()]
        ignore_fields = [1, ]   # Sample diff ID

    return _build_field_id_list(base_record, target_record, field_definitions, ignore_fields)


def build_field_id_list_for_live_against_sample(sample_record, live_record,
                                                hardware_type):
    """
    Like build_field_id_list this function builds a list of fields in the
    live_record that must be sent given the server has the specified
    sample_record. This allows some fields to be skipped.

    :param sample_record: Sample record the server is known to have
    :param live_record: Live record to be sent
    :param hardware_type: Station hardware type code
    :return: List of field IDs in the live record that do not have identical
             values in the sample record and a dictionary of subfield fields
             that also don't have identical values in the sample record.
    :rtype: list[int], dict[string, list[int]]
    """

    live_definitions = _live_fields[hardware_type.upper()]
    sample_definitions = _sample_fields[hardware_type.upper()]

    common_sample_fields = common_live_sample_field_ids[hardware_type.upper()][0]

    # We'll check against this list to see what fields *should* be in a sample
    # record instead of just looking at the supplied records keys in case there
    # is other cruft in there not meant for us.
    sample_field_names = [field[1] for field in sample_definitions]

    result = []
    subfields_result = dict()

    for field in live_definitions:
        field_number = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_number in [0, 1]:
            continue  # Special fields

        if field_name is None:
            continue  # Unused field

        if field_name not in sample_field_names or \
                field_number not in common_sample_fields:
            # The sample record doesn't contain this field at all or it contains
            # it but with a different field ID. So it must be sent.
            result.append(field_number)

            # TODO: make the decoder a bit smarter so that the field IDs don't
            # need to match exactly.

            continue

        base_value = sample_record[field_name]
        live_value = live_record[field_name]

        if field_type == _SUB_FIELDS:
            # This is a subfields field. Special attention required.
            subfield_definitions = field[3]
            subfields_result[field_name] = []

            for subfield in subfield_definitions:
                subfield_number = subfield[0]
                subfield_name = subfield[1]

                if subfield_name is None:
                    continue  # Unused field

                if subfield_name not in base_value or subfield_name not in live_value:
                    # Subfield is missing form either the live or base value so it must be
                    # sent.

                    # TODO: make the decoder a bit smarter so subfield IDs don't
                    # need to match exactly

                    # TODO: Should we be using the common field ids thing here?

                    subfields_result[field_name].append(subfield_number)

                base_subfield_value = sample_record[field_name][subfield_name]
                live_subfield_value = live_record[field_name][subfield_name]

                if base_subfield_value != live_subfield_value:
                    # Subfield exists in both records but values differ so we must send it.
                    subfields_result[field_name].append(subfield_number)

            if len(subfields_result[field_name]) > 0:
                # One or more subfields are different so we need to include the subfield set header
                result.append(field_number)

        elif base_value != live_value:
            # Fields exist in both records but they have different values. So
            # we must send it.
            result.append(field_number)

    return result, subfields_result


def get_field_ids_set(field_id_bits):
    """
    Returns the list of field IDs set in the supplied bit field

    >>> get_field_ids_set(512)
    [9]
    >>> get_field_ids_set(0)
    []
    >>> get_field_ids_set(1)
    [0]
    >>> get_field_ids_set(2147483648L)
    [31]
    >>> get_field_ids_set(63)
    [0, 1, 2, 3, 4, 5]

    :param field_id_bits: bit flags containing field IDs
    :type field_id_bits: int
    :return: List of field IDs set
    """
    bits = bin(field_id_bits)[::-1][:-2]

    field_ids = []

    field_id = 0
    for bit in bits:
        if bit == "1":
            field_ids.append(field_id)
        field_id += 1

    return field_ids


def set_field_ids(field_ids):
    """
    Returns the supplied list of Field IDs as an integer

    >>> set_field_ids([9])
    512
    >>> set_field_ids([])
    0
    >>> set_field_ids([0])
    1
    >>> set_field_ids([31])
    2147483648L
    >>> set_field_ids([0,1,2,3,4,5])
    63

    :param field_ids: List of field IDs to pack into a bit field
    :type field_ids: list[int]
    :return: Bit field containing field IDs
    :rtype: int
    """
    ids = ''

    for field_id in range(0, 32):
        if field_id in field_ids:
            ids += '1'
        else:
            ids += '0'

    return int(ids[::-1], 2)


def get_live_data_field_options(live_record, previous_live_record,
                                previous_sample_record, hardware_type):
    """
    Returns a list of options for sending live field data. These could be one or more of:
        no_diff_option - send the full live data record
        live_diff_option - send only the changes from the previous live data record
        sample_diff_option - send only the changes from a particular sample record
        skip_option - don't send anything at all

    :param live_record: The live record we want to trim unnecessary fields from
    :type live_record: dict
    :param previous_live_record: The last live record that was sent. We don't
                                 know if the server really received this so
                                 diffing against it has some risk.
    :type previous_live_record: dict or None
    :param previous_sample_record: The last sample record we know for certain
                                   the server received. Diffing against this is
                                   quite safe but has smaller savings.
    :type previous_sample_record: dict or None
    :param hardware_type: Type of weather station hardware that generated the
                          live and sample records
    :type hardware_type: str
    :return: Set of options. Each option consist of a bool - None for no diff,
             True for live diff, False for sample diff. Plus the list of fields,
             the computed size of the fields and the computed size of the fields
             that were excluded.
    :rtype: ((bool, list[int], int, int), (bool or None, list[int], int, int),
             (bool or None, list[int], int, int),
             (bool or None, list[int], int, int))
    """

    all_fields = all_live_field_ids[hardware_type.upper()][0]
    all_subfields = all_live_field_ids[hardware_type.upper()][1]
    all_fields_size = calculate_encoded_size(all_fields, all_subfields, hardware_type, True)

    live_diff_field_id = None
    sample_diff_field_id = None

    for field in _live_fields[hardware_type.upper()]:
        if field[1] == "live_diff_sequence":
            live_diff_field_id = field[0]
        elif field[1] == "sample_diff_timestamp":
            sample_diff_field_id = field[0]

        if live_diff_field_id is not None and sample_diff_field_id is not None:
            break  # Done!

    skip_available = False

    live_diff_fields = None
    live_diff_subfields = None
    live_diff_size = None
    if previous_live_record is not None:
        live_diff_fields, live_diff_subfields = build_field_id_list(
            previous_live_record, live_record, hardware_type, True)  # True for live

        if len(live_diff_fields) == 0:
            skip_available = True

        # Diffing includes a small size penalty as we have to send an additional
        # field we wouldn't normally to indicate what we're diffing against.
        live_diff_fields.append(live_diff_field_id)

        live_diff_size = calculate_encoded_size(live_diff_fields,
                                                live_diff_subfields,
                                                hardware_type,
                                                True)  # True for live

    sample_diff_fields = None
    sample_diff_subfields = None
    sample_diff_size = None
    if previous_sample_record is not None:
        sample_diff_fields, sample_diff_subfields = build_field_id_list_for_live_against_sample(
            previous_sample_record, live_record, hardware_type)

        # Diffing includes a small size penalty as we have to send an additional
        # field we wouldn't normally to indicate what we're diffing against.
        sample_diff_fields.append(sample_diff_field_id)

        sample_diff_size = calculate_encoded_size(sample_diff_fields,
                                                  sample_diff_subfields,
                                                  hardware_type,
                                                  True)  # True for live

    # (unused, field list, total size, size reduction)
    no_diff_option = (None, all_fields, all_fields_size, 0, all_subfields)
    live_diff_option = None
    sample_diff_option = None
    skip_option = None

    if live_diff_fields is not None:
        saving = all_fields_size - live_diff_size
        live_diff_option = (True, live_diff_fields, live_diff_size, saving, live_diff_subfields)

    if skip_available:
        skip_option = (None, [], 0, all_fields_size)

    if sample_diff_fields is not None:
        saving = all_fields_size - sample_diff_size
        sample_diff_option = (False, sample_diff_fields, sample_diff_size,
                              saving, sample_diff_subfields)

    return no_diff_option, live_diff_option, sample_diff_option, skip_option


def encode_live_record(live_record, previous_live_record,
                       previous_sample_record, hardware_type, compress):
    """
    Encodes a live data record. If previous_live_record and/or
    previous_sample_record are also supplied it may choose to encode the live
    record using the patching mechanism if it is more efficient.

    Because of this, the live_diff_sequence and sample_diff_timestamp keys
    must be present in the live record and have values set. live_diff_sequence
    should contain the sequence number for the supplied previous_live_record.
    sample_diff_timestamp should contain the timestamp for the supplied
    previous_sample_record

    :param live_record: The live record we want to encode
    :param previous_live_record: The last live record that was sent. We don't
                                 know if the server really received this so
                                 diffing against it has some risk.
    :type previous_live_record: dict or None
    :param previous_sample_record: The last sample record we know for certain
                                   the server received. Diffing against this is
                                   quite safe but has smaller savings.
    :type previous_sample_record: dict or None
    :param hardware_type: Type of weather station hardware that generated the
                          live and sample records
    :type hardware_type: str
    :param compress: If the live record should be compressed where possible
    :type compress: bool
    :returns: Encoded live record and a list of the fields that were encoded
    :rtype: bytearray, list[int], (int, int, str)
    """

    no_diff_option, live_diff_option, sample_diff_option, skip_option = \
        get_live_data_field_options(
            live_record, previous_live_record,
            previous_sample_record, hardware_type
        )

    # diff options
    #  0 - diff off/on
    #  1 - field ids
    #  2 - total size
    #  3 - saving
    #  4 - subfield ids

    if compress:
        options = [(1, no_diff_option[3])]

        if live_diff_option is not None:
            options.append((2, live_diff_option[3]))

        if sample_diff_option is not None:
            options.append((3, sample_diff_option[3]))

        if skip_option is not None:
            options.append((4, skip_option[3]))

        smallest_option = sorted(options,
                                 key=lambda x: x[1],
                                 reverse=True)[0][0]
    else:
        smallest_option = 1  # No compression

    field_ids = None
    subfield_ids = None
    saving = 0
    compression = "none"
    if smallest_option == 1:
        # No diff
        field_ids = no_diff_option[1]
        subfield_ids = no_diff_option[4]
    elif smallest_option == 2:
        # Live diff
        field_ids = live_diff_option[1]
        subfield_ids = live_diff_option[4]
        compression = "live-diff"
        saving = live_diff_option[3]
    elif smallest_option == 3:
        # Sample diff
        field_ids = sample_diff_option[1]
        subfield_ids = sample_diff_option[4]
        compression = "sample-diff"
        saving = sample_diff_option[3]
    elif smallest_option == 4:
        # Skip (don't even bother sending a record)
        field_ids = []
        subfield_ids = dict()
        compression = "skip"
        saving = skip_option[3]

    if smallest_option == 4:  # Skip option
        encoded = None
    else:
        encoded = encode_live_data(live_record, hardware_type, field_ids, subfield_ids)

    uncompressed_size = no_diff_option[2]

    return encoded, field_ids, (uncompressed_size, saving, compression)


def get_sample_data_field_options(sample_record, previous_sample_record,
                                  hardware_type):
    """
    Returns a list of options for sending sample field data

    :param sample_record: The sample record we want to trim unnecessary fields
                          from
    :type sample_record: dict
    :param previous_sample_record: The last sample record we know for certain
                                   the server received.
    :type previous_sample_record: dict or None
    :param hardware_type: Type of weather station hardware that generated the
                          record
    :type hardware_type: str
    :return: Set of options. Each option consist of a bool - False for no diff,
             True for diff. Plus the list of fields, the computed size of the
             fields and the computed size of the fields that were excluded.
    :rtype: ((bool, list[int], int, int), (bool, list[int], int, int))
    """

    all_fields = all_sample_field_ids[hardware_type.upper()][0]
    all_subfields = all_sample_field_ids[hardware_type.upper()][1]
    all_fields_size = calculate_encoded_size(all_fields, all_subfields, hardware_type, False)

    sample_diff_field_id = None

    for field in _sample_fields[hardware_type.upper()]:
        if field[1] == "sample_diff_timestamp":
            sample_diff_field_id = field[0]
            break

    sample_diff_fields = None
    sample_diff_subfields = None
    sample_diff_size = None
    if previous_sample_record is not None:
        sample_diff_fields, sample_diff_subfields = build_field_id_list(
            previous_sample_record, sample_record, hardware_type, False)

        # Diffing includes a small (8 byte) size penalty as we have to send an
        # additional field we wouldn't normally to indicate what we're diffing
        # against. If diffing only removes a single 8 byte field then the diffed
        # record would actually be 3 bytes larger than the full record.
        sample_diff_fields.append(sample_diff_field_id)

        sample_diff_size = calculate_encoded_size(sample_diff_fields,
                                                  sample_diff_subfields,
                                                  hardware_type,
                                                  False)  # True for live

    no_diff_option = (False, all_fields, all_fields_size, 0, all_subfields)
    sample_diff_option = None

    if sample_diff_fields is not None:
        saving = all_fields_size - sample_diff_size
        sample_diff_option = (True, sample_diff_fields, sample_diff_size,
                              saving, sample_diff_subfields)

    return no_diff_option, sample_diff_option


def encode_sample_record(sample_record, previous_sample_record, hardware_type):
    """
    Encodes a sample data record. If previous_sample_record is also supplied
    it may choose to encode the record using the patching mechanism if it is
    more efficient.

    Because of this, the sample_diff_timestamp keys must be present in the
    sample record and have its value set to the timestamp for the supplied
    previous_sample_record

    :param sample_record: The sample record we want to encode
    :type sample_record: dict
    :param previous_sample_record: The last sample record we know for certain
                                   the server received.
    :type previous_sample_record: dict or None
    :param hardware_type: Type of weather station hardware that generated the
                          record
    :type hardware_type: str
    :returns: Encoded sample record and a list of the fields that were encoded
    :rtype: bytearray, list[int], (int, int, str)
    """

    no_diff_option, sample_diff_option = get_sample_data_field_options(
        sample_record, previous_sample_record, hardware_type
    )

    # diff options
    #  0 - diff off/on
    #  1 - field ids
    #  2 - total size
    #  3 - saving
    #  4 - subfield ids

    saving = 0
    compression = "none"

    if sample_diff_option is None:
        # Full record is the only option
        field_ids = no_diff_option[1]
        subfield_ids = no_diff_option[4]
    elif sample_diff_option[3] <= no_diff_option[3]:
        # Diffing doesn't remove enough fields to make up for the diff ID we
        # would have to send. Don't bother.
        field_ids = no_diff_option[1]
        subfield_ids = no_diff_option[4]
    else:
        field_ids = sample_diff_option[1]
        subfield_ids = sample_diff_option[4]
        saving = sample_diff_option[3]
        compression = "sample-diff"

    encoded = encode_sample_data(sample_record, hardware_type, field_ids, subfield_ids)

    uncompressed_size = no_diff_option[2]

    return encoded, field_ids, (uncompressed_size, saving, compression)


def _patch_record(record, base_record, existing_field_ids, all_field_ids, field_definitions):
    missing_fields = [f for f in all_field_ids if f not in existing_field_ids]

    new_record = copy.deepcopy(record)

    for field in field_definitions:
        field_id = field[0]
        name = field[1]
        field_type = field[2]

        if name is None:
            continue

        if field_id not in missing_fields and field_type != _SUB_FIELDS:
            # Note that if the field is actually a set of subfields we need to have a
            # look at its subfield header before we can decide if it can be skipped.
            continue

        # If its a subfield and at least one of that subfields fields are present
        # then we'll loop over all the fields in the subfields definition and patch
        # in any that are missing. If the subfield set is entirely missing we'll
        # treat it as a regular field and copy the whole thing over from the base
        # record.
        if field_type == _SUB_FIELDS and name in record:
            subfield_definitions = field[3]

            for subfield in subfield_definitions:
                #subfield_id = subfield[0]
                subfield_name = subfield[1]

                if subfield_name is None:
                    continue

                # We don't have a list of field IDs we've got values for so we'll just go by what dictionary keys are
                # present. If a key is present we've got a value for it, if its not we need to copy the value from
                # the base dictionary.
                if subfield_name in record[name]:
                    continue  # We've already got a value for this subfield

                # If the record doesn't have a value for a subfield and the base record doesn't have it either we've
                # no choice but to skip it
                if subfield_name not in base_record[name]:
                    log.msg("Warning: field {0} in subfield {1} is missing a value in both supplied and base "
                            "records. Further errors may follow.".format(name, subfield_name))
                    continue

                new_record[name][subfield_name] = base_record[name][subfield_name]
        else:
            new_record[name] = base_record[name]

    return new_record


def patch_live_from_live(live_record, base_live_record, existing_field_ids,
                         hardware_type):
    """
    Copies fields from the base live record which are not present in the live
    record according to the list of existing field ids supplied.

    :param live_record: Live record to patch
    :type live_record: dict
    :param base_live_record: Source for missing field data
    :type base_live_record: dict
    :param existing_field_ids: List of fields missing in the record being
                               patched
    :type existing_field_ids: list[int]
    :param hardware_type: Type of hardware that generated both records
    :type hardware_type: str
    :return: Patched live record
    :rtype: dict
    """
    all_fields = all_live_field_ids[hardware_type.upper()][0]

    field_definitions = _live_fields[hardware_type.upper()]

    return _patch_record(live_record, base_live_record, existing_field_ids,
                         all_fields, field_definitions)


def patch_live_from_sample(live_record, base_sample_record, existing_field_ids,
                           hardware_type):
    """
    Copies fields from the base live record which are not present in the live
    record according to the list of existing field ids supplied.

    :param live_record: Live record to patch
    :type live_record: dict
    :param base_sample_record: Source for missing field data
    :type base_sample_record: dict
    :param existing_field_ids: List of fields missing in the record being
                               patched
    :type existing_field_ids: list[int]
    :param hardware_type: Type of hardware that generated both records
    :type hardware_type: str
    :return: Patched live record
    :rtype: dict
    """
    all_fields = common_live_sample_field_ids[hardware_type.upper()][0]
    field_definitions = _sample_fields[hardware_type.upper()]

    return _patch_record(live_record, base_sample_record, existing_field_ids,
                         all_fields, field_definitions)


def patch_sample(sample_record, base_sample_record, existing_field_ids,
                 hardware_type):
    """
    Copies fields from the base sample record which are not present in the
    sample record according to the list of existing field ids supplied.

    :param sample_record: Sample record to patch
    :type sample_record: dict
    :param base_sample_record: Source for missing field data
    :type base_sample_record: dict
    :param existing_field_ids: List of fields missing in the record being
                               patched
    :type existing_field_ids: list[int]
    :param hardware_type: Type of hardware that generated both records
    :type hardware_type: str
    :return: Patched live record
    :rtype: dict
    """
    all_fields = all_sample_field_ids[hardware_type.upper()][0]
    field_definitions = _sample_fields[hardware_type.upper()]

    return _patch_record(sample_record, base_sample_record, existing_field_ids,
                         all_fields, field_definitions)