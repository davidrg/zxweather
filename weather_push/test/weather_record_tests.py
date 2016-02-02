# coding=utf-8
import unittest
from datetime import datetime

from zxw_push.common.packets import WeatherRecord, LiveDataRecord, \
    SampleDataRecord

__author__ = 'david'


class WeatherRecordTests(unittest.TestCase):
    """
    Tests for the weather record class
    """

    def test_round_trip(self):
        rec = WeatherRecord()
        rec.record_type = 42

        encoded = rec.encode()

        out_rec = WeatherRecord()
        out_rec.decode(encoded)

        self.assertEqual(rec.record_type, out_rec.record_type)

    def test_large_record_type_is_rejected(self):
        """
        The record type field is only 8 bits. So it should accept values no
        higher than 256
        """
        packet = WeatherRecord()

        # This should be ok
        packet.record_type = 255

        def _set_val(val):
            packet.record_type = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 256)

    def test_negative_record_type_is_rejected(self):
        """
        The record type field is unsigned so should accept no values smaller
        than zero.
        """
        packet = WeatherRecord()

        # This should be ok
        packet.record_type = 0

        def _set_val(val):
            packet.record_type = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)

    def test_large_station_id_is_rejected(self):
        """
        The station ID field is only 8 bits. So it should accept values no
        higher than 256
        """
        record = LiveDataRecord()

        # This should be ok
        record.station_id = 255

        def _set_val(val):
            record.station_id = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 256)

    def test_negative_station_id_is_rejected(self):
        """
        The station id field is unsigned so it shouldn't accept negative values
        """
        record = LiveDataRecord()

        # This should be ok
        record.station_id = 0

        def _set_val(val):
            record.station_id = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)

    def test_field_list_property_round_trip(self):
        # This property doesn't store the supplied value. The setter transforms
        # it to a 32bit int and the getter transforms it back.
        fields = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                  29, 31]
        record = SampleDataRecord()
        record.field_list = fields

        self.assertListEqual(fields, record.field_list)


class LiveDataRecordTests(unittest.TestCase):
    """
    Tests the live data record
    """

    def test_round_trip(self):
        rec = LiveDataRecord()
        rec.station_id = 123
        rec.sequence_id = 12345
        rec.field_list = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                          29, 31]
        rec.field_data = """This is an automated test of the field data
        property. I'm just putting a chunk of text in there now but normally
        there would be binary stuff instead. So lets have some
        binary \xDE\xAD\xBE\xEF"""

        encoded = rec.encode()

        decoded = LiveDataRecord()
        decoded.decode(encoded)

        self.assertEqual(rec.station_id, decoded.station_id)
        self.assertEqual(rec.sequence_id, decoded.sequence_id)
        self.assertListEqual(rec.field_list, decoded.field_list)
        self.assertEqual(rec.field_data, decoded.field_data)

    def test_encoded_size(self):
        rec = LiveDataRecord()
        rec.station_id = 123
        rec.sequence_id = 12345
        rec.field_list = [1, 2, 3]
        rec.field_data = "Test"

        encoded_size = rec.encoded_size()

        # In bytes:
        expected_size = 1  # Record type
        expected_size += 1  # Station ID
        expected_size += 2  # Sequence ID
        expected_size += 4  # Field bits
        expected_size += 4  # Field data

        self.assertEqual(encoded_size, expected_size)

    def test_large_sequence_id_is_rejected(self):
        """
        The sequence ID field is only 16 bits. So it should accept values no
        higher than 65535
        """
        record = LiveDataRecord()

        # This should be ok
        record.sequence_id = 65535

        def _set_val(val):
            record.sequence_id = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 65536)

    def test_negative_sequence_id_is_rejected(self):
        """
        The sequence id field is unsigned so it shouldn't accept negative values
        """
        record = LiveDataRecord()

        # This should be ok
        record.sequence_id = 0

        def _set_val(val):
            record.sequence_id = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)


class SampleDataRecordTests(unittest.TestCase):
    """
    Tests the sample data record
    """

    def test_round_trip(self):

        rec = SampleDataRecord()
        rec.station_id = 123
        rec.timestamp = datetime(2015, 03, 07, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 03, 06, 10, 53, 42)
        rec.field_list = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                          29, 31]
        rec.field_data = """This is an automated test of the field data
        property. I'm just putting a chunk of text in there now but normally
        there would be binary stuff instead. So lets have some
        binary \xDE\xAD\xBE\xEF"""

        encoded = rec.encode()

        decoded = SampleDataRecord()
        decoded.decode(encoded)

        self.assertEqual(rec.station_id, decoded.station_id)
        self.assertListEqual(rec.field_list, decoded.field_list)
        self.assertEqual(rec.field_data, decoded.field_data)
        self.assertEqual(rec.timestamp, decoded.timestamp)
        self.assertEqual(rec.download_timestamp, decoded.download_timestamp)

    def test_encoded_size(self):
        rec = SampleDataRecord()
        rec.station_id = 123
        rec.field_list = [1, 2, 3]
        rec.field_data = "Test"
        rec.timestamp = datetime.utcnow()
        rec.download_timestamp = datetime.utcnow()

        encoded_size = rec.encoded_size()

        # In bytes:
        expected_size = 1  # Record type
        expected_size += 1  # Station ID
        expected_size += 4  # Timestamp
        expected_size += 4  # Download timestamp
        expected_size += 4  # Field bits
        expected_size += 4  # Field data

        self.assertEqual(encoded_size, expected_size)
