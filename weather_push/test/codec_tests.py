import copy
import datetime
import struct
import unittest
from zxw_push.common.data_codecs import _encode_dict, _U_INT_16, _U_INT_32, \
    _U_INT_8, _decode_dict, _SUB_FIELDS, _INT_32, _INT_16, _INT_16_NULL, _INT_8, _calculate_encoded_size, \
    _build_field_id_list, build_field_id_list_for_live_against_sample, get_live_data_field_options, \
    calculate_encoded_size, all_live_field_ids, _davis_live_fields, _davis_sample_fields, encode_live_record, \
    _float_encode, _float_encode_2dp, _date_encode, timestamp_encode, set_field_ids, get_sample_data_field_options, \
    all_sample_field_ids, encode_sample_record, _patch_record, _find_subfield_ids, find_live_subfield_ids, \
    get_sample_field_definitions, _sample_et_encode

__author__ = 'david'


def toHexString(string):
    """
    Converts the supplied string to hex.
    :param string: Input string
    :return:
    """

    if isinstance(string, bytearray):
        string = str(string)

    result = ""
    for char in string:
        if isinstance(char, str):
            hex_encoded = hex(ord(char))[2:]  # Type: str
        else:
            hex_encoded = hex(char)
        if len(hex_encoded) == 1:
            hex_encoded = '0' + hex_encoded

        result += r'\x{0}'.format(hex_encoded)
    return result


DAVIS_LIVE = {
    "live_diff_sequence": 397,  # 0
    "sample_diff_timestamp": 1442136671,  # 1  Common with sample
    "indoor_humidity": 23,  # 2  C
    "indoor_temperature": 5.4,  # 3  C
    "temperature": 9.3,  # 4  C
    "humidity": 83,  # 5  C
    "pressure": 913.4,  # 6  C
    "msl_pressure": 800.1, # 7 C
    "average_wind_speed": 34.1,  # 8  C
    "gust_wind_speed": 98.5,  # 9  C
    "wind_direction": 256,  # 10  C
    # 11 - reserved for future timestamp field
    "bar_trend": 6,  # 12
    "rain_rate": 123,  # 13
    "storm_rain": 985,  # 14
    "current_storm_start_date": 7981,  # 15    2015-09-13
    "transmitter_battery": 2,  # 16
    "console_battery_voltage": 5.9,  # 17
    "forecast_icon": 7,  # 18
    "forecast_rule_id": 10,  # 19
    "uv_index": 9,  # 20
    "solar_radiation": 1043,  # 21
    "average_wind_speed_2m": 5.3,  # 22
    "average_wind_speed_10m": 3.1,  # 23
    "gust_wind_speed_10m": 10.9,  # 24
    "gust_wind_direction_10m": 213,  # 25
    "heat_index": 10.8,  # 26
    "thsw_index": 11.2,  # 27
    "altimeter_setting": 914,  # 28
    "extra_fields": {  # 31    C (all fields)
        "leaf_wetness_1": 12,
        "leaf_wetness_2": 8,
        "leaf_temperature_1": -1.5,
        "leaf_temperature_2": 19,
        "soil_moisture_1": 1,
        "soil_moisture_2": 2,
        "soil_moisture_3": 3,
        "soil_moisture_4": 4,
        "soil_temperature_1": 5,
        "soil_temperature_2": -3.7,
        "soil_temperature_3": 7,
        "soil_temperature_4": 8,
        "extra_temperature_1": 9,
        "extra_temperature_2": 10,
        "extra_temperature_3": -4,
        "extra_humidity_1": 12,
        "extra_humidity_2": 13
    }
}

DAVIS_SAMPLE = {
    "time_stamp": 1442136671,  # Comes from the DB. This is encoded separately for transmission.
    "sample_diff_timestamp": 1442136671,  # Common with live
    "indoor_humidity": 23,  # C
    "indoor_temperature": 5.4,  # C
    "temperature": 9.3,  # C
    "humidity": 83,  # C
    "pressure": 913.4,  # C
    "msl_pressure": 800.1,  # C
    "average_wind_speed": 34.1,  # C
    "gust_wind_speed": 98.5,  # C
    "wind_direction": 256,  # C
    "rainfall": 19.3,
    "record_time": 1234,
    "record_date": 4567,
    "high_temperature": 19.34,
    "low_temperature": 8.3,
    "high_rain_rate": 89.3,
    "solar_radiation": 103,
    "wind_sample_count": 16,
    "gust_wind_direction": 45,
    "average_uv_index": 11.3,
    "evapotranspiration": 1.3,
    "high_solar_radiation": 1154,
    "high_uv_index": 12,
    "forecast_rule_id": 7,
    "extra_fields": {  # C (all fields)
        "leaf_wetness_1": 12,
        "leaf_wetness_2": 8,
        "leaf_temperature_1": -1.5,
        "leaf_temperature_2": 19,
        "soil_moisture_1": 1,
        "soil_moisture_2": 2,
        "soil_moisture_3": 3,
        "soil_moisture_4": 4,
        "soil_temperature_1": 5,
        "soil_temperature_2": -3.7,
        "soil_temperature_3": 7,
        "soil_temperature_4": 8,
        "extra_temperature_1": 9,
        "extra_temperature_2": 10,
        "extra_temperature_3": -4,
        "extra_humidity_1": 12,
        "extra_humidity_2": 13
    }
}

DAVIS_HW_TYPE = "DAVIS"


