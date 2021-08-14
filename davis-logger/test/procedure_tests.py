"""
Tests station procedures
"""
import struct
import unittest

from davis_logger.record_types.util import CRC
from davis_logger.station_procedures import DstSwitchProcedure


class WriteReceiver(object):
    def __init__(self):
        self.Data = bytes()

    def write(self, data):
        self.Data = data


class FinishedDetector(object):
    def __init__(self):
        self.IsFinished = False

    def finished(self):
        self.IsFinished = True


class TestDstSwitchProcedure(unittest.TestCase):
    # Basic procedure to set DST:
    #       > Send to Station
    #       < Receive from Station
    # > EEBWR 13 1      Start EEPROM Write
    # < ACK
    # > data            Send new EEPROM value with two byte CRC
    # < ACK
    # > GETTIME         Get current time from station
    # < ACK
    # < data            Receive time with two byte CRC
    # > SETTIME         Set current time on station
    # < ACK
    # > data            Current time with two byte CRC
    # < ACK
    # Finished!

    # TODO: Test feeding the DST Switch procedure data one byte at a time.

    def setUp(self):
        self._ACK = b'\x06'

    def test_turns_DST_on(self):
        # The DST setting lives at address 0x13 and occupies a single byte.
        #  Value 0 = DST off
        #  Value 1 = DST on

        # The conversation should look like this:
        # > EEBWR 13 1
        # < ACK
        # > data+crc
        # < ACK

        recv = WriteReceiver()

        proc = DstSwitchProcedure(recv.write, True)

        proc.start()
        self.assertEqual(b'EEBWR 13 1\n', recv.Data)
        proc.data_received(self._ACK)
        self.assertEqual(b'\x01\x10!', recv.Data)
        proc.data_received(self._ACK)

    def test_turns_DST_off(self):
        # The DST setting lives at address 0x13 and occupies a single byte.
        #  Value 0 = DST off
        #  Value 1 = DST on

        # The conversation should look like this:
        # > EEBWR 13 1
        # < ACK
        # > data+crc
        # < ACK

        recv = WriteReceiver()

        proc = DstSwitchProcedure(recv.write, False)

        proc.start()
        self.assertEqual(b'EEBWR 13 1\n', recv.Data)
        proc.data_received(self._ACK)
        self.assertEqual(b'\x00\x00\x00', recv.Data)
        proc.data_received(self._ACK)

    def test_reads_time_after_DST_update(self):
        recv = WriteReceiver()

        proc = DstSwitchProcedure(recv.write, True)
        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(self._ACK)
        self.assertEqual(b'GETTIME\n', recv.Data)

    def test_puts_clock_forward_for_dst_on(self):
        recv = WriteReceiver()

        proc = DstSwitchProcedure(recv.write, True)
        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(self._ACK)
        proc.data_received(self._ACK)

        # Current time: 14-AUG-2021 06:21:15 + CRC
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertEqual(b'SETTIME\n', recv.Data)
        proc.data_received(self._ACK)

        # New time: 14-AUG-2021 07:21:15 + CRC
        self.assertEqual(b'\x0f\x15\x07\x0e\x08r\x7f^', recv.Data)

    def test_puts_clock_backward_for_dst_off(self):
        recv = WriteReceiver()

        proc = DstSwitchProcedure(recv.write, False)
        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(self._ACK)
        proc.data_received(self._ACK)

        # Current time: 14-AUG-2021 06:21:15 + CRC
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertEqual(b'SETTIME\n', recv.Data)
        proc.data_received(self._ACK)

        # New time: 14-AUG-2021 05:21:15 + CRC
        crc = 37430
        crc_packed = struct.pack(CRC.FORMAT, crc)
        expected = b'\x0f\x15\x05\x0e\x08r' + crc_packed
        self.assertEqual(expected, recv.Data)

    def test_procedure_doesnt_finish_until_after_clock_set(self):
        recv = WriteReceiver()
        fd = FinishedDetector()

        proc = DstSwitchProcedure(recv.write, True)
        proc.finished += fd.finished
        self.assertFalse(fd.IsFinished)
        proc.start()
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertTrue(fd.IsFinished)
