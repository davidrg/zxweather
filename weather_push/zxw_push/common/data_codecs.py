# coding=utf-8
"""
Functions for encoding and decoding weather data to be sent over the wire
"""
import calendar
import struct
import datetime
from twisted.python import log

__author__ = 'david'


def timestamp_encode(time_stamp):
    """
    NOTE: supplied time stamp must be at UTC

    :param time_stamp: Timestamp at UTC
    :type time_stamp: datetime.datetime
    """
    return calendar.timegm(time_stamp.timetuple())


def timestamp_decode(value):
    """
    Decodes the supplied timestamp value and returns it as a timestamp at UTC

    :param value: timestamp value
    :type value: int
    :rtype: datetime.datetime
    """
    return datetime.datetime.utcfromtimestamp(value)


def _date_encode(date):
    if date is None:
        return 0xFFFF

    year = date.year
    year -= 2000
    month = date.month
    day = date.day

    value = (year << 9) + (month << 5) + day

    return value


def _date_decode(date):

    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    # | YEAR                      | MONTH         | DAY               |
    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    #    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0

    if date == 0xFFFF:
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
    Encodes the supplied time in the format used by DMP records
    :param record_time: Time to encode
    :type record_time: datetime.time
    :return: Encoded time
    :rtype: int
    """

    if record_time is None:
        return 0xFFFF

    return record_time.hour * 100 + record_time.minute


def _time_decode(binary_val):
    """
    Decodes the time format used in DMP records
    :param binary_val: Time value (short integer)
    :type binary_val: int
    :return: Decoded time
    :rtype: datetime.time
    """

    if binary_val == 0xFFFF:
        return None

    hour = binary_val / 100
    minute = binary_val - (hour * 100)

    return datetime.time(hour=hour, minute=minute)


def _float_encode(value):
    return int(round(value, 1)*10)


def _float_decode(value):
    return value / 10.0


def _float_encode_2dp(value):
    return int(round(value, 2)*100)


def _float_decode_2dp(value):
    return value / 100.0

_INT_8 = "b"
_U_INT_8 = "B"
_INT_16 = "h"
_U_INT_16 = "H"
_INT_32 = "l"
_U_INT_32 = "L"
_BOOL = "?"

_INT_8_NULL = -128
_U_INT_8_NULL = 255
_INT_16_NULL = -32768  # For float this is -3276.8 @ 1dp or -327.68 @ 2dp
_U_INT_16_NULL = 65535  # For float this is 6553.5 @ 1dp or 655.35 @ 2dp
_U_INT_32_NULL = 4294967295

# Live fields in the order they appear in binary encodings.
# Tuple values are:
#   0 - field ID
#   1 - field name
#   2 - field packing type
#   3 - optional function to adjust data before packing
#   4 - optional function to adjust data after unpacking
#   5 - null value
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
    (7, "average_wind_speed", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (8, "gust_wind_speed", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (9, "wind_direction", _U_INT_16, None, None, _U_INT_16_NULL),
    (10, None, None, None, None, None),  # hardware: davis
    (11, None, None, None, None, None),  # hardware: davis
    (12, None, None, None, None, None),  # hardware: davis
    (13, None, None, None, None, None),  # hardware: davis
    (14, None, None, None, None, None),  # hardware: davis
    (15, None, None, None, None, None),  # hardware: davis
    (16, None, None, None, None, None),  # hardware: davis
    (17, None, None, None, None, None),  # hardware: davis
    (18, None, None, None, None, None),  # hardware: davis
    (19, None, None, None, None, None),  # hardware: davis
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

_davis_live_fields = [
    _generic_live_fields[0],  # Live diff sequence
    _generic_live_fields[1],  # sample diff timestamp
    _generic_live_fields[2],  # indoor humidity
    _generic_live_fields[3],  # indoor temperature
    _generic_live_fields[4],  # temperature
    _generic_live_fields[5],  # humidity
    _generic_live_fields[6],  # pressure
    _generic_live_fields[7],  # average wind speed
    _generic_live_fields[8],  # gust wind speed
    _generic_live_fields[9],  # wind direction
    (10, "bar_trend", _INT_8, None, None, _INT_8_NULL),
    (11, "rain_rate", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (12, "storm_rain", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (13, "current_storm_start_date", _U_INT_16, _date_encode, _date_decode,
     _U_INT_16_NULL),
    (14, "transmitter_battery", _U_INT_8, None, None, _U_INT_8_NULL),
    (15, "console_battery_voltage", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (16, "forecast_icon", _U_INT_8, None, None, _U_INT_8_NULL),
    (17, "forecast_rule_id", _U_INT_8, None, None, _U_INT_8_NULL),
    (18, "uv_index", _U_INT_8, _float_encode, _float_decode, _U_INT_8_NULL),
    (19, "solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),
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

_live_fields = {
    "GENERIC": _generic_live_fields,
    "WH1080": _generic_live_fields,
    "DAVIS": _davis_live_fields
}

# List of all field IDs for each station type. This does not include the diff
# id fields.
all_live_field_ids = {
    "GENERIC": range(2, 10),
    "WH1080": range(2, 10),
    "DAVIS": range(2, 20)
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
    _generic_live_fields[7],  # average wind speed
    _generic_live_fields[8],  # gust wind speed
    _generic_live_fields[9],  # wind direction
    (10, "rainfall", _U_INT_16, _float_encode, _float_decode, _U_INT_16_NULL),
    (11, None, None, None, None, None),  # hardware: davis, wh1080
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
    _generic_sample_fields[7],  # average wind speed
    _generic_sample_fields[8],  # gust wind speed
    _generic_sample_fields[9],  # wind direction
    _generic_sample_fields[10],  # rainfall
    (11, "sample_interval", _U_INT_8, None, None, _U_INT_8_NULL),  # minutes
    (12, "record_number", _U_INT_16, None, None, _U_INT_16_NULL),
    (13, "last_in_batch", _BOOL, None, None, None),
    (14, "invalid_data", _BOOL, None, None, None),
    (15, "wind_direction", "3s", None, None, '\xFF\xFF\xFF'),
    (16, "total_rain", _U_INT_32, _float_encode, _float_decode, _U_INT_32_NULL),
    (17, "rain_overflow", _BOOL, None, None, None),
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

_davis_sample_fields = [
    # Num, name, type, encode conversion function, decode conversion function
    (0, None, None, None, None, None),
    _generic_sample_fields[1],  # sample diff timestamp
    _generic_sample_fields[2],  # indoor humidity
    _generic_sample_fields[3],  # indoor temperature
    _generic_sample_fields[4],  # temperature
    _generic_sample_fields[5],  # humidity
    _generic_sample_fields[6],  # pressure
    _generic_sample_fields[7],  # average wind speed
    _generic_sample_fields[8],  # gust wind speed
    _generic_sample_fields[9],  # wind direction
    _generic_sample_fields[10],  # rainfall
    (11, "record_time", _U_INT_16, None, None, None),
    (12, "record_date", _U_INT_16, None, None, None),
    (13, "high_temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (14, "low_temperature", _INT_16, _float_encode_2dp, _float_decode_2dp,
     _INT_16_NULL),
    (15, "high_rain_rate", _INT_16, _float_encode, _float_decode,
     _INT_16_NULL),
    (16, "solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),
    (17, "wind_sample_count", _U_INT_8, None, None, _U_INT_8_NULL),
    (18, "gust_wind_direction", _U_INT_16, _float_encode, _float_decode,
     _U_INT_16_NULL),
    (19, "average_uv_index", _U_INT_8, _float_encode, _float_decode,
     _U_INT_8_NULL),
    (20, "evapotranspiration", _U_INT_32, None, None, _U_INT_32_NULL),
    (21, "high_solar_radiation", _U_INT_16, None, None, _U_INT_16_NULL),
    (22, "high_uv_index", _U_INT_8, _float_encode, _float_decode,
     _U_INT_8_NULL),
    (23, "forecast_rule_id", _U_INT_8, None, None, _U_INT_8_NULL),
    (24, None, None, None, None, None),
    (25, None, None, None, None, None),
    (26, None, None, None, None, None),
    (27, None, None, None, None, None),
    (28, None, None, None, None, None),
    (29, None, None, None, None, None),
    (30, None, None, None, None, None),
    (31, None, None, None, None, None),
]

_sample_fields = {
    "GENERIC": _generic_sample_fields,
    "WH1080": _wh1080_sample_fields,
    "DAVIS": _davis_sample_fields
}

# List of all sample field IDs for each hardware type. This does not include
# the diff id field
all_sample_field_ids = {
    "GENERIC": range(2, 11),
    "WH1080": range(2, 18),
    "DAVIS": range(2, 24)
}

# These are fields that both live and sample records share in common. Same
# field ID, same field name, same data type, etc
common_live_sample_field_ids = {
    "GENERIC": range(1, 10),
    "WH1080": range(1, 10),
    "DAVIS": range(1, 10)
}


def _encode_dict(data_dict, field_definitions, field_ids):

    result = bytearray()

    for field in field_definitions:
        # Num, name, type, encode conversion function, decode conversion
        # function
        field_number = field[0]
        field_name = field[1]

        if field_name is None:
            continue  # Unused field

        field_type = "!" + field[2]
        encode_function = field[3]
        # decode_function = field[4]
        null_value = field[5]

        if field_number not in field_ids:
            continue

        if field_name is None:
            continue  # Reserved field

        if encode_function is None:
            encode_function = lambda x: x

        log.msg("Encode field {0} value {1}".format(field_name, data_dict[field_name]))

        unencoded_value = data_dict[field_name]

        # If these two are equal then we've got a bug! The null values in the
        # field definition tables should never occur in real data.
        assert(unencoded_value != null_value)

        # The field doesn't have a null value defined and we've got a null value
        # for it! We've encountered data we can't encode. This would be a bug.
        assert(not(unencoded_value is None and null_value is None))

        if unencoded_value is None:
            field_value = null_value
        else:
            field_value = encode_function(data_dict[field_name])

        encoded_value = struct.pack(field_type, field_value)

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

        if field_name is None:
            continue  # Unused field

        field_type = "!" + field[2]
        # encode_function = field[3]
        decode_function = field[4]
        null_value = field[5]

        if field_number not in field_ids:
            continue

        if field_name is None:
            continue  # Reserved field

        log.msg("Decode field " + field_name)

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


def encode_live_data(data_dict, hardware_type, field_ids):
    """
    Converts a dictionary containing live data to binary

    :param data_dict: Live data dictionary
    :type data_dict: dict
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :return: Encoded data
    :rtype: bytearray
    """
    return _encode_dict(data_dict,
                        _live_fields[hardware_type.upper()],
                        field_ids)


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


def encode_sample_data(data_dict, hardware_type, field_ids):
    """
    Converts a dictionary containing sample data to binary

    :param data_dict: Sample data dictionary
    :type data_dict: dict
    :param hardware_type: Hardware type code
    :type hardware_type: str
    :param field_ids: List of field IDs to encode
    :type field_ids: list[int]
    :return: Encoded data
    :rtype: bytearray
    """
    return _encode_dict(data_dict,
                        _sample_fields[hardware_type.upper()],
                        field_ids)


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


def calculate_encoded_size(field_ids, hardware_type, is_live):
    """
    Calculates the size required to encode the supplied field IDs for the
    specified hardware type and record type (live or sample)

    :param field_ids: Field IDs to encode
    :type field_ids: list[int]
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

    total_size = 0

    for field in field_definitions:
        field_id = field[0]
        field_name = field[1]
        field_type = field[2]

        if field_id not in field_ids:
            continue

        if field_name is None:
            continue  # Reserved field has no size

        total_size += struct.calcsize(field_type)

    return total_size


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
    :return: list of field IDs that need to be sent
    :rtype: list[int]
    """

    if is_live:
        field_definitions = _live_fields[hardware_type.upper()]
        ignore_fields = [0, 1]  # Live diff ID and sample diff ID
    else:
        field_definitions = _sample_fields[hardware_type.upper()]
        ignore_fields = [1, ]   # Sample diff ID

    result = []

    for field in field_definitions:
        field_id = field[0]
        field_name = field[1]

        if field_name is None:
            continue  # Unused field

        if field_id in ignore_fields:
            continue  # Special non-data field.

        base_value = base_record[field_name]
        target_value = target_record[field_name]

        if base_value != target_value:
            result.append(field_id)

    return result


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
             values in the sample record
    :rtype: list[int]
    """

    live_definitions = _live_fields[hardware_type.upper()]
    sample_definitions = _sample_fields[hardware_type.upper()]

    # We'll check against this list to see what fields *should* be in a sample
    # record instead of just looking at the supplied records keys in case there
    # is other cruft in there not meant for us.
    sample_field_names = [field[1] for field in sample_definitions]

    result = []

    for field in live_definitions:
        field_number = field[0]
        field_name = field[1]

        if field_name is None:
            continue  # Unused field

        if field_name not in sample_field_names:
            # The sample record doesn't contain this field at all. So it must
            # be sent.
            result.append(field_number)
            continue

        base_value = sample_record[field_name]
        live_value = live_record[field_name]

        if base_value != live_value:
            # Fields exist in both records but they have different values. So
            # we must send it.
            result.append(field_name)

    return result


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
    Returns a list of options for sending live field data

    :param live_record: The live record we want to trim unnecessary fields from
    :type live_record: dict
    :param previous_live_record: The last live record that was sent. We don't
                                 know if the server really received this so
                                 diffing against it has some risk.
    :type previous_live_record: dict
    :param previous_sample_record: The last sample record we know for certain
                                   the server received. Diffing against this is
                                   quite safe but has smaller savings.
    :type previous_sample_record: dict
    :param hardware_type: Type of weather station hardware that generated the
                          live and sample records
    :type hardware_type: str
    :return: Set of options. Each option consist of a bool - None for no diff,
             True for live diff, False for sample diff. Plus the list of fields,
             the computed size of the fields and the computed size of the fields
             that were excluded.
    :rtype: ((bool, list[int], int, int), (bool or None, list[int], int, int),
             (bool or None, list[int], int, int))
    """

    all_fields = all_live_field_ids[hardware_type.upper()]
    all_fields_size = calculate_encoded_size(all_fields, hardware_type, True)

    live_diff_field_id = None
    sample_diff_field_id = None

    for field in _live_fields[hardware_type.upper()]:
        if field[1] == "live_diff_sequence":
            live_diff_field_id = field[0]
        elif field[1] == "sample_diff_timestamp":
            sample_diff_field_id = field[0]

        if live_diff_field_id is not None and sample_diff_field_id is not None:
            break  # Done!

    live_diff_fields = None
    live_diff_size = None
    if previous_live_record is not None:
        live_diff_fields = build_field_id_list(previous_live_record,
                                               live_record,
                                               hardware_type,
                                               True)  # True for live

        # Diffing includes a small size penalty as we have to send an additional
        # field we wouldn't normally to indicate what we're diffing against.
        live_diff_fields.append(live_diff_field_id)

        live_diff_size = calculate_encoded_size(live_diff_fields,
                                                hardware_type,
                                                True)  # True for live

    sample_diff_fields = None
    sample_diff_size = None
    if previous_sample_record is not None:
        sample_diff_fields = build_field_id_list_for_live_against_sample(
            previous_sample_record, live_record, hardware_type)

        # Diffing includes a small size penalty as we have to send an additional
        # field we wouldn't normally to indicate what we're diffing against.
        sample_diff_fields.append(sample_diff_field_id)

        sample_diff_size = calculate_encoded_size(sample_diff_fields,
                                                  hardware_type,
                                                  True)  # True for live

    no_diff_option = (None, all_fields, all_fields_size, 0)
    live_diff_option = None
    sample_diff_option = None

    if live_diff_fields is not None:
        saving = all_fields_size - live_diff_size
        live_diff_option = (True, live_diff_fields, live_diff_size, saving)

    if sample_diff_fields is not None:
        saving = all_fields_size - sample_diff_size
        sample_diff_option = (False, sample_diff_fields, sample_diff_size,
                              saving)

    return no_diff_option, live_diff_option, sample_diff_option


def encode_live_record(live_record, previous_live_record,
                       previous_sample_record, hardware_type):
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
    :type previous_live_record: dict
    :param previous_sample_record: The last sample record we know for certain
                                   the server received. Diffing against this is
                                   quite safe but has smaller savings.
    :type previous_sample_record: dict
    :param hardware_type: Type of weather station hardware that generated the
                          live and sample records
    :type hardware_type: str
    :returns: Encoded live record and a list of the fields that were encoded
    :rtype: bytearray, list[int], (int, int, str)
    """

    no_diff_option, live_diff_option, sample_diff_option = \
        get_live_data_field_options(
            live_record, previous_live_record,
            previous_sample_record, hardware_type
        )

    options = [(1, no_diff_option[3])]

    if live_diff_option is not None:
        options.append((2, live_diff_option[3]))

    if sample_diff_option is not None:
        options.append((3, sample_diff_option[3]))

    smallest_option = sorted(options, key=lambda x: x[1], reverse=True)[0][0]

    field_ids = None
    saving = 0
    compression = "none"
    if smallest_option == 1:
        # No diff
        field_ids = no_diff_option[1]
    elif smallest_option == 2:
        # Live diff
        field_ids = live_diff_option[1]
        compression = "live-diff"
        saving = live_diff_option[3]
    elif smallest_option == 3:
        # Sample diff
        field_ids = sample_diff_option[1]
        compression = "sample-diff"
        saving = sample_diff_option[3]

    encoded = encode_live_data(live_record, hardware_type, field_ids)

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
    :type previous_sample_record: dict
    :param hardware_type: Type of weather station hardware that generated the
                          record
    :type hardware_type: str
    :return: Set of options. Each option consist of a bool - False for no diff,
             True for diff. Plus the list of fields, the computed size of the
             fields and the computed size of the fields that were excluded.
    :rtype: ((bool, list[int], int, int), (bool, list[int], int, int))
    """

    all_fields = all_sample_field_ids[hardware_type.upper()]
    all_fields_size = calculate_encoded_size(all_fields, hardware_type, False)

    sample_diff_field_id = None

    for field in _live_fields:
        if field[1] == "sample_diff_timestamp":
            sample_diff_field_id = field[0]
            break

    sample_diff_fields = None
    sample_diff_size = None
    if previous_sample_record is not None:
        sample_diff_fields = build_field_id_list(
            previous_sample_record, sample_record, hardware_type, False)

        # Diffing includes a small (8 byte) size penalty as we have to send an
        # additional field we wouldn't normally to indicate what we're diffing
        # against. If diffing only removes a single 8 byte field then the diffed
        # record would actually be 3 bytes larger than the full record.
        sample_diff_fields.append(sample_diff_field_id)

        sample_diff_size = calculate_encoded_size(sample_diff_fields,
                                                  hardware_type,
                                                  False)  # True for live

    no_diff_option = (False, all_fields, all_fields_size, 0)
    sample_diff_option = None

    if sample_diff_fields is not None:
        saving = all_fields_size - sample_diff_size
        sample_diff_option = (True, sample_diff_fields, sample_diff_size,
                              saving)

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
    :type previous_sample_record: dict
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

    saving = 0
    compression = "none"

    if sample_diff_option is None:
        # Full record is the only option
        field_ids = no_diff_option[1]
    elif sample_diff_option[3] <= no_diff_option[3]:
        # Diffing doesn't remove enough fields to make up for the diff ID we
        # would have to send. Don't bother.
        field_ids = no_diff_option[1]
    else:
        field_ids = sample_diff_option[1]
        saving = sample_diff_option[3]
        compression = "sample-diff"

    encoded = encode_sample_data(sample_record, hardware_type, field_ids)

    uncompressed_size = no_diff_option[2]

    return encoded, field_ids, (uncompressed_size, saving, compression)


def _patch_record(record, base_record, existing_field_ids, all_field_ids,
                  field_definitions):
    missing_fields = [f for f in all_field_ids if f not in existing_field_ids]

    new_record = record

    for field in field_definitions:
        field_id = field[0]
        name = field[1]

        if field_id in missing_fields:
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
    all_fields = all_live_field_ids[hardware_type.upper()]
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
    all_fields = common_live_sample_field_ids[hardware_type.upper()]
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
    all_fields = all_sample_field_ids[hardware_type.upper()]
    field_definitions = _sample_fields[hardware_type.upper()]

    return _patch_record(sample_record, base_sample_record, existing_field_ids,
                         all_fields, field_definitions)