class EncodeDictTests(unittest.TestCase):
    """
    Unit tests for the _encode_dict function in data_codecs. This function
    handles turning a list of field definitions, field IDs and a dictionary of
    data into a byte array.
    """

    def test_unused_field_is_not_encoded(self):
        """
        Fields with a None name are not used and so should not be encoded.
        """
        field_definitions = [
            (1, None, None, None, None, None)
        ]

        field_ids = [1, ]

        data = {
            "foo": 123
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, bytearray())

    def test_excluded_field_is_not_encoded(self):
        """
        Fields not included in the list of field IDs should not be encoded.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_16, None, None, None),
        ]

        field_ids = []

        data = {
            "live_diff_sequence": 123,
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, bytearray())

    def test_excluded_subfield_set_is_not_encoded(self):
        """
        Subfield sets not included in the list of field IDs should not be encoded.
        """

        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
        ]

        subfield_ids = {
            "subfields": []
        }

        data = {
            "subfields": {
                "subfield": 42
            }
        }

        result = _encode_dict(data, field_definitions, field_ids, subfield_ids)

        self.assertEqual(result, bytearray())

    def test_included_field_is_encoded(self):
        """
        Fields that are in the list of field IDs should be encoded while fields
        that are not should not be encoded.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        field_ids = [
            1
        ]

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, struct.pack("B", 50))

    def test_multi_byte_fields_are_encoded_big_endian(self):
        """
        Multi-byte field should be encoded big-endian.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_16, None, None, None),
            (1, "sample_id", _U_INT_16, None, None, None),
        ]

        field_ids = [
            1
        ]

        value = 61434

        data = {
            "live_diff_sequence": 61534,
            "sample_id": value
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, struct.pack("!H", value))

    def test_multiple_fields_are_encoded(self):
        """
        Multi-byte field should be encoded big-endian.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_16, None, None, None),
            (1, "sample_id", _U_INT_16, None, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        value_0 = 61534
        value_1 = 61434

        data = {
            "live_diff_sequence": value_0,
            "sample_id": value_1
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, struct.pack("!HH", value_0, value_1))

    def test_value_encoding_function_is_used(self):
        """
        Multi-byte field should be encoded big-endian.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_16, lambda x: 5, None, None),
            (1, "sample_id", _U_INT_16, lambda x: x + 10, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        value = 61434
        data = {
            "live_diff_sequence": 61534,
            "sample_id": value
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, struct.pack("!HH", 5, value + 10))

    def test_null_value_is_encoded(self):
        """
        Fields that are in the list of field IDs should be encoded while fields
        that are not should not be encoded.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, 20),
        ]

        field_ids = [
            1
        ]

        data = {
            "live_diff_sequence": 123,
            "sample_id": None
        }

        result = _encode_dict(data, field_definitions, field_ids, None)

        self.assertEqual(result, struct.pack("B", 20))

    def test_null_value_for_not_null_field_fails(self):
        """
        Attempting to encode a null value for a field that doesn't support it
        should cause a failed assertion.
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        field_ids = [
            1
        ]

        data = {
            "live_diff_sequence": 123,
            "sample_id": None
        }

        self.assertRaises(AssertionError,
                          _encode_dict, *(data, field_definitions, field_ids, None))

    def test_value_matching_null_value_fails(self):
        """
        Attempting to encode a value which matches a fields null value should
        cause a failed assertion
        """

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, 124),
        ]

        field_ids = [
            1
        ]

        data = {
            "live_diff_sequence": 123,
            "sample_id": 124
        }

        self.assertRaises(AssertionError,
                          _encode_dict, *(data, field_definitions, field_ids, None))

    def test_subfield_encoded(self):
        """
        Tests a single subfield in a single subfield set is encoded
        """

        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1]
        }

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50,
            "subfields": {
                "subfield": 42
            }
        }

        # The result should be the sample_id field:
        expected_result = struct.pack("B", data["sample_id"])

        # Plus the subfields header and its payload. The header should be 0x02 or, in binary:
        # 0000 0000 0000 0000 0000 0000 0000 0010
        #
        # So the total subfield value should be:
        expected_result = expected_result + struct.pack("!LB", 2, data["subfields"]["subfield"])

        # Overall expected result should be: 0x32 0x00 0x00 0x00 0x02 0x2A
        print("Expected result: " + toHexString(expected_result))

        result = _encode_dict(data, field_definitions, field_ids, subfield_ids)

        print("Received result: " + toHexString(result))

        self.assertEqual(result, expected_result)

    def test_subfields_encoded(self):
        """
        Tests multiple subfields in a subfield set are encoded including with a single null
        value
        """
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1, 2, 3, 4]
        }

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50,
            "subfields": {
                "a": 42,
                "b": 1967347807,
                "c": 5193,
                "d": None
            }
        }

        # The result should be the sample_id field:
        expected_result = struct.pack("B", data["sample_id"])

        # Plus the subfields header and its payload. The header should be 0x1E or, in binary:
        # 0000 0000 0000 0000 0000 0000 0001 1110
        #
        # So the total subfield value should be:
        expected_result += struct.pack(
            "!LBlHh",
            0x1E,
            data["subfields"]["a"],
            data["subfields"]["b"],
            data["subfields"]["c"],
            _INT_16_NULL,
        )

        # Overall expected result should be:
        # 0x32 0x00 0x00 0x00 0x1E 0x2A 0x75 0x43 0x58 0x5F 0x14 0x49 0x80 0x00
        # |    \-subfield header-/  A   \------- B -------/ \-- C --/ \-- D --/
        # sample_id
        print("Expected result: " + toHexString(expected_result))

        result = _encode_dict(data, field_definitions, field_ids, subfield_ids)

        print("Received result: " + toHexString(result))

        self.assertEqual(result, expected_result)

    def test_some_subfields_encoded(self):
        """
        Tests only specified subfields in a subfield set are encoded
        """
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1, 3] # Skip fields 2 and 4
        }

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50,
            "subfields": {
                "a": 42,
                "b": 1967347807,
                "c": 5193,
                "d": None
            }
        }

        # The result should be the sample_id field:
        expected_result = struct.pack("B", data["sample_id"])

        # Plus the subfields header and its payload. The header should be 0x0A or, in binary:
        # 0000 0000 0000 0000 0000 0000 0000 1010
        #
        # So the total subfield value should be:
        expected_result += struct.pack(
            "!LBH",
            0x0A,
            data["subfields"]["a"],
            data["subfields"]["c"],
        )

        # Overall expected result should be:
        # 0x32 0x00 0x00 0x00 0x1E 0x2A 0x14 0x49
        # |    \-subfield header-/  A   \-- C --/
        # sample_id
        print("Expected result: " + toHexString(expected_result))

        result = _encode_dict(data, field_definitions, field_ids, subfield_ids)

        print("Received result: " + toHexString(result))

        self.assertEqual(result, expected_result)

    def test_multiple_subfield_sets_encoded(self):
        """
        Tests that multiple sets of subfields are encoded
        """
        subfields_a_definition = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        subfields_b_definition = [
            (0, None, None, None, None, None),
            (1, "e", _U_INT_8, None, None, None),
            (2, "f", _INT_32, None, None, None),
            (3, "g", _U_INT_16, None, None, None),
            (4, "h", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields-a", _SUB_FIELDS, subfields_a_definition, None, None),
            (3, "scalar", _INT_8, None, None, None),
            (4, "subfields-b", _SUB_FIELDS, subfields_b_definition, None, None)
        ]

        field_ids = [
            1,
            2,
            3,
            4
        ]

        subfield_ids = {
            "subfields-a": [1, 2, 3, 4],
            "subfields-b": [1, 3, 4]
        }

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50,
            "subfields-a": {
                "a": 42,
                "b": 1967347807,
                "c": 5193,
                "d": None
            },
            "scalar": 93,
            "subfields-b": {
                "e": 91,
                "f": 3194324123,
                "g": 1234,
                "h": None
            }
        }

        # The result should be the sample_id field:
        expected_result = struct.pack("B", data["sample_id"])

        # Plus the subfields header and its payload. The header should be 0x1E or, in binary:
        # 0000 0000 0000 0000 0000 0000 0001 1110
        #
        # So the total subfield value should be:
        expected_result += struct.pack(
            "!LBlHh",
            0x1E,
            data["subfields-a"]["a"],
            data["subfields-a"]["b"],
            data["subfields-a"]["c"],
            _INT_16_NULL,
        )

        # Plus the scalar value:
        expected_result += struct.pack("B", data["scalar"])

        # Plus the extra subfields. The header for the second lot should be 0x1A (dropped subfield
        # number 2)
        expected_result += struct.pack(
            "!LBHh",
            0x1A,
            data["subfields-b"]["e"],
            data["subfields-b"]["g"],
            _INT_16_NULL,
        )

        print("Expected result: " + toHexString(expected_result))

        result = _encode_dict(data, field_definitions, field_ids, subfield_ids)

        print("Received result: " + toHexString(result))

        self.assertEqual(result, expected_result)

    def test_subfield_within_subfield_fails(self):
        """
        Tests that subfields nested within subfields will be rejected
        """

        subfield_subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "foo", _U_INT_8, None, None, None)
        ]

        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None),
            (2, "subsubfield", _SUB_FIELDS, subfield_subfield_definitions, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1, 2]
        }

        data = {
            "live_diff_sequence": 123,
            "sample_id": 50,
            "subfields": {
                "subfield": 42,
                "subsubfield": {
                    "foo": 5
                }
            }
        }

        self.assertRaises(AssertionError,
                          _encode_dict, *(data, field_definitions, field_ids, subfield_ids))


class DecodeDictTests(unittest.TestCase):
    def test_single_value_is_decoded(self):
        """
        Checks that a single field is decoded according to the field definition.
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        field_ids = [
            1
        ]

        value_1 = 42

        encoded = struct.pack("B", value_1)

        expected_dict = {
            "sample_id": value_1
        }

        result = _decode_dict(encoded, field_definitions, field_ids)

        self.assertDictEqual(result, expected_dict)

    def test_multiple_values_are_decoded(self):
        """
        Checks that multiple fields are decoded according to the field spec
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_16, None, None, None),
            (1, "sample_id", _U_INT_16, None, None, None),
            (2, "test", _U_INT_16, None, None, None),
        ]

        field_ids = [
            0,
            1,
            2
        ]

        value_0 = 60535
        value_1 = 61535
        value_2 = 62535

        encoded = struct.pack("!HHH", value_0, value_1, value_2)

        expected_dict = {
            "live_diff_sequence": value_0,
            "sample_id": value_1,
            "test": value_2
        }

        result = _decode_dict(encoded, field_definitions, field_ids)

        self.assertDictEqual(result, expected_dict)

    def test_multi_byte_values_are_decoded_big_endian(self):
        """
        Checks that multi-byte fields are decoded as big-endian
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_16, None, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        value_0 = 123
        value_1 = 42589

        encoded = struct.pack("!BH", value_0, value_1)

        expected_dict = {
            "live_diff_sequence": value_0,
            "sample_id": value_1
        }

        result = _decode_dict(encoded, field_definitions, field_ids)

        self.assertDictEqual(result, expected_dict)

    def test_decoding_unnamed_fields_fails(self):
        """
        Checks that attempting to decode nameless fields fails. Nameless fields
        can't be encoded or decoded and so should never be included in the
        field list.
        """
        field_definitions = [
            (0, None, _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_16, None, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        value_1 = 42589

        encoded = struct.pack("!H", value_1)

        self.assertRaises(AssertionError, _decode_dict,
                          *(encoded, field_definitions, field_ids))

    def test_null_values_are_decoded(self):
        """
        Checks that null values are decoded appropriately
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, 50),
            (1, "sample_id", _U_INT_16, None, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        value_0 = 50
        value_1 = 42589

        encoded = struct.pack("!BH", value_0, value_1)

        expected_dict = {
            "live_diff_sequence": None,
            "sample_id": value_1
        }

        result = _decode_dict(encoded, field_definitions, field_ids)

        self.assertDictEqual(result, expected_dict)

    def test_decode_function_is_used(self):
        """
        Checks that null values are deocded appropriately
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, lambda x: 20, None),
            (1, "sample_id", _U_INT_16, None, lambda x: x-10, None),
        ]

        field_ids = [
            0,
            1
        ]

        value_0 = 50
        value_1 = 42589

        encoded = struct.pack("!BH", value_0, value_1)

        expected_dict = {
            "live_diff_sequence": 20,
            "sample_id": value_1 - 10
        }

        result = _decode_dict(encoded, field_definitions, field_ids)

        self.assertDictEqual(result, expected_dict)

    def test_subfield_decoded(self):
        """
        Tests a subfield is decoded successfully
        """

        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1]
        }

        data = {
            #"live_diff_sequence": 123,  # This was not encoded so should not be decoded
            "sample_id": 50,
            "subfields": {
                "subfield": 42
            }
        }

        encoded_value = struct.pack("B", data["sample_id"])

        # The subfield
        encoded_value = encoded_value + struct.pack("!LB", 2, data["subfields"]["subfield"])

        print("Encoded value: " + toHexString(encoded_value))

        result = _decode_dict(encoded_value, field_definitions, field_ids)

        self.assertDictEqual(result, data)

    def test_subfields_decoded(self):
        """
        Tests multiple subfields are decoded successfully
        """
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1, 2, 3, 4]
        }

        data = {
            #"live_diff_sequence": 123,  # This was not encoded so should not be decoded
            "sample_id": 50,
            "subfields": {
                "a": 42,
                "b": 1967347807,
                "c": 5193,
                "d": None
            }
        }

        encoded_value = struct.pack("B", data["sample_id"])

        # subfield
        encoded_value += struct.pack(
            "!LBlHh",
            0x1E,
            data["subfields"]["a"],
            data["subfields"]["b"],
            data["subfields"]["c"],
            _INT_16_NULL,
        )

        print("Encoded value: " + toHexString(encoded_value))

        result = _decode_dict(encoded_value, field_definitions, field_ids)

        self.assertDictEqual(result, data)

    def test_multiple_subfield_sets_decoded(self):
        """
        Tests multiple sets of subfields are decoded successfully
        """
        subfields_a_definition = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        subfields_b_definition = [
            (0, None, None, None, None, None),
            (1, "e", _U_INT_8, None, None, None),
            (2, "f", _INT_32, None, None, None),
            (3, "g", _U_INT_16, None, None, None),
            (4, "h", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields-a", _SUB_FIELDS, subfields_a_definition, None, None),
            (3, "scalar", _INT_8, None, None, None),
            (4, "subfields-b", _SUB_FIELDS, subfields_b_definition, None, None)
        ]

        field_ids = [
            1,
            2,
            3,
            4
        ]

        subfield_ids = {
            "subfields-a": [1, 2, 3, 4],
            "subfields-b": [1, 3, 4]
        }

        data = {
            #"live_diff_sequence": 123,  # This was not encoded so should not be decoded
            "sample_id": 50,
            "subfields-a": {
                "a": 42,
                "b": 1967347807,
                "c": 5193,
                "d": None
            },
            "scalar": 93,
            "subfields-b": {
                "e": 91,
                # "f": 3194324123,  # This one wasn't encoded so shouldn't be decoded
                "g": 1234,
                "h": None
            }
        }

        # first scalar
        encoded_value = struct.pack("B", data["sample_id"])

        # first subfields
        encoded_value += struct.pack(
            "!LBlHh",
            0x1E,
            data["subfields-a"]["a"],
            data["subfields-a"]["b"],
            data["subfields-a"]["c"],
            _INT_16_NULL,
        )

        # second scalar
        encoded_value += struct.pack("B", data["scalar"])

        # second subfields
        encoded_value += struct.pack(
            "!LBHh",
            0x1A,
            data["subfields-b"]["e"],
            data["subfields-b"]["g"],
            _INT_16_NULL,
        )

        print("Encoded value: " + toHexString(encoded_value))

        result = _decode_dict(encoded_value, field_definitions, field_ids)

        self.assertDictEqual(result, data)


class EncodeSizeTests(unittest.TestCase):
    def test_calculate_encoded_size_single_field(self):
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        field_ids = [
            1
        ]

        result = _calculate_encoded_size(field_definitions, field_ids, None)

        self.assertEqual(result, 1)

    def test_calculate_encoded_size_two_fields(self):
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        field_ids = [
            0,
            1
        ]

        result = _calculate_encoded_size(field_definitions, field_ids, None)

        self.assertEqual(result, 2)

    def test_calculate_encoded_size_with_one_subfield_set(self):
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
            2
        ]

        subfield_ids = {
            "subfields": [1]
        }

        result = _calculate_encoded_size(field_definitions, field_ids, subfield_ids)

        # six bytes:
        #   1 byte  for sample_id
        #   4 bytes for subfields header
        #   1 byte  fpr subfield
        self.assertEqual(result, 6)

    def test_calculate_encoded_size_subfield_set_excluded(self):
        """
        Tests that the encoded size only includes the subfield set header if the subfield set
        is included in the list of fields to calculate the size for
        """
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        field_ids = [
            1,
        ]

        subfield_ids = {
            "subfields": [1]
        }

        result = _calculate_encoded_size(field_definitions, field_ids, subfield_ids)

        self.assertEqual(result, 1)

    def test_calculate_encoded_size_with_two_subfield_sets(self):
        subfields_a_definition = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        subfields_b_definition = [
            (0, None, None, None, None, None),
            (1, "e", _U_INT_8, None, None, None),
            (2, "f", _INT_32, None, None, None),
            (3, "g", _U_INT_16, None, None, None),
            (4, "h", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields-a", _SUB_FIELDS, subfields_a_definition, None, None),
            (3, "scalar", _INT_8, None, None, None),
            (4, "subfields-b", _SUB_FIELDS, subfields_b_definition, None, None)
        ]

        field_ids = [
            1,
            2,
            3,
            4
        ]

        subfield_ids = {
            "subfields-a": [1, 2, 3, 4],
            "subfields-b": [1, 3, 4]
        }

        result = _calculate_encoded_size(field_definitions, field_ids, subfield_ids)

        self.assertEqual(result, 24)


class FieldIDListTests(unittest.TestCase):
    # LIVE = {
    #     "live_diff_sequence": 397,              # 0
    #     "sample_diff_timestamp": 1442136671,    # 1  Common with sample
    #     "indoor_humidity": 23,                  # 2  C
    #     "indoor_temperature": 5.4,              # 3  C
    #     "temperature": 9.3,                     # 4  C
    #     "humidity": 83,                         # 5  C
    #     "pressure": 913.4,                      # 6  C
    #     "average_wind_speed": 34.1,             # 7  C
    #     "gust_wind_speed": 98.5,                # 8  C
    #     "wind_direction": 256,                  # 9  C
    #     "bar_trend": 6,                         # 10
    #     "rain_rate": 123,                       # 11
    #     "storm_rain": 985,                      # 12
    #     "current_storm_start_date": 7981,       # 13    2015-09-13
    #     "transmitter_battery": 2,               # 14
    #     "console_battery_voltage": 5.9,         # 15
    #     "forecast_icon": 7,                     # 16
    #     "forecast_rule_id": 10,                 # 17
    #     "uv_index": 9,                          # 18
    #     "solar_radiation": 1043,                # 19
    #     "extra_fields": {                       # 31    C (all fields)
    #         "leaf_wetness_1": 12,       # 1
    #         "leaf_wetness_2": 8,        # 2
    #         "leaf_temperature_1": 15,   # 3
    #         "leaf_temperature_2": 19,   # 4
    #         "soil_moisture_1": 1,       # 5
    #         "soil_moisture_2": 2,       # 6
    #         "soil_moisture_3": 3,       # 7
    #         "soil_moisture_4": 4,       # 8
    #         "soil_temperature_1": 5,    # 9
    #         "soil_temperature_2": 6,    # 10
    #         "soil_temperature_3": 7,    # 11
    #         "soil_temperature_4": 8,    # 12
    #         "extra_temperature_1": 9,   # 13
    #         "extra_temperature_2": 10,  # 14
    #         "extra_temperature_3": 11,  # 15
    #         "extra_humidity_1": 12,     # 16
    #         "extra_humidity_2": 13      # 17
    #     }
    #
    # }
    #
    # SAMPLE = {
    #     "sample_diff_timestamp": 1442136671,  # Common with live
    #     "indoor_humidity": 23,                # C
    #     "indoor_temperature": 5.4,            # C
    #     "temperature": 9.3,                   # C
    #     "humidity": 83,                       # C
    #     "pressure": 913.4,                    # C
    #     "average_wind_speed": 34.1,           # C
    #     "gust_wind_speed": 98.5,              # C
    #     "wind_direction": 256,                # C
    #     "rainfall": 19.3,
    #     "record_time": 1234,
    #     "record_date": 4567,
    #     "high_temperature": 19.34,
    #     "low_temperature": 8.3,
    #     "high_rain_rate": 89.3,
    #     "solar_radiation": 103,
    #     "wind_sample_count": 16,
    #     "gust_wind_direction": 45,
    #     "average_uv_index": 11.3,
    #     "evapotranspiration": 18456874,
    #     "high_solar_radiation": 1154,
    #     "high_uv_index": 12,
    #     "forecast_rule_id": 7,
    #     "extra_fields": {                        # C (all fields)
    #         "leaf_wetness_1": 12,
    #         "leaf_wetness_2": 8,
    #         "leaf_temperature_1": 15,
    #         "leaf_temperature_2": 19,
    #         "soil_moisture_1": 1,
    #         "soil_moisture_2": 2,
    #         "soil_moisture_3": 3,
    #         "soil_moisture_4": 4,
    #         "soil_temperature_1": 5,
    #         "soil_temperature_2": 6,
    #         "soil_temperature_3": 7,
    #         "soil_temperature_4": 8,
    #         "extra_temperature_1": 9,
    #         "extra_temperature_2": 10,
    #         "extra_temperature_3": 11,
    #         "extra_humidity_1": 12,
    #         "extra_humidity_2": 13
    #     }
    # }

    def test_ignored_fields_are_ignored(self):
        """
        Checks that ignored fields are never included in the output even if they differ
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 123,
            "sample_id": 5
        }

        record_b = {
            "live_diff_sequence": 20,
            "sample_id": 5
        }

        expected_result = []
        expected_subfields_result = dict()

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_single_field_difference(self):
        """
        Tests that a single field difference is picked up
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 5,
            "sample_id": 5
        }

        record_b = {
            "live_diff_sequence": 5,
            "sample_id": 10
        }

        expected_result = [1]
        expected_subfields_result = dict()

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_no_field_difference(self):
        """
        Tests no field differences are recognised
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 123,
            "sample_id": 5
        }

        record_b = {
            "live_diff_sequence": 123,
            "sample_id": 5
        }

        expected_result = []
        expected_subfields_result = dict()

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_subfield_differences_only_are_detected(self):
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 123,
            "sample_id": 5,
            "subfields": {
                "subfield": 5
            }
        }

        record_b = {
            "live_diff_sequence": 123,
            "sample_id": 5,
            "subfields": {
                "subfield": 10
            }
        }

        expected_result = [2]
        expected_subfields_result = {
            "subfields": [1]
        }

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_no_subfield_differences_are_detected(self):
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 123,
            "sample_id": 6,
            "subfields": {
                "subfield": 5
            }
        }

        record_b = {
            "live_diff_sequence": 123,
            "sample_id": 5,
            "subfields": {
                "subfield": 5
            }
        }

        expected_result = [1]
        expected_subfields_result = {
            "subfields": []
        }

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_subfield_and_field_differences_are_detected(self):
        subfield_definitions = [
            (0, None, None, None, None, None),
            (1, "subfield", _U_INT_8, None, None, None)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields", _SUB_FIELDS, subfield_definitions, None, None)
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 123,
            "sample_id": 6,
            "subfields": {
                "subfield": 5
            }
        }

        record_b = {
            "live_diff_sequence": 123,
            "sample_id": 5,
            "subfields": {
                "subfield": 10
            }
        }

        expected_result = [1, 2]
        expected_subfields_result = {
            "subfields": [1]
        }

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_difference_in_multiple_subfield_sets_are_detected(self):
        subfields_a_definition = [
            (0, None, None, None, None, None),
            (1, "a", _U_INT_8, None, None, None),
            (2, "b", _INT_32, None, None, None),
            (3, "c", _U_INT_16, None, None, None),
            (4, "d", _INT_16, None, None, _INT_16_NULL)
        ]

        subfields_b_definition = [
            (0, None, None, None, None, None),
            (1, "e", _U_INT_8, None, None, None),
            (2, "f", _INT_32, None, None, None),
            (3, "g", _U_INT_16, None, None, None),
            (4, "h", _INT_16, None, None, _INT_16_NULL)
        ]

        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "subfields-a", _SUB_FIELDS, subfields_a_definition, None, None),
            (3, "scalar", _INT_8, None, None, None),
            (4, "subfields-b", _SUB_FIELDS, subfields_b_definition, None, None)
        ]

        ignore_fields = [
            0
        ]

        record_a = {
            "live_diff_sequence": 5,
            "sample_id": 5,
            "subfields-a": {
                "a": 1,
                "b": 2,
                "c": 3,
                "d": 4
            },
            "scalar": 42,
            "subfields-b": {
                "e": 5,
                "f": 6,
                "g": 7,
                "h": 8
            }
        }

        record_b = {
            "live_diff_sequence": 5,
            "sample_id": 9,
            "subfields-a": {
                "a": 1,
                "b": 6,
                "c": 3,
                "d": None
            },
            "scalar": 42,
            "subfields-b": {
                "e": 5,
                "f": 6,
                "g": 7,
                "h": 20
            }
        }

        expected_result = [1, 2, 4]
        expected_subfields_result = {
            "subfields-a": [2, 4],
            "subfields-b": [4]
        }

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_missing_fields_are_ignored(self):
        """
        Tests that any fields missing from one of the two records are ignored
        """
        field_definitions = [
            (0, "live_diff_sequence", _U_INT_8, None, None, None),
            (1, "sample_id", _U_INT_8, None, None, None),
            (2, "missing-a", _U_INT_8, None, None, None),
            (3, "missing-b", _U_INT_8, None, None, None),
        ]

        ignore_fields = [

        ]

        record_a = {
            "live_diff_sequence": 5,
            "sample_id": 5,
            "missing-b": 10
        }

        record_b = {
            "live_diff_sequence": 5,
            "sample_id": 10,
            "missing-a": 15
        }

        expected_result = [1]
        expected_subfields_result = dict()

        result, subfields_result = _build_field_id_list(
            record_a, record_b, field_definitions, ignore_fields)

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_build_field_id_for_live_against_sample_no_differences(self):
        live = copy.deepcopy(DAVIS_LIVE)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        # These live fields aren't in a sample so will always be included when encoding against
        # a sample.
        expected_result = [12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28]
        expected_subfields_result = {
            "extra_fields": []
        }

        result, subfields_result = build_field_id_list_for_live_against_sample(sample, live, "DAVIS")

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_build_field_id_for_live_against_sample_one_subfield_difference(self):
        live = copy.deepcopy(DAVIS_LIVE)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        live["extra_fields"]["leaf_wetness_1"] = 10

        # These live fields aren't in a sample so will always be included when encoding against
        # a sample.
        expected_result = [12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 31]
        expected_subfields_result = {
            "extra_fields": [1]
        }

        result, subfields_result = build_field_id_list_for_live_against_sample(sample, live, "DAVIS")

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_build_field_id_for_live_against_sample_one_subfield_difference_and_one_regular(self):
        live = copy.deepcopy(DAVIS_LIVE)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        live["temperature"] = 7
        live["extra_fields"]["leaf_wetness_1"] = 10

        # These live fields aren't in a sample so will always be included when encoding against
        # a sample.
        expected_result = [4, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 31]
        expected_subfields_result = {
            "extra_fields": [1]
        }

        result, subfields_result = build_field_id_list_for_live_against_sample(sample, live, "DAVIS")

        self.assertListEqual(result, expected_result)
        self.assertDictEqual(subfields_result, expected_subfields_result)

    def test_sample_with_some_missing_subfields_fields(self):
        base = copy.deepcopy(DAVIS_SAMPLE)
        samp = copy.deepcopy(DAVIS_SAMPLE)

        samp["extra_fields"].pop("leaf_wetness_1", None)

        result, subfields_result = _build_field_id_list(
            samp, base, get_sample_field_definitions("DAVIS"), [])

        # List should be empty as nothing needs to be transmitted
        expected = []

        self.assertListEqual(sorted(result), sorted(expected))




class LiveDataFieldOptionsTests(unittest.TestCase):
    """
    Tests for the get_live_data_field_options function

    The options returned by this function consist of four tuples containing:
        0 - True - difference is against another live record
            False - difference is against another sample
            None - No difference (sending either no fields or all fields)
        1 - List of fields to send
        2 - Size of encoded fields
        3 - Saving vs sending all fields
        4 - List of subfields to send

     A returned option will be None if that option is not available for some reason:
        * Diff-against-live is only available if there is a previous live record to difference against
           (if the supplied previous_live_record is not None)
        * Diff-against-sample is only available if there is a previous sample to difference against
           (if the supplied previous_sample_record is not None)
        * Skip is only available if the live and previous live records are identical
        * The No-diff option is always available.
    """

    @staticmethod
    def _get_current_and_prev():
        prev_live = copy.deepcopy(DAVIS_LIVE)
        live = copy.deepcopy(prev_live)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        # These fields normally indicate the data the record is encoded against. As these are all
        # full records that we're pretending are straight from the database (or message broker)
        # these keys should not be present.
        live.pop("live_diff_sequence", None)
        prev_live.pop("live_diff_sequence", None)
        live.pop("sample_diff_timestamp", None)
        prev_live.pop("sample_diff_timestamp", None)
        sample.pop("sample_diff_timestamp", None)

        return live, prev_live, sample

    @staticmethod
    def _live_field_id_for_name(field_name):
        for f in _davis_live_fields:
            if f[1] == field_name:
                return f[0]
        return None

    def test_live_option_not_given_for_no_previous_live(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        None,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        # Because no previous live record is available to compute a difference against that option should
        # not be provided.
        self.assertIsNone(live_diff_option)

    def test_sample_option_not_given_for_no_previous_sample(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        None,
                                        DAVIS_HW_TYPE)

        # Because no previous sample is available to compute a difference against that option should
        # not be provided.
        self.assertIsNone(sample_diff_option)

    def test_skip_option_not_given_for_non_identical_updates(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["temperature"] = 10

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        # Because the two live records differ (temperature field is not identical) the skip option should not
        # be provided as at least the temperature field must be transmitted somehow.
        self.assertIsNone(skip_option)

    def test_skip_option_given_for_identical_updates(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        # Because the live and previous live records are identical skip should be available
        self.assertIsNotNone(skip_option)

    def test_skip_option_has_no_fields_and_correct_sizes(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertListEqual(skip_option[1], [], "Skip option should specify no fields to send")
        self.assertEqual(skip_option[2], 0, "Skip option field size should be zero (as we're sending no fields)")
        self.assertEqual(skip_option[3], all_fields_size, "Data savings should equal total size of all fields")

    def test_no_diff_option_includes_all_fields_and_correct_sizes(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertListEqual(no_diff_option[1], all_fields, "No diff option should specify all fields need to be sent")
        self.assertEqual(no_diff_option[2], all_fields_size,
                         "No diff option field size should be the size of all fields")
        self.assertEqual(no_diff_option[3], 0, "No diff option should not save any data")
        self.assertDictEqual(no_diff_option[4], all_subfields,
                             "No diff option should specify all subfields fields need to be sent")

    def test_differing_field_appears_in_live_diff_option(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["temperature"] = -3

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertListEqual(
            sorted(live_diff_option[1]),
            sorted([
                self._live_field_id_for_name("live_diff_sequence"),
                self._live_field_id_for_name("temperature")
            ]),
            "Only temperature and live_diff_sequence should be sent in live diff option")
        self.assertEqual(live_diff_option[2], 4,
                         "Live diff size should be 4 (2 bytes each for temperature and live sequence)")
        self.assertEqual(live_diff_option[3], all_fields_size-4,
                         "Savings should be all fields minus four bytes for live diff option")
        self.assertDictEqual(live_diff_option[4], {'extra_fields': []},
                             "Live diff option should send no subfields as none have changed")

    def test_differing_field_appears_in_sample_diff_option(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["temperature"] = -3

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertListEqual(
            sorted(sample_diff_option[1]),
            sorted([
                self._live_field_id_for_name("temperature"),

                # So we know what to diff against
                self._live_field_id_for_name("sample_diff_timestamp"),

                # Plus all these because they're not present in the sample record we're diffing against:
                self._live_field_id_for_name("bar_trend"),
                self._live_field_id_for_name("rain_rate"),
                self._live_field_id_for_name("storm_rain"),
                self._live_field_id_for_name("current_storm_start_date"),
                self._live_field_id_for_name("transmitter_battery"),
                self._live_field_id_for_name("console_battery_voltage"),
                self._live_field_id_for_name("forecast_icon"),
                self._live_field_id_for_name("uv_index"),
                self._live_field_id_for_name("average_wind_speed_2m"),
                self._live_field_id_for_name("average_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_direction_10m"),
                self._live_field_id_for_name("heat_index"),
                self._live_field_id_for_name("thsw_index"),
                self._live_field_id_for_name("altimeter_setting"),

                # And these because their IDs don't match (one day the encoder should be made smarter
                self._live_field_id_for_name("forecast_rule_id"),
                self._live_field_id_for_name("solar_radiation"),
            ]),
            "Only temperature and sample_diff_timestamp should be sent in sample diff option")
        self.assertEqual(
            sample_diff_option[2],
            35,
            "Encoded size should be 35 (2 for temperature, 4 for sample diff timestamp, 22 for live-unique fields)")
        self.assertEqual(sample_diff_option[3], all_fields_size-35,
                         "Savings should be all fields minus 35 bytes for sample diff option")
        self.assertDictEqual(sample_diff_option[4], {'extra_fields': []},
                             "Sample diff option should send no subfields as none have changed")

    def test_differing_subfield_appears_in_live_diff_option(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["extra_fields"]["soil_moisture_1"] = 5

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        print(repr(live_diff_option[1]))

        self.assertListEqual(
            sorted(live_diff_option[1]),
            sorted([
                self._live_field_id_for_name("live_diff_sequence"),
                self._live_field_id_for_name("extra_fields")
            ]),
            "Only extra_fields should be sent in live diff option"
        )
        self.assertEqual(live_diff_option[2], 7,
                         "Difference should be 7 bytes (2 bytes for the live sequence, 4 bytes for subfield header, "
                         "1 byte for soil moisture subfield)")
        self.assertEqual(live_diff_option[3], all_fields_size-7,
                         "Savings should be all fields minus five bytes for live diff option")
        self.assertDictEqual(live_diff_option[4], {"extra_fields": [5]}, # 5 = soil_moisture_1
                             "Only soil_moisture_1 subfield should be sent")

    def test_differing_subfield_appears_in_sample_diff_option(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["extra_fields"]["soil_moisture_1"] = 5

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertListEqual(
            sorted(sample_diff_option[1]),
            sorted([
                self._live_field_id_for_name("extra_fields"),
                self._live_field_id_for_name("sample_diff_timestamp"),

                # And these fields are all unique to live records so must always be transmitted when encoding
                # a live record against a sample.
                self._live_field_id_for_name("bar_trend"),
                self._live_field_id_for_name("rain_rate"),
                self._live_field_id_for_name("storm_rain"),
                self._live_field_id_for_name("current_storm_start_date"),
                self._live_field_id_for_name("transmitter_battery"),
                self._live_field_id_for_name("console_battery_voltage"),
                self._live_field_id_for_name("forecast_icon"),
                self._live_field_id_for_name("uv_index"),
                self._live_field_id_for_name("average_wind_speed_2m"),
                self._live_field_id_for_name("average_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_direction_10m"),
                self._live_field_id_for_name("heat_index"),
                self._live_field_id_for_name("thsw_index"),
                self._live_field_id_for_name("altimeter_setting"),

                # And these because their IDs don't match (one day the encoder should be made smarter
                self._live_field_id_for_name("forecast_rule_id"),
                self._live_field_id_for_name("solar_radiation"),
            ]),
            "Only extra_fields and sample_diff_timestamp should be sent in sample diff option"
        )
        self.assertEqual(
            sample_diff_option[2],
            38,
            "Difference should be 38 bytes (4 bytes for subfield header, 1 byte for soil moisture subfield, "
            "4 bytes for sample timestamp, 29 bytes for live-unique fields)"
        )
        self.assertEqual(sample_diff_option[3], all_fields_size-38,
                         "Savings should be all fields minus 24 bytes for live diff option")
        self.assertDictEqual(sample_diff_option[4], {"extra_fields": [5]},  # 5 = soil_moisture_1
                             "Only soil_moisture_1 subfield should be sent")

    def test_live_unique_fileds_included_in_sample_option_even_when_unchanged_option(self):
        live_record, previous_live_record, previous_sample_record = self._get_current_and_prev()

        live_record["temperature"] = 42

        no_diff_option, live_diff_option, sample_diff_option, skip_option = \
            get_live_data_field_options(live_record,
                                        previous_live_record,
                                        previous_sample_record,
                                        DAVIS_HW_TYPE)

        self.assertListEqual(
            sorted(sample_diff_option[1]),
            sorted([
                # This is the field thats changed
                self._live_field_id_for_name("temperature"),
                self._live_field_id_for_name("sample_diff_timestamp"),
                # And these fields are all unique to live records so must always be transmitted when encoding
                # a live record against a sample.
                self._live_field_id_for_name("bar_trend"),
                self._live_field_id_for_name("rain_rate"),
                self._live_field_id_for_name("storm_rain"),
                self._live_field_id_for_name("current_storm_start_date"),
                self._live_field_id_for_name("transmitter_battery"),
                self._live_field_id_for_name("console_battery_voltage"),
                self._live_field_id_for_name("forecast_icon"),
                self._live_field_id_for_name("uv_index"),

                self._live_field_id_for_name("average_wind_speed_2m"),
                self._live_field_id_for_name("average_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_direction_10m"),
                self._live_field_id_for_name("heat_index"),
                self._live_field_id_for_name("thsw_index"),
                self._live_field_id_for_name("altimeter_setting"),

                # And these because their IDs don't match (one day the encoder should be made smarter
                self._live_field_id_for_name("forecast_rule_id"),
                self._live_field_id_for_name("solar_radiation"),
            ]),
            "All fields unique to live records must be transmitted when encoding against a sample record.")


class LiveDataEncodeTests(unittest.TestCase):
    """
    Tests the encode_live_record function. This function should make a call to
    get_live_data_field_options to get the various encoding options, sort the options by output
    size and then go with the smallest.

    Outputs are a tuple consisting of:
        * encoded data (binary)
        * List of field IDs encoded
        * Tuple of:
            * Uncompressed size
            * Saving
            * compression option taken (one of "none", "live-diff", "sample-diff", "skip")
    """

    @staticmethod
    def _get_current_and_prev():
        prev_live = copy.deepcopy(DAVIS_LIVE)

        # Because we're feeding this into the encoder we need real dates and timestamps
        prev_live["current_storm_start_date"] = datetime.date(year=2015, month=9, day=13)
        prev_live["sample_diff_timestamp"] = datetime.datetime(year=2015, month=9, day=13, hour=1, minute=15)

        live = copy.deepcopy(prev_live)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        live["current_storm_start_date"] = prev_live["current_storm_start_date"]
        live["sample_diff_timestamp"] = prev_live["sample_diff_timestamp"]

        live["live_diff_sequence"] = prev_live["live_diff_sequence"] + 1
        sample["time_stamp"] = live["sample_diff_timestamp"]

        return live, prev_live, sample

    @staticmethod
    def _live_field_id_for_name(field_name):
        for f in _davis_live_fields:
            if f[1] == field_name:
                return f[0]
        return None

    def test_no_compression_gives_no_diff_option(self):
        live, prev_live, sample = self._get_current_and_prev()

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, False)

        self.assertEqual(encode_results[2], "none",
                         "Turning off compression should give no compression")

    def test_skip_is_chosen_when_no_differences(self):
        live, prev_live, sample = self._get_current_and_prev()

        # With no differences the skip option should be the smallest. This option transmits
        # nothing so it will always be the smallest.

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, True)

        self.assertEqual(encode_results[2], "skip",
                         "Skip is the smallest option when there are no differences")

    def test_skip_option_works(self):
        live, prev_live, sample = self._get_current_and_prev()

        # With no differences the skip option should be the smallest. This option transmits
        # nothing so it will always be the smallest.

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, True)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertIsNone(encoded, "encoded value should be None as there is nothing to encode")
        self.assertListEqual(field_ids, [],
                             "Field IDs list should be empty as nothing is being encoded")
        self.assertEqual(encode_results[0], all_fields_size,
                         "Uncompressed size is all fields size")
        self.assertEqual(encode_results[1], all_fields_size,
                         "Saving is the size is all fields")
        self.assertEqual(encode_results[2], "skip",
                         "Skip is the smallest option when there are no differences")

    def test_live_diff_option_works(self):
        live, prev_live, sample = self._get_current_and_prev()

        live_seq = live["live_diff_sequence"]
        new_temperature = 42
        live["temperature"] = new_temperature

        # With a difference of only one shared field live-diff should be the smallest as
        # sample-diff must always transmit 15 bytes of live-unique fields plus an extra two bytes
        # for its larger record ID field.

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, True)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        expected_encoding = struct.pack("!hh", live_seq, _float_encode_2dp(new_temperature))

        self.assertEqual(encoded,
                         expected_encoding,
                         "encoded value should consist of only live sequence and temperature")
        self.assertListEqual(sorted(field_ids), sorted([0, 4]),
                             "Field IDs list should have only live sequence and temperature")
        self.assertEqual(encode_results[0], all_fields_size,
                         "Uncompressed size is all fields size")
        self.assertEqual(encode_results[1], all_fields_size-4,
                         "Saving is the size is all fields minus four")
        self.assertEqual(encode_results[2], "live-diff",
                         "Live-diff is the smallest option when only one field differs")

    def test_sample_diff_option_works(self):
        live, prev_live, sample = self._get_current_and_prev()

        # We wan't a sample-diff! In practice this is pretty unlikely to occur in the wild because
        # sample-diff has a 17 byte penalty over live-diff when all matching fields in the current
        # and previous live records are identical to the previous sample.
        #
        # So we need to change at least 18 bytes of fields between current and previous lives
        # while not changing any fields between the current live and the previous sample!

        # These fields are always transmitted in live-diff so that's an easy 15 bytes of difference
        # between the two live records
        live["bar_trend"] = 1
        live["rain_rate"] = 1
        live["storm_rain"] = 1
        live["current_storm_start_date"] = datetime.date(year=2015, month=1, day=1)
        live["transmitter_battery"] = 1
        live["console_battery_voltage"] = 1
        live["forecast_icon"] = 1
        live["uv_index"] = 1
        live["forecast_rule_id"] = 1
        live["solar_radiation"] = 1

        # Plus these too!
        live["average_wind_speed_2m"] = 1
        live["average_wind_speed_10m"] = 1
        live["gust_wind_speed_10m"] = 1
        live["gust_wind_direction_10m"] = 1
        live["heat_index"] = 1
        live["thsw_index"] = 1
        live["altimeter_setting"] = 1

        # TODO: still necessary?
        # Then we'll change 3 bytes in the previous live record making sample-diff cheaper
        prev_live["temperature"] = 2
        prev_live["humidity"] = 2

        # With a difference of only one shared field live-diff should be the smallest as
        # sample-diff must always transmit 15 bytes of live-unique fields plus an extra two bytes
        # for its larger record ID field.

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, True)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        self.assertEqual(encoded,
                         struct.pack("!LbHHHBHBBBHHHHHhhH",
                                     timestamp_encode(live["sample_diff_timestamp"]),
                                     live["bar_trend"],
                                     _float_encode(live["rain_rate"]),
                                     _float_encode(live["storm_rain"]),
                                     _date_encode(live["current_storm_start_date"]),
                                     live["transmitter_battery"],
                                     _float_encode(live["console_battery_voltage"]),
                                     live["forecast_icon"],
                                     live["forecast_rule_id"],
                                     _float_encode(live["uv_index"]),
                                     live["solar_radiation"],
                                     _float_encode(live["average_wind_speed_2m"]),
                                     _float_encode(live["average_wind_speed_10m"]),
                                     _float_encode(live["gust_wind_speed_10m"]),
                                     live["gust_wind_direction_10m"],
                                     _float_encode_2dp(live["heat_index"]),
                                     _float_encode_2dp(live["thsw_index"]),
                                     _float_encode(live["altimeter_setting"]),
                                     ),
                         "encoded value should consist of only live unique fields and sample TS")
        self.assertListEqual(
            sorted(field_ids),
            sorted([
                self._live_field_id_for_name("sample_diff_timestamp"),
                self._live_field_id_for_name("bar_trend"),
                self._live_field_id_for_name("rain_rate"),
                self._live_field_id_for_name("storm_rain"),
                self._live_field_id_for_name("current_storm_start_date"),
                self._live_field_id_for_name("transmitter_battery"),
                self._live_field_id_for_name("console_battery_voltage"),
                self._live_field_id_for_name("forecast_icon"),
                self._live_field_id_for_name("forecast_rule_id"),
                self._live_field_id_for_name("uv_index"),
                self._live_field_id_for_name("solar_radiation"),
                self._live_field_id_for_name("average_wind_speed_2m"),
                self._live_field_id_for_name("average_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_direction_10m"),
                self._live_field_id_for_name("heat_index"),
                self._live_field_id_for_name("thsw_index"),
                self._live_field_id_for_name("altimeter_setting"),
            ]),
            "Field IDs list should have only live-unique fields and sample-ts")
        self.assertEqual(encode_results[0], all_fields_size,
                         "Uncompressed size is all fields size")

        # 33 is the size of the live unique fields that can't be encoded aginst
        # the sample (the list of fields we're struct.packing above)
        self.assertEqual(encode_results[1], all_fields_size-33,
                         "Saving is the size is all fields minus 33")
        self.assertEqual(encode_results[2], "sample-diff",
                         "Sample-diff is the smallest option in this very contrived scenario")

    def test_no_diff_option_works(self):
        live, prev_live, sample = self._get_current_and_prev()

        # With no previous live or sample the no diff option is all we've got!
        encoded, field_ids, encode_results = encode_live_record(
            live, None, None, DAVIS_HW_TYPE, True)

        all_fields = all_live_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_live_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, True)

        print(repr(all_fields))

        all_live_fields = struct.pack(
            "!BhhBHHHHHbHHHBHBBBHHHHHhhHLbbhhbbbbhhhhhhhbb",
            # ^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ^ ^   ^   ^  ^
            # ||THPP||DBRS||C||USAAGGHTA|| | SM  |   |  |
            # |IT   |GW   |TB|FR WWWWIHL|| LT    |   |  EH
            # IH    AW    SD FI  2TTT ST|LW      ST  ET
            #                       D WI\Subfields Header
            live["indoor_humidity"],
            _float_encode_2dp(live["indoor_temperature"]),
            _float_encode_2dp(live["temperature"]),
            live["humidity"],
            _float_encode(live["pressure"]),
            _float_encode(live["msl_pressure"]),
            _float_encode(live["average_wind_speed"]),
            _float_encode(live["gust_wind_speed"]),
            live["wind_direction"],
            live["bar_trend"],
            _float_encode(live["rain_rate"]),
            _float_encode(live["storm_rain"]),
            _date_encode(live["current_storm_start_date"]),
            live["transmitter_battery"],
            _float_encode(live["console_battery_voltage"]),
            live["forecast_icon"],
            live["forecast_rule_id"],
            _float_encode(live["uv_index"]),
            live["solar_radiation"],
            _float_encode(live["average_wind_speed_2m"]),
            _float_encode(live["average_wind_speed_10m"]),
            _float_encode(live["gust_wind_speed_10m"]),
            live["gust_wind_direction_10m"],
            _float_encode_2dp(live["heat_index"]),
            _float_encode_2dp(live["thsw_index"]),
            _float_encode(live["altimeter_setting"]),
            set_field_ids(range(1, 18)),
            live["extra_fields"]["leaf_wetness_1"],
            live["extra_fields"]["leaf_wetness_2"],
            _float_encode(live["extra_fields"]["leaf_temperature_1"]),
            _float_encode(live["extra_fields"]["leaf_temperature_2"]),
            live["extra_fields"]["soil_moisture_1"],
            live["extra_fields"]["soil_moisture_2"],
            live["extra_fields"]["soil_moisture_3"],
            live["extra_fields"]["soil_moisture_4"],
            _float_encode(live["extra_fields"]["soil_temperature_1"]),
            _float_encode(live["extra_fields"]["soil_temperature_2"]),
            _float_encode(live["extra_fields"]["soil_temperature_3"]),
            _float_encode(live["extra_fields"]["soil_temperature_4"]),
            _float_encode(live["extra_fields"]["extra_temperature_1"]),
            _float_encode(live["extra_fields"]["extra_temperature_2"]),
            _float_encode(live["extra_fields"]["extra_temperature_3"]),
            live["extra_fields"]["extra_humidity_1"],
            live["extra_fields"]["extra_humidity_2"]
        )

        print(toHexString(encoded))
        print(toHexString(all_live_fields))

        self.assertEqual(encoded,
                         all_live_fields,
                         "everything should be encoded")
        self.assertListEqual(
            sorted(field_ids),
            sorted([
                # Common
                self._live_field_id_for_name("indoor_humidity"),
                self._live_field_id_for_name("indoor_temperature"),
                self._live_field_id_for_name("temperature"),
                self._live_field_id_for_name("humidity"),
                self._live_field_id_for_name("pressure"),
                self._live_field_id_for_name("msl_pressure"),
                self._live_field_id_for_name("average_wind_speed"),
                self._live_field_id_for_name("gust_wind_speed"),
                self._live_field_id_for_name("wind_direction"),

                # Davis
                self._live_field_id_for_name("bar_trend"),
                self._live_field_id_for_name("rain_rate"),
                self._live_field_id_for_name("storm_rain"),
                self._live_field_id_for_name("current_storm_start_date"),
                self._live_field_id_for_name("transmitter_battery"),
                self._live_field_id_for_name("console_battery_voltage"),
                self._live_field_id_for_name("forecast_icon"),
                self._live_field_id_for_name("forecast_rule_id"),
                self._live_field_id_for_name("uv_index"),
                self._live_field_id_for_name("solar_radiation"),
                self._live_field_id_for_name("average_wind_speed_2m"),
                self._live_field_id_for_name("average_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_speed_10m"),
                self._live_field_id_for_name("gust_wind_direction_10m"),
                self._live_field_id_for_name("heat_index"),
                self._live_field_id_for_name("thsw_index"),
                self._live_field_id_for_name("altimeter_setting"),
                self._live_field_id_for_name("extra_fields"),
            ]),
            "Field IDs should have everything but live-sequence and sample-ts")
        self.assertEqual(encode_results[0], all_fields_size,
                         "Uncompressed size is all fields size")
        self.assertEqual(encode_results[1], 0,
                         "No savings - we're transmitting everything")
        self.assertEqual(encode_results[2], "none",
                         "No-diff is the only option as there is nothing to diff against")


class SampleFieldOptionsTests(unittest.TestCase):
    """
    Tests for the get_sample_data_field_options function. This behaves much the same as
    get_live_data_field_options but returns fewer options as there are only two ways of sending
    a sample:
        * Send only fields that differ from a previous sample (sample-diff)
        * Send all fields

    The options returned by this function consist of two tuples containing:
        0 - True - difference is against another sample record
            False - No difference (send all fields)
        1 - List of fields to send
        2 - Size of encoded fields
        3 - Saving vs sending all fields
        4 - List of subfields to send

     The sample diff option will be None if no previous sample is available.
    """

    @staticmethod
    def _get_current_and_prev():
        sample = copy.deepcopy(DAVIS_SAMPLE)
        prev_sample = copy.deepcopy(sample)

        # These fields normally indicate the data the record is encoded against. As these are all
        # full records that we're pretending are straight from the database (or message broker)
        # these keys should not be present.
        prev_sample["sample_diff_timestamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=10)
        prev_sample["time_stamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=15)
        sample["sample_diff_timestamp"] = prev_sample["time_stamp"]
        sample["time_stamp"] = datetime.datetime(year=2020,month=5,day=31,hour=15,minute=20)

        return sample, prev_sample

    @staticmethod
    def _field_id_for_name(field_name):
        for f in _davis_sample_fields:
            if f[1] == field_name:
                return f[0]
        return None

    def test_no_sample_diff_option_if_no_previous_sample(self):
        sample, prev_sample = self._get_current_and_prev()

        no_diff_option, sample_diff_option = get_sample_data_field_options(sample,
                                                                           None,
                                                                           DAVIS_HW_TYPE)

        self.assertIsNone(sample_diff_option,
                          "Sample-diff option should not be available when "
                          "there is no previous sample.")

    def test_no_diff_option_has_all_fields(self):
        sample, prev_sample = self._get_current_and_prev()

        no_diff_option, sample_diff_option = get_sample_data_field_options(sample,
                                                                           None,
                                                                           DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]

        self.assertListEqual(
            sorted(all_fields),
            sorted(no_diff_option[1]),
            "No-diff option should have all fields present"
        )

        self.assertDictEqual(
            all_subfields,
            no_diff_option[4],
            "No-diff option should have all sub-fields present"
        )

    def test_no_diff_option_has_correct_sizes(self):
        sample, prev_sample = self._get_current_and_prev()

        no_diff_option, sample_diff_option = get_sample_data_field_options(sample,
                                                                           None,
                                                                           DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, False)

        self.assertEqual(
            all_fields_size,
            no_diff_option[2],
            "No-diff option should have size of all fields"
        )

        self.assertEqual(
            0,
            no_diff_option[3],
            "Data savings should be zero as we're sending all fields"
        )

    def test_sample_diff_option_present_when_previous_sample_supplied(self):
        sample, prev_sample = self._get_current_and_prev()

        no_diff_option, sample_diff_option = get_sample_data_field_options(sample,
                                                                           prev_sample,
                                                                           DAVIS_HW_TYPE)

        self.assertIsNotNone(sample_diff_option,
                           "Sample-diff option should be available if a previous sample is "
                           "supplied")

    def test_sample_diff_has_only_changed_fields_and_appropriate_size(self):
        sample, prev_sample = self._get_current_and_prev()

        sample["temperature"] = 42

        no_diff_option, sample_diff_option = get_sample_data_field_options(sample,
                                                                           prev_sample,
                                                                           DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, False)



        self.assertListEqual(
            sorted([
                self._field_id_for_name("temperature"),
                self._field_id_for_name("sample_diff_timestamp")
            ]),
            sorted(sample_diff_option[1]),
            "Sample-diff option should have only sample diff timestamp and the changed field"
        )

        self.assertEqual(
            6,
            sample_diff_option[2],
            "Total size should be six bytes (2 for temperature field, 4 for sample diff ts field)"
        )

        self.assertEqual(
            all_fields_size - 6,
            sample_diff_option[3],
            "Savings should be all fields minus 6 bytes"
        )


class SampleEncodeTests(unittest.TestCase):
    """
    Tests the encode_sample_record function. This function should make a call to the
    get_sample_data_field_options function to get the up to two encoding options available and then
    encode the sample using the smallest of the two options.

    Outputs are a tuple consiting of:
        * encoded data (binary)
        * List of field IDs encoded
        * Tuple consisting of:
            * uncompressed size
            * data saved by compression (uncompressed size - encoded size)
            * compression strategy used (either "none" or "sample-diff")
    """

    @staticmethod
    def _get_current_and_prev():
        sample = copy.deepcopy(DAVIS_SAMPLE)

        prev_sample = copy.deepcopy(sample)

        # These fields normally indicate the data the record is encoded against. As these are all
        # full records that we're pretending are straight from the database (or message broker)
        # these keys should not be present.
        prev_sample["sample_diff_timestamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=10)
        prev_sample["time_stamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=15)
        sample["sample_diff_timestamp"] = prev_sample["time_stamp"]
        sample["time_stamp"] = datetime.datetime(year=2020,month=5,day=31,hour=15,minute=20)

        return sample, prev_sample

    @staticmethod
    def _field_id_for_name(field_name):
        for f in _davis_sample_fields:
            if f[1] == field_name:
                return f[0]
        return None

    def test_sample_diff_is_the_smallest_option_when_previous_sample_is_supplied(self):
        sample, prev_sample = self._get_current_and_prev()

        encoded, field_ids, option_info = encode_sample_record(sample, prev_sample, DAVIS_HW_TYPE)

        self.assertEqual(option_info[2], "sample-diff",
                         "sample-diff should always be the smallest option if a previous sample is "
                         "available")

    def test_no_diff_option_taken_when_no_previous_sample(self):
        sample, prev_sample = self._get_current_and_prev()

        encoded, field_ids, option_info = encode_sample_record(sample, None, DAVIS_HW_TYPE)

        self.assertEqual(option_info[2], "none",
                         "no-diff should always be the encoding method when no previous sample "
                         "is available")

    def test_no_diff_is_the_smallest_option_when_every_field_differs(self):
        sample, prev_sample = self._get_current_and_prev()

        # If we change every single field then no-diff should be the cheapest encoding method as
        # sample-diff would involve sending everything no-diff does *plus* the
        # sample-diff-timestamp field.

        sample["indoor_humidity"] = 1
        sample["indoor_temperature"] = 1
        sample["temperature"] = 1
        sample["humidity"] = 1
        sample["pressure"] = 1
        sample["average_wind_speed"] = 1
        sample["gust_wind_speed"] = 1
        sample["wind_direction"] = 1
        sample["rainfall"] = 1
        sample["record_time"] = 1235
        sample["record_date"] = 4568
        sample["high_temperature"] = 1
        sample["low_temperature"] = 1
        sample["high_rain_rate"] = 1
        sample["solar_radiation"] = 1
        sample["wind_sample_count"] = 1
        sample["gust_wind_direction"] = 1
        sample["average_uv_index"] = 1
        sample["evapotranspiration"] = 1
        sample["high_solar_radiation"] = 1
        sample["high_uv_index"] = 1
        sample["forecast_rule_id"] = 1

        sample["extra_fields"]["leaf_wetness_1"] = 1
        sample["extra_fields"]["leaf_wetness_2"] = 1
        sample["extra_fields"]["leaf_temperature_1"] = 1
        sample["extra_fields"]["leaf_temperature_2"] = 1
        sample["extra_fields"]["soil_moisture_1"] = 2
        sample["extra_fields"]["soil_moisture_2"] = 1
        sample["extra_fields"]["soil_moisture_3"] = 1
        sample["extra_fields"]["soil_moisture_4"] = 1
        sample["extra_fields"]["soil_temperature_1"] = 1
        sample["extra_fields"]["soil_temperature_2"] = 1
        sample["extra_fields"]["soil_temperature_3"] = 1
        sample["extra_fields"]["soil_temperature_4"] = 1
        sample["extra_fields"]["extra_temperature_1"] = 1
        sample["extra_fields"]["extra_temperature_2"] = 1
        sample["extra_fields"]["extra_temperature_3"] = 1
        sample["extra_fields"]["extra_humidity_1"] = 1
        sample["extra_fields"]["extra_humidity_2"] = 1

        encoded, field_ids, option_info = encode_sample_record(sample, prev_sample, DAVIS_HW_TYPE)

        self.assertEqual(option_info[2], "none",
                         "no-diff should always be the cheapest encoding when every field differs")

    def test_identical_records_send_only_diff_ts(self):
        sample, prev_sample = self._get_current_and_prev()

        encoded, field_ids, option_info = encode_sample_record(sample, prev_sample, DAVIS_HW_TYPE)

        self.assertListEqual(
            sorted(field_ids),
            sorted([
                self._field_id_for_name("sample_diff_timestamp")
            ]),
            "Identical samples should result in transmission of only sample diff timestamp"
        )

    def test_sample_diff_works(self):
        sample, prev_sample = self._get_current_and_prev()

        sample["temperature"] = 42

        encoded, field_ids, option_info = encode_sample_record(sample, prev_sample, DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, False)

        expected_result = struct.pack("!Lh",
                                      timestamp_encode(sample["sample_diff_timestamp"]),
                                      _float_encode_2dp(sample["temperature"]))

        self.assertEqual(encoded, expected_result,
                         "Fields should be encoded correctly.")
        self.assertListEqual(
            sorted(field_ids),
            sorted([
                self._field_id_for_name("sample_diff_timestamp"),
                self._field_id_for_name("temperature")
            ]),
            "Only temperature and sample diff timestamp should be included"
        )

        self.assertEqual(
            option_info[0],
            all_fields_size,
            "Uncompressed value should be the size of all fields"
        )

        self.assertEqual(
            option_info[1],
            all_fields_size - 6,
            "Savings should be all fields size minus six bytes for the two fields being sent"
        )

        self.assertEqual(
            option_info[2],
            "sample-diff",
            "Sample-diff method should be used"
        )

    def test_sample_diff_works_with_only_a_subfield_difference(self):
        sample, prev_sample = self._get_current_and_prev()

        # Only difference between the two samples is the value of a single subfield
        sample["extra_fields"]["leaf_wetness_1"] = 42

        encoded, field_ids, option_info = encode_sample_record(sample, prev_sample, DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, False)

        expected_result = struct.pack("!LLB",
                                      timestamp_encode(sample["sample_diff_timestamp"]),
                                      set_field_ids([1,]),
                                      sample["extra_fields"]["leaf_wetness_1"])

        self.assertEqual(encoded, expected_result,
                         "Fields should be encoded correctly.")
        self.assertListEqual(
            sorted(field_ids),
            sorted([
                self._field_id_for_name("sample_diff_timestamp"),
                self._field_id_for_name("extra_fields")
            ]),
            "Only sample diff timestamp and extra_fields should be included"
        )

        self.assertEqual(
            option_info[0],
            all_fields_size,
            "Uncompressed value should be the size of all fields"
        )

        self.assertEqual(
            option_info[1],
            all_fields_size - 9,
            "Savings should be all fields size minus 9 bytes for the two fields "
            "and one subfield being sent"
        )

        self.assertEqual(
            option_info[2],
            "sample-diff",
            "Sample-diff method should be used"
        )

    def test_no_diff_works(self):
        sample, prev_sample = self._get_current_and_prev()

        # Pass no previous sample to force a dump of all fields
        encoded, field_ids, option_info = encode_sample_record(sample, None, DAVIS_HW_TYPE)

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]
        all_subfields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][1]
        all_fields_size = calculate_encoded_size(all_fields, all_subfields, DAVIS_HW_TYPE, False)

        # target = 34
        expected_result = struct.pack(
            "!BhhBHHHHHHHHhhhHBHBBHBBLbbhhbbbbhhhhhhhbb",
            # ^^^^^^^^^^^^^^^^^^^^^^^^^ ^ ^   ^   ^  ^
            # ||T||M||DR|||L|||GU|||F|LW| SM  ST  ET EH
            # |IT||S|GW ||HT||WSC||HU|  LT
            # IH HPLAW  |RD |SR  |HSR|
            #      P   RT  HRR  EVA Subfield HDR
            sample["indoor_humidity"],
            _float_encode_2dp(sample["indoor_temperature"]),
            _float_encode_2dp(sample["temperature"]),
            sample["humidity"],
            _float_encode(sample["pressure"]),
            _float_encode(sample["msl_pressure"]),
            _float_encode(sample["average_wind_speed"]),
            _float_encode(sample["gust_wind_speed"]),
            sample["wind_direction"],
            _float_encode(sample["rainfall"]),
            sample["record_time"],
            sample["record_date"],
            _float_encode_2dp(sample["high_temperature"]),
            _float_encode_2dp(sample["low_temperature"]),
            _float_encode(sample["high_rain_rate"]),
            sample["solar_radiation"],
            sample["wind_sample_count"],
            _float_encode(sample["gust_wind_direction"]),
            _float_encode(sample["average_uv_index"]),
            _sample_et_encode(sample["evapotranspiration"]),
            sample["high_solar_radiation"],
            _float_encode(sample["high_uv_index"]),
            sample["forecast_rule_id"],
            set_field_ids(range(1, 18)),
            sample["extra_fields"]["leaf_wetness_1"],
            sample["extra_fields"]["leaf_wetness_2"],
            _float_encode(sample["extra_fields"]["leaf_temperature_1"]),
            _float_encode(sample["extra_fields"]["leaf_temperature_2"]),
            sample["extra_fields"]["soil_moisture_1"],
            sample["extra_fields"]["soil_moisture_2"],
            sample["extra_fields"]["soil_moisture_3"],
            sample["extra_fields"]["soil_moisture_4"],
            _float_encode(sample["extra_fields"]["soil_temperature_1"]),
            _float_encode(sample["extra_fields"]["soil_temperature_2"]),
            _float_encode(sample["extra_fields"]["soil_temperature_3"]),
            _float_encode(sample["extra_fields"]["soil_temperature_4"]),
            _float_encode(sample["extra_fields"]["extra_temperature_1"]),
            _float_encode(sample["extra_fields"]["extra_temperature_2"]),
            _float_encode(sample["extra_fields"]["extra_temperature_3"]),
            sample["extra_fields"]["extra_humidity_1"],
            sample["extra_fields"]["extra_humidity_2"]
        )

        self.assertEqual(encoded, expected_result,
                         "Fields should be encoded correctly.")
        self.assertListEqual(
            sorted(field_ids),
            sorted(all_fields),
            "All fields should be encoded"
        )

        self.assertEqual(
            option_info[0],
            all_fields_size,
            "Uncompressed value should be the size of all fields"
        )

        self.assertEqual(
            option_info[1],
            0,
            "There should be no savings"
        )

        self.assertEqual(
            option_info[2],
            "none",
            "no-diff method should be used"
        )


class PatchRecordTests(unittest.TestCase):
    """
    Tests the _patch_record function. This function takes:
        * a record that (potentially) missing one or more fields
        * a base record that can be used to fill in any missing fields
        * A list of all the fields present in the record
        * A list of all fields (these are the fields guaranteed to be present in the base record)
        * A list of all subfield ids (not used?)
        * The definitions for all fields

    It returns a new record that has all values from the supplied record with any gaps filled in
    from the base record.
    """

    @staticmethod
    def _get_current_and_prev():
        sample = copy.deepcopy(DAVIS_SAMPLE)

        prev_sample = copy.deepcopy(sample)

        # These fields normally indicate the data the record is encoded against. As these are all
        # full records that we're pretending are straight from the database (or message broker)
        # these keys should not be present.
        prev_sample["sample_diff_timestamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=10)
        prev_sample["time_stamp"] = datetime.datetime(year=2020, month=5, day=31, hour=15, minute=15)
        sample["sample_diff_timestamp"] = prev_sample["time_stamp"]
        sample["time_stamp"] = datetime.datetime(year=2020,month=5,day=31,hour=15,minute=20)

        return sample, prev_sample

    @staticmethod
    def _field_id_for_name(field_name):
        for f in _davis_sample_fields:
            if f[1] == field_name:
                return f[0]
        return None

    def test_full_record_is_returned_unchanged(self):
        """
        A field with no missing fields shouldn't get patched.
        """
        sample, prev_sample = self._get_current_and_prev()

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]

        patched = _patch_record(sample, prev_sample, all_fields, all_fields,
                                _davis_sample_fields)

        self.assertDictEqual(sample, patched,
                             "Full sample should pass through patching unmodified")

    def test_missing_field_is_patched(self):
        """
        Test a regular missing field is patched in
        """

        original_sample, prev_sample = self._get_current_and_prev()

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]

        sample = copy.deepcopy(original_sample)
        sample_fields = copy.copy(all_fields)

        # Remove the temperature field
        sample.pop("temperature", None)
        sample_fields.remove(self._field_id_for_name("temperature"))

        patched = _patch_record(sample, prev_sample, sample_fields, all_fields,
                                _davis_sample_fields)

        self.assertDictEqual(patched, original_sample,
                             "Temperature field should be patched back in")

    def test_missing_subfield_sets_are_patched(self):
        """
        Test an entirely missing set of subfields are fully patched in
        """
        original_sample, prev_sample = self._get_current_and_prev()

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]

        sample = copy.deepcopy(original_sample)
        sample_fields = copy.copy(all_fields)

        # Remove the temperature field
        sample.pop("extra_fields", None)
        sample_fields.remove(self._field_id_for_name("extra_fields"))

        patched = _patch_record(sample, prev_sample, sample_fields, all_fields,
                                _davis_sample_fields)

        self.assertDictEqual(patched, original_sample,
                             "extra_fields subfield set should be patched back in")

    def test_missing_subfield_fields_are_patched(self):
        """
        Test missing fields within a subfield are patched
        """
        original_sample, prev_sample = self._get_current_and_prev()

        all_fields = all_sample_field_ids[DAVIS_HW_TYPE.upper()][0]

        sample = copy.deepcopy(original_sample)
        sample_fields = copy.copy(all_fields)

        # Remove the temperature field
        sample["extra_fields"].pop("soil_moisture_1", None)
        # NOTE: We don't remove the extra_fields ID from the list of fields in the
        # sample as while one of the extra_fields fields is missing the extra_fields
        # subfield itself is not - its header is present.

        patched = _patch_record(sample, prev_sample, sample_fields, all_fields,
                                _davis_sample_fields)

        self.assertDictEqual(patched, original_sample,
                             "extra_fields subfield set should be patched back in")

class FindSubfieldIdsTests(unittest.TestCase):
    @staticmethod
    def _get_current_and_prev():
        prev_live = copy.deepcopy(DAVIS_LIVE)

        # Because we're feeding this into the encoder we need real dates and timestamps
        prev_live["current_storm_start_date"] = datetime.date(year=2015, month=9, day=13)
        prev_live["sample_diff_timestamp"] = datetime.datetime(year=2015, month=9, day=13, hour=1, minute=15)

        live = copy.deepcopy(prev_live)
        sample = copy.deepcopy(DAVIS_SAMPLE)

        live["current_storm_start_date"] = prev_live["current_storm_start_date"]
        live["sample_diff_timestamp"] = prev_live["sample_diff_timestamp"]

        live["live_diff_sequence"] = prev_live["live_diff_sequence"] + 1
        sample["time_stamp"] = live["sample_diff_timestamp"]

        return live, prev_live, sample

    def test_find_subfields_in_full_live(self):
        live, prev_live, sample = self._get_current_and_prev()

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, False)

        subfields = find_live_subfield_ids(encoded, field_ids, DAVIS_HW_TYPE)

        self.assertTrue("extra_fields" in subfields,
                        "Subfields should be present")

        self.assertListEqual(
            list(range(1, 18)),
            sorted(subfields["extra_fields"]),
            "All subfield IDs should be present"
        )

    def test_find_subfields_in_partial_live(self):
        live, prev_live, sample = self._get_current_and_prev()

        live["extra_fields"]["soil_moisture_1"] = 5
        live["extra_fields"]["soil_temperature_3"] = 10
        live["extra_fields"]["extra_temperature_3"] = 9
        live["temperature"] = 3

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, True)

        subfields = find_live_subfield_ids(encoded, field_ids, DAVIS_HW_TYPE)

        self.assertTrue("extra_fields" in subfields,
                        "Subfields should be present")

        self.assertListEqual(
            [5, 11, 15],
            sorted(subfields["extra_fields"]),
            "All subfield IDs should be present"
        )

    def test_find_subfields_in_truncated_live(self):
        live, prev_live, sample = self._get_current_and_prev()

        encoded, field_ids, encode_results = encode_live_record(
            live, prev_live, sample, DAVIS_HW_TYPE, False)

        subfields = find_live_subfield_ids(encoded[0:len(encoded)//2], field_ids, DAVIS_HW_TYPE)

        self.assertIsNone(subfields,
                         "Result should be None if there was insufficient data to complete search")
#