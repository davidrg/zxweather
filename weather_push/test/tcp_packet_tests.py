# coding=utf-8
import copy
import struct
import unittest
from datetime import datetime, date
from random import choice

from zxw_push.common.data_codecs import get_sample_field_definitions, _encode_dict, get_live_field_definitions
from zxw_push.common.packets import SampleDataRecord, WeatherDataTCPPacket, \
    LiveDataRecord, SampleAcknowledgementTCPPacket, AuthenticateTCPPacket
from zxw_push.common.packets.tcp_packets import TcpPacket, StationInfoTCPPacket, \
    ImageTCPPacket, ImageAcknowledgementTCPPacket

__author__ = 'david'

# TODO: checks around calculated lengths, etc


class PacketTests(unittest.TestCase):
    """
    Unit tests for the Packet class (the parent class for all packets)

    """
    def test_packet_round_trip(self):
        input_packet = TcpPacket(20)

        encoded = input_packet.encode()

        output_packet = TcpPacket(0)
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)

    def test_large_packet_type_is_rejected(self):
        """
        The packet type field is only 8 bits. So it should accept values no
        higher than 256
        """
        packet = TcpPacket(0)

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
        packet = TcpPacket(0)

        # This should be ok
        packet.packet_type = 0

        def _set_val(val):
            packet.packet_type = val

        # This should fail
        self.assertRaises(ValueError, _set_val, -1)


class AuthenticatePacketTests(unittest.TestCase):
    def test_round_trip(self):
        input_packet = AuthenticateTCPPacket(12345)

        encoded = input_packet.encode()

        self.assertEqual(len(encoded), 10)

        output_packet = AuthenticateTCPPacket(0)
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.authorisation_code, 12345)

    def test_large_authorisation_code_is_rejected(self):
        """
        The authorisation code field is only 32 bits. So it should accept
        values no higher than 0xFFFFFFFF
        """

        # This should be ok - four bytes total
        AuthenticateTCPPacket(0xFFFFFFFFFFFFFFFF)

        # This should fail - nine bytes total (65 bits set)
        self.assertRaises(ValueError, AuthenticateTCPPacket,
                          0x1FFFFFFFFFFFFFFFF)

    def test_negative_authorisation_code_is_rejected(self):
        """
        The authorisation code field is unsigned so it shouldn't accept negative
        values
        """

        # This should be ok
        TcpPacket(0)

        # This should fail
        self.assertRaises(ValueError, AuthenticateTCPPacket, -1)

    def test_size(self):
        """
        Checks that the packet size returned is correct
        """

        input_packet = AuthenticateTCPPacket(12345)

        encoded = input_packet.encode()

        size = AuthenticateTCPPacket.packet_size(encoded)

        self.assertEqual(len(encoded), size)


class StationInfoResponsePacketTests(unittest.TestCase):
    def test_station_info_response_packet_round_trip(self):
        input_packet = StationInfoTCPPacket()

        input_packet.add_station("test1", "davis", 1)
        input_packet.add_station("test2", "fowh1080", 2)
        input_packet.add_station("test3", "generic", 3)

        input_packet.add_image_source("src1", 4)
        input_packet.add_image_source("src2", 5)

        input_packet.add_image_type("typ1", 6)
        input_packet.add_image_type("typ2", 7)

        encoded = input_packet.encode()

        output_packet = StationInfoTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)

        self.assertListEqual(output_packet.stations, input_packet.stations)
        self.assertListEqual(output_packet.image_sources,
                             input_packet.image_sources)
        self.assertListEqual(output_packet.image_types,
                             input_packet.image_types)

    def test_size(self):
        """
        Checks that the packet size returned is correct
        """

        input_packet = StationInfoTCPPacket()

        input_packet.add_station("test1", "davis", 1)
        input_packet.add_station("test2", "fowh1080", 2)
        input_packet.add_station("test3", "generic", 3)

        input_packet.add_image_source("src1", 4)
        input_packet.add_image_source("src2", 5)

        input_packet.add_image_type("typ1", 6)
        input_packet.add_image_type("typ2", 7)

        encoded = input_packet.encode()

        size = StationInfoTCPPacket.packet_size(encoded)

        self.assertEqual(len(encoded), size)


