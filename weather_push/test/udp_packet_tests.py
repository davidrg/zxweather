# coding=utf-8
import copy
import struct
import unittest
from datetime import datetime, date

from zxw_push.common.data_codecs import _encode_dict, get_sample_field_definitions, get_live_field_definitions
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

    GENERIC_LIVE = {
        "live_diff_sequence": 397,  # 0
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # 1  Common with sample
        "indoor_humidity": 23,  # 2  C
        "indoor_temperature": 5.4,  # 3  C
        "temperature": 9.3,  # 4  C
        "humidity": 83,  # 5  C
        "pressure": 913.4,  # 6  C
        "msl_pressure": 800.1,  # 7 C
        "average_wind_speed": 34.1,  # 8  C
        "gust_wind_speed": 98.5,  # 9  C
        "wind_direction": 256,  # 10  C
    }

    GENERIC_SAMPLE = {
        "time_stamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Comes from the DB. This is encoded separately for transmission.
        "sample_diff_timestamp": datetime(year=2015, month=9, day=13, hour=1, minute=15),  # Common with live
        "indoor_humidity": 23,  # C
        "indoor_temperature": 5.4,  # C
        "temperature": 9.3,  # C
        "humidity": 83,  # C
        "pressure": 913.4,  # C
        "msl_pressure": 800.1,  # 7 C
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
        "msl_pressure": 800.1,  # 7 C
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
        "msl_pressure": 800.1,  # 7 C
        "average_wind_speed": 34.1,  # 8  C
        "gust_wind_speed": 98.5,  # 9  C
        "wind_direction": 256,  # 10  C
        "bar_trend": 6,  # 12
        "rain_rate": 123,  # 13
        "storm_rain": 985,  # 14
        "current_storm_start_date": date(year=2015, month=9, day=13),  # 15    2015-09-13
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
        "msl_pressure": 800.1,  # 7 C
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
        "evapotranspiration": 3.4,
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

    def make_packet(self):
        packet = WeatherDataUDPPacket(sequence=5, authorisation_code=0xABCDEFAB)
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
            2: "FOWH1080",
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
            2: "FOWH1080",
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
