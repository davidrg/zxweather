# coding=utf-8
import unittest
from zxw_push.common.packets import Packet, StationInfoRequestPacket, \
    StationInfoResponsePacket

__author__ = 'david'

class PacketTests(unittest.TestCase):
    def test_packet_round_trip(self):
        input_packet = Packet()
        input_packet.packet_type = 20
        input_packet.sequence = 123
        input_packet.authorisation_code = 9876543210

        encoded = input_packet.encode()

        output_packet = Packet()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, 20)
        self.assertEqual(output_packet.sequence, 123)
        self.assertEqual(output_packet.authorisation_code, 9876543210)

    def test_station_info_request_packet_round_trip(self):
        input_packet = StationInfoRequestPacket()
        input_packet.sequence = 123
        input_packet.authorisation_code = 9876543210

        encoded = input_packet.encode()

        output_packet = StationInfoRequestPacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.sequence, 123)
        self.assertEqual(output_packet.authorisation_code, 9876543210)

    def test_station_info_response_packet_round_trip(self):
        input_packet = StationInfoResponsePacket()
        input_packet.sequence = 123
        input_packet.authorisation_code = 9876543210

        input_packet.add_station("test1", "davis")
        input_packet.add_station("test2", "wh1080")
        input_packet.add_station("test3", "generic")

        encoded = input_packet.encode()

        output_packet = StationInfoResponsePacket()
        output_packet.decode(encoded)

        self.assertEqual(output_packet.packet_type, input_packet.packet_type)
        self.assertEqual(output_packet.sequence, 123)
        self.assertEqual(output_packet.authorisation_code, 9876543210)

        self.assertListEqual(output_packet.stations, input_packet.stations)