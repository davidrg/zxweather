# coding=utf-8
import struct
import unittest
from datetime import datetime
from zxw_push.common.packets import UDPPacket, StationInfoRequestUDPPacket, \
    StationInfoResponseUDPPacket, LiveDataRecord, SampleDataRecord, \
    WeatherDataUDPPacket, SampleAcknowledgementUDPPacket

__author__ = 'david'


class UDPPacketTests(unittest.TestCase):
    """
    Unit tests for the Packet class (the parent class for all packets)

    """
    def test_packet_round_trip(self):
        input_packet = UDPPacket(20)
        input_packet.sequence = 123
        input_packet.authorisation_code = 12345

        encoded = input_packet.encode()

        output_packet = UDPPacket(0)
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.sequence, input_packet.sequence)
        self.assertEqual(output_packet.authorisation_code,
                         input_packet.authorisation_code)

    def test_large_packet_type_is_rejected(self):
        """
        The packet type field is only 8 bits. So it should accept values no
        higher than 256
        """
        packet = UDPPacket(0)

        # This should be ok
        packet.packet_type = 255

        def _set_val(val):
            packet.packet_type = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 256)

    def test_negative_packet_type_is_rejected(self):
        """
        The packet type field is unsigned so it shouldn't accept negative values
        """
        packet = UDPPacket(0)

        # This should be ok
        packet.packet_type = 0

        def _set_val(val):
            packet.packet_type = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)

    def test_large_sequence_id_is_rejected(self):
        """
        The sequence field is only 16 bits. So it should accept values no
        higher than 65535
        """
        packet = UDPPacket(0)

        # This should be ok
        packet.sequence = 65535

        def _set_val(val):
            packet.sequence = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 65536)

    def test_negative_sequence_id_is_rejected(self):
        """
        The sequence field is unsigned so it shouldn't accept negative values
        """
        packet = UDPPacket(0)

        # This should be ok
        packet.sequence = 0

        def _set_val(val):
            packet.sequence = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)

    def test_large_authorisation_code_is_rejected(self):
        """
        The authorisation code field is only 32 bits. So it should accept
        values no higher than 0xFFFFFFFF
        """
        packet = UDPPacket(0)

        # This should be ok - four bytes total
        packet.authorisation_code = 0xFFFFFFFF  # 4,294,967,295

        def _set_val(val):
            packet.authorisation_code = val

        # This should fail - five bytes total (33 bits set) 8,589,934,591
        self.assertRaises(ValueError, _set_val, 0x01FFFFFFFF)

    def test_negative_authorisation_code_is_rejected(self):
        """
        The authorisation code field is unsigned so it shouldn't accept negative
        values
        """
        packet = UDPPacket(0)

        # This should be ok
        packet.authorisation_code = 0

        def _set_val(val):
            packet.authorisation_code = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)


class StationInfoRequestUDPPacketTests(unittest.TestCase):
    def test_station_info_request_packet_round_trip(self):
        input_packet = StationInfoRequestUDPPacket()
        input_packet.sequence = 123
        input_packet.authorisation_code = 12345

        encoded = input_packet.encode()

        output_packet = StationInfoRequestUDPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.sequence, 123)
        self.assertEqual(output_packet.authorisation_code, 12345)


class StationInfoResponseUDPPacketTests(unittest.TestCase):
    def test_station_info_response_packet_round_trip(self):
        input_packet = StationInfoResponseUDPPacket()
        input_packet.sequence = 123
        input_packet.authorisation_code = 12345

        input_packet.add_station("test1", "davis", 1)
        input_packet.add_station("test2", "fowh1080", 2)
        input_packet.add_station("test3", "generic", 3)

        encoded = input_packet.encode()

        output_packet = StationInfoResponseUDPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.sequence, 123)
        self.assertEqual(output_packet.authorisation_code, 12345)

        self.assertListEqual(output_packet.stations, input_packet.stations)


