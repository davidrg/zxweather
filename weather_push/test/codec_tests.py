import struct
import unittest
from zxw_push.common.data_codecs import _encode_dict, _U_INT_16, _U_INT_32, \
    _U_INT_8, _decode_dict

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

# test
