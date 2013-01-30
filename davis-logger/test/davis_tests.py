# coding=utf-8
"""
Unit tests for the davis module
"""
import unittest
from davis_logger.davis import DavisWeatherStation, STATE_SLEEPING, STATE_AWAKE
from davis_logger.util import Event
from test.util import CallTracker

__author__ = 'david'

class DavisTests(unittest.TestCase):
    def setUp(self):
        self.station = DavisWeatherStation()

    def test_init(self):
        self.assertEqual(self.station._state, STATE_SLEEPING)
        self.assertEqual(self.station._wake_attempts, 0)


    def test_wakeup(self):
        """
        Tests that the station will wake on first try. In the future it will
        make one attempt every 1.2 seconds (up to a maximum of three attempts)
        before giving up. This calls for somewhat nastier test code.
        """
        def _sendData(data):
            pass

        tracker = CallTracker(_sendData)

        self.station.sendData += tracker
        self.station._wakeUp()

        self.assertEqual(tracker.args[0], '\n')

        self.station.dataReceived('\r\n')

        self.assertEqual(self.station._state, STATE_AWAKE)


    def test_loop_packets(self):

        # Setup the sendData tracker
        def _sendData(data):
            pass
        tracker = CallTracker(_sendData)
        self.station.sendData += tracker

        self.assertEqual(self.station._lps_packets_remaining, 0)
        self.assertFalse(self.station._lps_acknowledged)

        self.station.getLoopPackets(3)

        # It should send an LPS command
        self.assertEqual(tracker.args[0], 'LPS 1 3\n')
        self.assertEqual(self.station._lps_packets_remaining, 3)

        # Then the hardware responds with an ACK.
        self.station.dataReceived(self.station._ACK)
        self.assertTrue(self.station._lps_acknowledged)

        # Then the hardware responds with a new LOOP2 packet every 2.5 seconds.
        # We'll just spam through three of them in one go.

        # This is a rev. B packet:
        #    LOO - magic number
        #   0xC4 - bar trend: -60, falling rapidly
        #      1 - LOOP2 packet type
        #   0x7F - Reserved A
        #   0xFF - Reserved B
        packet_1 = 'LOO\xC41\x7F\xFF'

        # This is a rev. A packet:
        #   LOOP - magic number
        #        - bar trend not present
        #      1 - LOOP2 packet type
        #   0x7F - Reserved A
        #   0xFF - Reserved B
        packet_2 = 'LOOP1\x7F\xFF'


        # This is a rev. B packet:
        #    LOO - magic number
        #   0x01 - bar trend: insufficient data to determine bar trend
        #      1 - LOOP2 packet type
        #   0x7F - Reserved A
        #   0xFF - Reserved B
        packet_3 = 'LOO\x011\x7F\xFF'

        # TODO: test CRC checking.
        # TODO: check double-ack