class WeatherDataUDPPacketTests(unittest.TestCase):
    """
    Tests the WeatherData Packet
    """

    def test_multi_record_round_trip(self):
        """
        Tests that what goes in comes back out.
        """

        hardware_types = {
            1: "DAVIS",
            2: "WH1080",
            3: "GENERIC"
        }

        packet = WeatherDataUDPPacket(sequence=5, authorisation_code=0xABCDEFAB)

        # Add a selection of records
        rec = SampleDataRecord()
        rec.station_id = 1  # Davis
        rec.timestamp = datetime(2015, 03, 07, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 03, 06, 10, 53, 42)
        rec.field_list = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                          29, 31]
        # Given the above field list and hardware type, the field data should
        # be 22 bytes
        rec.field_data = "\xDE\xAD\xBE\xEF 123456789ABCEFAE3"
        self.assertEqual(len(rec.field_data), 22)
        packet.add_record(rec)

        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 12345
        rec.field_list = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                          29, 31]
        # Given the above field list and hardware type, the field data should
        # be 11 bytes
        rec.field_data = "123456 \xDE\xAD\xBE\xEF"
        self.assertEqual(len(rec.field_data), 11)
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 3  # Generic
        rec.timestamp = datetime(2015, 03, 07, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 03, 06, 10, 53, 42)
        rec.field_list = [2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28,
                          30, 32]
        # Given the above field list and hardware type, the field data should
        # be 9 bytes
        rec.field_data = "123 \x1E 456"
        self.assertEqual(len(rec.field_data), 9)
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 1  # Davis
        rec.timestamp = datetime(2015, 03, 07, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 03, 06, 10, 53, 42)
        rec.field_list = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                          17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                          30, 31]
        # Given the above field list and hardware type, the field data should
        # be 44 bytes
        rec.field_data = "\xDE\xAD \xBE\xEF \x1E More \x1E separator \x1E " \
                         "testing 123456789"
        self.assertEqual(len(rec.field_data), 44)
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 2  # WH1080
        rec.timestamp = datetime(2015, 03, 07, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 03, 06, 10, 53, 42)
        rec.field_list = [1, 2, 3, 4, 5]
        # Given the above field list and hardware type, the field data should
        # be 10 bytes
        rec.field_data = """a\x1Eb\x1Ec\x1Ed\x1Ee\x1E"""
        self.assertEqual(len(rec.field_data), 10)
        packet.add_record(rec)

        # This record contains the end-of-record marker early on in the header.
        # We need to make sure this situation is still correctly handled.
        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 0x1E
        rec.field_list = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
                          29, 31]
        # Given the above field list and hardware type, the field data should
        # be 11 bytes
        rec.field_data = "123456 \xDE\xAD\xBE\xEF"
        self.assertEqual(len(rec.field_data), 11)
        packet.add_record(rec)

        encoded = packet.encode()

        out_packet = WeatherDataUDPPacket()
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

        self.assertEqual(packet.sequence, out_packet.sequence)
        self.assertEqual(packet.authorisation_code,
                         out_packet.authorisation_code)

        # Now test all the records survived!
        self.assertEqual(len(packet.records), len(out_packet.records))

        for idx, in_record in enumerate(packet.records):
            out_record = out_packet.records[idx]

            self.assertEqual(in_record.station_id, out_record.station_id)
            self.assertListEqual(in_record.field_list,
                                 out_record.field_list)
            self.assertEqual(in_record.field_data, out_record.field_data)

            if isinstance(in_record, LiveDataRecord):
                self.assertIsInstance(out_record, LiveDataRecord)
                self.assertEqual(in_record.sequence_id, out_record.sequence_id)

            elif isinstance(in_record, SampleDataRecord):
                self.assertIsInstance(out_record, SampleDataRecord)
                self.assertEqual(in_record.timestamp, out_record.timestamp)
                self.assertEqual(in_record.download_timestamp,
                                 out_record.download_timestamp)
            else:
                self.assertTrue(False)

    def test_single_record_round_trip(self):
        """
        Tests that what goes in comes back out.
        """

        hardware_types = {
            1: "DAVIS",
            2: "WH1080",
            3: "GENERIC"
        }

        packet = WeatherDataUDPPacket(sequence=5, authorisation_code=0xABCDEFAB)

        # Add a selection of records
        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 4
        rec.field_list = [4]
        # Given the above field list and hardware type, the field data should
        # be 11 bytes
        rec.field_data = "12"
        self.assertEqual(len(rec.field_data), 2)
        packet.add_record(rec)

        encoded = packet.encode()

        out_packet = WeatherDataUDPPacket()
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

        self.assertEqual(packet.sequence, out_packet.sequence)
        self.assertEqual(packet.authorisation_code,
                         out_packet.authorisation_code)

        # Now test all the records survived!
        self.assertEqual(len(packet.records), len(out_packet.records))

        for idx, in_record in enumerate(packet.records):
            out_record = out_packet.records[idx]

            self.assertEqual(in_record.station_id, out_record.station_id)
            self.assertListEqual(in_record.field_list,
                                 out_record.field_list)
            self.assertEqual(in_record.field_data, out_record.field_data)

            if isinstance(in_record, LiveDataRecord):
                self.assertIsInstance(out_record, LiveDataRecord)
                self.assertEqual(in_record.sequence_id, out_record.sequence_id)

            elif isinstance(in_record, SampleDataRecord):
                self.assertIsInstance(out_record, SampleDataRecord)
                self.assertEqual(in_record.timestamp, out_record.timestamp)
                self.assertEqual(in_record.download_timestamp,
                                 out_record.download_timestamp)
            else:
                self.assertTrue(False)

    def test_single_record_round_trip_with_EOT_Marker_at_end_of_data(self):
        """
        Tests that what goes in comes back out.
        """

        hardware_types = {
            1: "DAVIS",
            2: "WH1080",
            3: "GENERIC"
        }

        packet = WeatherDataUDPPacket(sequence=5, authorisation_code=0xABCDEFAB)

        # Add a selection of records
        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 12
        rec.field_list = [0, 9]
        # Given the above field list and hardware type, the field data should
        # be 11 bytes
        rec.field_data = struct.pack("!HH", 10, 4)
        self.assertEqual(len(rec.field_data), 4)
        packet.add_record(rec)

        encoded = packet.encode()

        out_packet = WeatherDataUDPPacket()
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

        self.assertEqual(packet.sequence, out_packet.sequence)
        self.assertEqual(packet.authorisation_code,
                         out_packet.authorisation_code)

        # Now test all the records survived!
        self.assertEqual(len(packet.records), len(out_packet.records))

        for idx, in_record in enumerate(packet.records):
            out_record = out_packet.records[idx]

            self.assertEqual(in_record.station_id, out_record.station_id)
            self.assertListEqual(in_record.field_list,
                                 out_record.field_list)
            self.assertEqual(in_record.field_data, out_record.field_data)

            if isinstance(in_record, LiveDataRecord):
                self.assertIsInstance(out_record, LiveDataRecord)
                self.assertEqual(in_record.sequence_id, out_record.sequence_id)

            elif isinstance(in_record, SampleDataRecord):
                self.assertIsInstance(out_record, SampleDataRecord)
                self.assertEqual(in_record.timestamp, out_record.timestamp)
                self.assertEqual(in_record.download_timestamp,
                                 out_record.download_timestamp)
            else:
                self.assertTrue(False)


