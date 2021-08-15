"""
Tests station procedures
"""
import struct
import unittest

from davis_logger.record_types.util import CRC
from davis_logger.station_procedures import DstSwitchProcedure, GetConsoleInformationProcedure


class WriteReceiver(object):
    """
    Helper for receiving writes from a procedure
    """
    def __init__(self):
        self.Data = bytes()

    def write(self, data):
        """
        Write callback to provide to the procedure. Data will appear in the
        Data property after each write.
        :param data: Data from the procedure
        :type data: bytes
        """
        self.Data = data


class FinishedDetector(object):
    """
    Helper for catching a procedures finished event
    """
    def __init__(self):
        self.IsFinished = False

    def finished(self):
        """
        Bind this to the procedures finished event
        """
        self.IsFinished = True


def byte_at_a_time_send(procedure, data):
    """
    Sends data to the procedure one byte at a time to check its buffering data
    properly
    :param procedure: Procedure to send data to
    :type procedure: Procedure
    :param data: Data to send
    :type data: bytes
    """
    for i in range(len(data)):
        byte = data[i:i + 1]
        procedure.data_received(byte)


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


class TestGetConsoleInformationProcedure(unittest.TestCase):
    # Basic procedure is:
    #       > Send to Station
    #       < Receive from Station
    # > WRD\x12\x4D\n
    # < ACK
    # < Station Type
    # > VER\n
    # < \r\nOK\r\n
    # < "Apr 24 2002"\r\n
    # > NVER\n                      ! ONLY IF station type is Vantage Pro2 or Vantage Vue
    # > \r\nOK\r\n
    # > version-number

    def setUp(self):
        self._ACK = b'\x06'

    def test_all_information_unknown_at_start(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        self.assertEqual("Unknown", proc.hw_type)
        self.assertIsNone(proc.version)
        self.assertIsNone(proc.version_date)

    def hw_type(self, type_number):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()

        self.assertEqual(b'WRD\x12\x4D\n', recv.Data)
        proc.data_received(self._ACK)
        proc.data_received(bytearray([type_number]))
        return proc.hw_type

    def test_hw_type_wizard_III(self):
        self.assertEqual("Wizard III", self.hw_type(0))

    def test_hw_type_wizard_II(self):
        self.assertEqual("Wizard II", self.hw_type(1))

    def test_hw_type_monitor(self):
        self.assertEqual("Monitor", self.hw_type(2))

    def test_hw_type_perception(self):
        self.assertEqual("Perception", self.hw_type(3))

    def test_hw_type_groweather(self):
        self.assertEqual("GroWeather", self.hw_type(4))

    def test_hw_type_energy_enviromonitor(self):
        self.assertEqual("Energy Enviromonitor", self.hw_type(5))

    def test_hw_type_health_enviromonitor(self):
        self.assertEqual("Health Enviromonitor", self.hw_type(6))

    def test_hw_type_vantage_pro(self):
        self.assertEqual("Vantage Pro, Vantage Pro2", self.hw_type(16))

    def test_hw_type_vantage_vue(self):
        self.assertEqual("Vantage Vue", self.hw_type(17))

    def test_version_date(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()

        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))
        self.assertEqual(b'VER\n', recv.Data)
        proc.data_received(b'\r\nOK\r\nJan 22 2018\r\n')
        self.assertEqual("Jan 22 2018", proc.version_date)

    def test_version(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()

        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))
        proc.data_received(b'\r\nOK\r\nJan 22 2018\r\n')
        self.assertEqual(b"NVER\n", recv.Data)
        proc.data_received(b"\n\rOK\n\r3.83\n\r")
        self.assertEqual(proc.version, '3.83')

    def test_finished_only_after_entire_procedure(self):
        recv = WriteReceiver()
        fd = FinishedDetector()

        proc = GetConsoleInformationProcedure(recv.write)
        proc.finished += fd.finished

        proc.start()
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        proc.data_received(bytearray([16]))
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\r\nOK\r\nJan 22 2018\r\n')
        self.assertFalse(fd.IsFinished)
        proc.data_received(b"\n\rOK\n\r3.83\n\r")
        self.assertTrue(fd.IsFinished)

    def test_buffering(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(b'\x10')
        self.assertEqual("Vantage Pro, Vantage Pro2", proc.hw_type)
        byte_at_a_time_send(proc, b'\r\nOK\r\nJan 22 2018\r\n')
        self.assertEqual(b"NVER\n", recv.Data)
        byte_at_a_time_send(proc, b"\n\rOK\n\r3.83\n\r")
        self.assertEqual(proc.version, '3.83')

    def test_post_190_firmware_marked_as_supporting_lps(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()

        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))
        proc.data_received(b'\r\nOK\r\nJan 22 2018\r\n')
        proc.data_received(b"\n\rOK\n\r3.83\n\r")
        self.assertTrue(proc.lps_supported)

    def test_pre_190_firmware_marked_as_not_supporting_lps(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()

        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))
        proc.data_received(b'\r\nOK\r\nDec 30 2008\r\n')
        self.assertIsNotNone(proc.lps_supported)
        self.assertFalse(proc.lps_supported)

    def test_version_not_requested_for_pre_190_firmware(self):
        recv = WriteReceiver()
        fd = FinishedDetector()

        proc = GetConsoleInformationProcedure(recv.write)
        proc.finished += fd.finished

        proc.start()
        self.assertFalse(fd.IsFinished)
        proc.data_received(self._ACK)
        self.assertFalse(fd.IsFinished)
        proc.data_received(bytearray([16]))
        self.assertFalse(fd.IsFinished)
        recv.Data = None
        proc.data_received(b'\r\nOK\r\nDec 30 2008\r\n')
        self.assertIsNone(recv.Data)
        self.assertTrue(fd.IsFinished)

    def test_upgradable_firmware_detected(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))

        # Firmware dated 28 November 2005 or newer supports firmware upgrades
        # from the PC
        proc.data_received(b'\r\nOK\r\nNov 28 2005\r\n')
        self.assertIsNotNone(proc.self_firmware_upgrade_supported)
        self.assertTrue(proc.self_firmware_upgrade_supported)

    def test_non_upgradable_firmware_detected(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(bytearray([16]))

        # A special tool is required for upgrading from firmware dated prior to
        # 28 November 2005.
        proc.data_received(b'\r\nOK\r\nNov 13 2004\r\n')
        self.assertIsNotNone(proc.self_firmware_upgrade_supported)
        self.assertFalse(proc.self_firmware_upgrade_supported)

    def test_vue_firmware_always_requests_version_number(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(bytearray([17]))

        proc.data_received(b'\r\nOK\r\nNov 13 2004\r\n')
        self.assertEqual(b"NVER\n", recv.Data)
        proc.data_received(b"\n\rOK\n\r1.0\n\r")
        self.assertIsNotNone(proc.lps_supported)
        self.assertTrue(proc.lps_supported)

    def test_vue_firmware_always_upgradable(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(bytearray([17]))

        proc.data_received(b'\r\nOK\r\nNov 13 2004\r\n')
        proc.data_received(b"\n\rOK\n\r1.0\n\r")
        self.assertIsNotNone(proc.self_firmware_upgrade_supported)
        self.assertTrue(proc.self_firmware_upgrade_supported)

    def test_vue_firmware_always_supports_lps(self):
        recv = WriteReceiver()

        proc = GetConsoleInformationProcedure(recv.write)

        proc.start()
        proc.data_received(self._ACK)
        proc.data_received(bytearray([17]))

        # A special tool is required for upgrading from firmware dated prior to
        # 28 November 2005.
        proc.data_received(b'\r\nOK\r\nNov 13 2004\r\n')
        proc.data_received(b"\n\rOK\n\r1.0\n\r")
        self.assertIsNotNone(proc.lps_supported)
        self.assertTrue(proc.lps_supported)
