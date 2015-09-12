import struct
import unittest
from zxw_push.common.data_codecs import _encode_dict, _U_INT_16, _U_INT_32, \
    _U_INT_8

__author__ = 'david'

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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

        result = _encode_dict(data, field_definitions, field_ids)

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
                          _encode_dict, *(data, field_definitions, field_ids))

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
                          _encode_dict, *(data, field_definitions, field_ids))