class SampleAcknowledgementUDPPacketTests(unittest.TestCase):
    """
    Tests the Sample acknowledgement packet.
    """
    def test_round_trip(self):
        input_packet = SampleAcknowledgementUDPPacket()
        input_packet.sequence = 123
        input_packet.authorisation_code = 12345
        input_packet.lost_live_records = 250

        input_packet.add_sample_acknowledgement(
            15, datetime(2015, 3, 7, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            20, datetime(2015, 4, 8, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            25, datetime(2015, 5, 9, 10, 53, 42))

        encoded = input_packet.encode()

        output_packet = SampleAcknowledgementUDPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.lost_live_records,
                         input_packet.lost_live_records)

        self.assertListEqual(output_packet.sample_acknowledgements(),
                             input_packet.sample_acknowledgements())

    def test_large_lost_record_count_is_rejected(self):
        """
        The field is only 8 bits. So it should accept values no higher than 256
        """
        packet = SampleAcknowledgementUDPPacket()

        # This should be ok
        packet.lost_live_records = 255

        def _set_val(val):
            packet.lost_live_records = val

        # This should fail
        self.assertRaises(ValueError, _set_val, 256)

    def test_negative_lost_record_count_is_rejected(self):
        """
        The field is unsigned so should accept no values smaller than zero.
        """
        packet = SampleAcknowledgementUDPPacket()

        # This should be ok
        packet.lost_live_records = 0

        def _set_val(val):
            packet.lost_live_records = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)

    def test_large_station_id_is_rejected(self):
        """
        The field is only 8 bits. So it should accept values no higher than 256
        """
        packet = SampleAcknowledgementUDPPacket()

        # This should be ok
        packet.add_sample_acknowledgement(255, datetime.now())

        # This should fail
        self.assertRaises(ValueError, packet.add_sample_acknowledgement, 256,
                          datetime.now())

    def test_negative_station_id_is_rejected(self):
        """
        The field is unsigned so should accept no values smaller than zero.
        """
        packet = SampleAcknowledgementUDPPacket()

        # This should be ok
        packet.add_sample_acknowledgement(0, datetime.now())

        # This should fail
        self.assertRaises(ValueError, packet.add_sample_acknowledgement, -1,
                          datetime.now())