class WeatherDataPacketTests(unittest.TestCase):
    """
    Tests the WeatherData Packet
    """

    GENERIC_LIVE = {
        "live_diff_sequence": 397,  # 0
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # 1  Common with sample
        "indoor_humidity": 23,  # 2  C
        "indoor_temperature": 5.4,  # 3  C
        "temperature": 9.3,  # 4  C
        "humidity": 83,  # 5  C
        "pressure": 913.4,  # 6  C
        "average_wind_speed": 34.1,  # 7  C
        "gust_wind_speed": 98.5,  # 8  C
        "wind_direction": 256,  # 9  C
    }

    GENERIC_SAMPLE = {
        "time_stamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Comes from the DB. This is encoded separately for transmission.
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Common with live
        "indoor_humidity": 23,  # C
        "indoor_temperature": 5.4,  # C
        "temperature": 9.3,  # C
        "humidity": 83,  # C
        "pressure": 913.4,  # C
        "average_wind_speed": 34.1,  # C
        "gust_wind_speed": 98.5,  # C
        "wind_direction": 256,  # C
        "rainfall": 19.3,
    }

    WH1080_SAMPLE = {
        "time_stamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Comes from the DB. This is encoded separately for transmission.
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Common with live
        "indoor_humidity": 23,  # C
        "indoor_temperature": 5.4,  # C
        "temperature": 9.3,  # C
        "humidity": 83,  # C
        "pressure": 913.4,  # C
        "average_wind_speed": 34.1,  # C
        "gust_wind_speed": 98.5,  # C
        "wind_direction": 256,  # C
        "rainfall": 19.3,
        "sample_interval": 300,
        "record_number": 23,
        "last_in_batch": True,
        "invalid_data": False,
        "wh1080_wind_direction": "NNE",
        "total_rain": 125512,
        "rain_overflow": False
    }

    DAVIS_LIVE = {
        "live_diff_sequence": 397,  # 0
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # 1  Common with sample
        "indoor_humidity": 23,  # 2  C
        "indoor_temperature": 5.4,  # 3  C
        "temperature": 9.3,  # 4  C
        "humidity": 83,  # 5  C
        "pressure": 913.4,  # 6  C
        "average_wind_speed": 34.1,  # 7  C
        "gust_wind_speed": 98.5,  # 8  C
        "wind_direction": 256,  # 9  C
        "bar_trend": 6,  # 10
        "rain_rate": 123,  # 11
        "storm_rain": 985,  # 12
        "current_storm_start_date": date(year=2015, month=9, day=13),  # 13    2015-09-13
        "transmitter_battery": 2,  # 14
        "console_battery_voltage": 5.9,  # 15
        "forecast_icon": 7,  # 16
        "forecast_rule_id": 10,  # 17
        "uv_index": 9,  # 18
        "solar_radiation": 1043,  # 19
        "extra_fields": {  # 31    C (all fields)
            "leaf_wetness_1": 12,  # 1
            "leaf_wetness_2": 8,  # 2
            "leaf_temperature_1": 15,  # 3
            "leaf_temperature_2": 19,  # 4
            "soil_moisture_1": 1,  # 5
            "soil_moisture_2": 2,  # 6
            "soil_moisture_3": 3,  # 7
            "soil_moisture_4": 4,  # 8
            "soil_temperature_1": 5,  # 9
            "soil_temperature_2": 6,  # 10
            "soil_temperature_3": 7,  # 11
            "soil_temperature_4": 8,  # 12
            "extra_temperature_1": 9,  # 13
            "extra_temperature_2": 10,  # 14
            "extra_temperature_3": 11,  # 15
            "extra_humidity_1": 12,  # 16
            "extra_humidity_2": 13  # 17
        }
    }

    DAVIS_SAMPLE = {
        "time_stamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Comes from the DB. This is encoded separately for transmission.
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Common with live
        "indoor_humidity": 23,  # C
        "indoor_temperature": 5.4,  # C
        "temperature": 9.3,  # C
        "humidity": 83,  # C
        "pressure": 913.4,  # C
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
        "evapotranspiration": 18456874,
        "high_solar_radiation": 1154,
        "high_uv_index": 12,
        "forecast_rule_id": 7,
        "extra_fields": {  # C (all fields)
            "leaf_wetness_1": 12,
            "leaf_wetness_2": 8,
            "leaf_temperature_1": 15,
            "leaf_temperature_2": 19,
            "soil_moisture_1": 1,
            "soil_moisture_2": 2,
            "soil_moisture_3": 3,
            "soil_moisture_4": 4,
            "soil_temperature_1": 5,
            "soil_temperature_2": 6,
            "soil_temperature_3": 7,
            "soil_temperature_4": 8,
            "extra_temperature_1": 9,
            "extra_temperature_2": 10,
            "extra_temperature_3": 11,
            "extra_humidity_1": 12,
            "extra_humidity_2": 13
        }
    }

    def test_multi_record_round_trip(self):
        """
        Tests that what goes in comes back out.
        """

        hardware_types = {
            1: "DAVIS",
            2: "FOWH1080",
            3: "GENERIC"
        }

        packet = self.make_packet()

        encoded = packet.encode()

        def log(msg):
            print(msg)

        out_packet = WeatherDataTCPPacket(log)
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

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

    def make_packet(self):
        packet = WeatherDataTCPPacket()
        # Add a selection of records
        rec = SampleDataRecord()
        rec.station_id = 1  # Davis
        rec.timestamp = datetime(2015, 3, 7, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 3, 6, 10, 53, 42)
        rec.field_list = range(2, 24) + [31]
        rec.field_data = str(_encode_dict(
            self.DAVIS_SAMPLE,
            get_sample_field_definitions("DAVIS"),
            rec.field_list,
            {
                "extra_fields": range(1, 18)
            }
        ))
        packet.add_record(rec)

        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 12345
        rec.field_list = range(2, 10)
        rec.field_data = str(_encode_dict(
            self.GENERIC_LIVE,
            get_live_field_definitions("FOWH1080"),
            rec.field_list,
            None
        ))
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 3  # Generic
        rec.timestamp = datetime(2015, 3, 7, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 3, 6, 10, 53, 42)
        rec.field_list = range(2, 11)
        rec.field_data = str(_encode_dict(
            self.GENERIC_SAMPLE,
            get_sample_field_definitions("GENERIC"),
            rec.field_list,
            None
        ))
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 1  # Davis
        rec.timestamp = datetime(2015, 3, 7, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 3, 6, 10, 53, 42)
        rec.field_list = range(2, 24) + [31]

        # Stick some record separators in the sample to make sure thats handled properly
        samp = copy.deepcopy(self.DAVIS_SAMPLE)
        samp["indoor_humidity"] = 0x1E
        samp["wind_direction"] = 0x1E
        samp["record_time"] = 0x1E
        samp["solar_radiation"] = 0x1E

        rec.field_data = str(_encode_dict(
            samp,
            get_sample_field_definitions("DAVIS"),
            rec.field_list,
            {
                "extra_fields": range(1, 18)
            }
        ))
        packet.add_record(rec)

        rec = SampleDataRecord()
        rec.station_id = 2  # WH1080
        rec.timestamp = datetime(2015, 3, 7, 10, 53, 42)
        rec.download_timestamp = datetime(2015, 3, 6, 10, 53, 42)
        rec.field_list = range(2, 11)

        # Stick some record separators in the sample to make sure thats handled properly
        samp = copy.deepcopy(self.WH1080_SAMPLE)
        samp["indoor_humidity"] = 0x1E
        samp["wind_direction"] = 0x1E
        samp["sample_interval"] = 0x1E
        samp["record_number"] = 0x1E

        rec.field_data = str(_encode_dict(
            samp,
            get_sample_field_definitions("FOWH1080"),
            rec.field_list,
            None
        ))
        packet.add_record(rec)

        # This record contains the end-of-record marker early on in the header.
        # We need to make sure this situation is still correctly handled.
        rec = LiveDataRecord()
        rec.station_id = 2  # WH1080
        rec.sequence_id = 0x1E
        rec.field_list = range(2, 10)
        rec.field_data = str(_encode_dict(
            self.GENERIC_LIVE,
            get_live_field_definitions("FOWH1080"),
            rec.field_list,
            None
        ))
        packet.add_record(rec)
        return packet

    def test_single_record_round_trip(self):
        """
        Tests that what goes in comes back out.
        """

        hardware_types = {
            1: "DAVIS",
            2: "FOWH1080",
            3: "GENERIC"
        }

        packet = WeatherDataTCPPacket()

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

        out_packet = WeatherDataTCPPacket()
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

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
            2: "FOWH1080",
            3: "GENERIC"
        }

        packet = WeatherDataTCPPacket()

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

        out_packet = WeatherDataTCPPacket()
        out_packet.decode(encoded)
        out_packet.decode_records(hardware_types)

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

    def test_size(self):
        """
        Checks that the packet size returned is correct
        """

        input_packet = self.make_packet()

        encoded = input_packet.encode()

        size = WeatherDataTCPPacket.packet_size(encoded)

        self.assertEqual(len(encoded), size)


class SampleAcknowledgementPacketTests(unittest.TestCase):
    """
    Tests the Sample acknowledgement packet.
    """

    def test_round_trip(self):
        input_packet = SampleAcknowledgementTCPPacket()

        input_packet.add_sample_acknowledgement(
            15, datetime(2015, 3, 7, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            20, datetime(2015, 4, 8, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            25, datetime(2015, 5, 9, 10, 53, 42))

        encoded = input_packet.encode()

        output_packet = SampleAcknowledgementTCPPacket()
        output_packet.decode(encoded)

        self.assertListEqual(output_packet.sample_acknowledgements(),
                             input_packet.sample_acknowledgements())

    def test_large_station_id_is_rejected(self):
        """
        The field is only 8 bits. So it should accept values no higher than 256
        """
        packet = SampleAcknowledgementTCPPacket()

        # This should be ok
        packet.add_sample_acknowledgement(255, datetime.now())

        # This should fail
        self.assertRaises(ValueError, packet.add_sample_acknowledgement, 256,
                          datetime.now())

    def test_negative_station_id_is_rejected(self):
        """
        The field is unsigned so should accept no values smaller than zero.
        """
        packet = SampleAcknowledgementTCPPacket()

        # This should be ok
        packet.add_sample_acknowledgement(0, datetime.now())

        # This should fail
        self.assertRaises(ValueError, packet.add_sample_acknowledgement, -1,
                          datetime.now())

    def test_free_space_is_indicated_by_add_function(self):
        """
        The add function should indicate if the packet is full by returning
        False or if there is still free space by returning True.
        """
        pkt = SampleAcknowledgementTCPPacket()

        for i in range(1, 256):
            result = pkt.add_sample_acknowledgement(i, datetime.now())

            if i <= 255:
                self.assertTrue(result)
            else:
                self.assertFalse(result)

    def test_size(self):
        """
        Checks that the packet size returned is correct
        """

        input_packet = SampleAcknowledgementTCPPacket()

        input_packet.add_sample_acknowledgement(
            15, datetime(2015, 3, 7, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            20, datetime(2015, 4, 8, 10, 53, 42))
        input_packet.add_sample_acknowledgement(
            25, datetime(2015, 5, 9, 10, 53, 42))

        encoded = input_packet.encode()

        size = SampleAcknowledgementTCPPacket.packet_size(encoded)

        self.assertEqual(len(encoded), size)


class ImageTcpPacketTests(unittest.TestCase):
    def test_round_trip(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = "-12-title-34-"
        description = "-56-description-78-"
        mime_type = "-90-mime-type-12-"
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_without_title(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = ""
        description = "-56-description-78-"
        mime_type = "-90-mime-type-12-"
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_without_description(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = "-12-title-34-"
        description = ""
        mime_type = "-90-mime-type-12-"
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_without_mime_type(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = "-12-title-34-"
        description = "-56-description-78-"
        mime_type = ""
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_without_metadata(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = "-12-title-34-"
        description = "-56-description-78-"
        mime_type = "-90-mime-type-12-"
        metadata = ""
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_without_title_or_description(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = ""
        description = ""
        mime_type = "-90-mime-type-12-"
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_round_trip_with_no_text_fields(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = ""
        description = ""
        mime_type = ""
        metadata = ""
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.image_type_id, type_id)
        self.assertEqual(output_packet.image_source_id, source_id)
        self.assertEqual(output_packet.timestamp, time)
        self.assertEqual(output_packet.title, title)
        self.assertEqual(output_packet.description, description)
        self.assertEqual(output_packet.mime_type, mime_type)
        self.assertEqual(output_packet.metadata, metadata)
        self.assertEqual(output_packet.image_data, image_data)

    def test_packet_size(self):
        type_id = 1
        source_id = 2
        time = datetime(2016, 2, 3, 22, 53, 18)
        title = "-12-title-34-"
        description = "-56-description-78-"
        mime_type = "-90-mime-type-12-"
        metadata = "-34-metadata-56-"
        image_data = "~!@#$%^^&*()\x1E\x00\xDE\xAD\xBE\xEFThis should accept " \
                     "binary\x00data!@#$%^&"

        input_packet = ImageTCPPacket(type_id, source_id, time, title,
                                      description, mime_type, metadata,
                                      image_data)

        encoded = input_packet.encode()

        output_packet = ImageTCPPacket()

        required = output_packet.packet_size_bytes_required()

        header = encoded[:required]

        size = output_packet.packet_size(header)

        self.assertEqual(size, len(encoded))


class ImageAcknowledgementTCPPacketTests(unittest.TestCase):
    def test_round_trip_with_one_record(self):
        input_packet = ImageAcknowledgementTCPPacket()
        input_packet.add_image(1, 2, datetime(2016, 2, 3, 22, 53, 18))

        encoded = input_packet.encode()

        output_packet = ImageAcknowledgementTCPPacket()
        output_packet.decode(encoded)

        self.assertListEqual(output_packet.images, input_packet.images)

    def test_round_trip_with_two_records(self):
        input_packet = ImageAcknowledgementTCPPacket()
        input_packet.add_image(1, 2, datetime(2016, 2, 3, 22, 53, 18))
        input_packet.add_image(3, 4, datetime(2016, 2, 3, 23, 53, 18))

        encoded = input_packet.encode()

        output_packet = ImageAcknowledgementTCPPacket()
        output_packet.decode(encoded)

        self.assertListEqual(output_packet.images, input_packet.images)

    def test_round_trip_with_three_records(self):
        input_packet = ImageAcknowledgementTCPPacket()
        input_packet.add_image(1, 2, datetime(2016, 2, 3, 22, 53, 18))
        input_packet.add_image(3, 4, datetime(2016, 2, 3, 23, 53, 18))
        input_packet.add_image(5, 6, datetime(2016, 2, 4, 0, 53, 18))

        encoded = input_packet.encode()

        output_packet = ImageAcknowledgementTCPPacket()
        output_packet.decode(encoded)

        self.assertListEqual(output_packet.images, input_packet.images)

    def test_packet_size(self):
        input_packet = ImageAcknowledgementTCPPacket()
        input_packet.add_image(1, 2, datetime(2016, 2, 3, 22, 53, 18))
        input_packet.add_image(3, 4, datetime(2016, 2, 3, 23, 53, 18))
        input_packet.add_image(5, 6, datetime(2016, 2, 4, 0, 53, 18))

        encoded = input_packet.encode()

        output_packet = ImageAcknowledgementTCPPacket()

        required = output_packet.packet_size_bytes_required()

        header = encoded[:required]

        size = output_packet.packet_size(header)

        self.assertEqual(size, len(encoded))