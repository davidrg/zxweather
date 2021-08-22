"""
Tests station procedures
"""
import random

import datetime
import struct
import unittest

from davis_logger.record_types.dmp import Dmp, encode_date, encode_time, build_page, serialise_dmp, _compass_points, \
    deserialise_dmp
from davis_logger.record_types.util import CRC
from davis_logger.station_procedures import DstSwitchProcedure, GetConsoleInformationProcedure, \
    GetConsoleConfigurationProcedure, DmpProcedure


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

    def read(self):
        """
        Returns and wipes the buffer.
        :return: Buffer contents
        :rtype: bytes
        """
        data = self.Data
        self.Data = bytes()
        return data


class LogReceiver(object):
    """
    Helper for receiving log messages from a procedure
    """
    def __init__(self):
        self.LastMessage = ''

    def log(self, msg):
        """
        Log callback to provide to the procedure. The most recent log message
        will appear in the LastMessage property.
        :param msg: Log message
        :type msg: str
        """
        print("Log: {0}".format(msg))
        self.LastMessage = msg


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



class TestGetConsoleConfigurationProcedure(unittest.TestCase):
    # Basic procedure is:
    #       > Send to Station
    #       < Receive from Station
    # > GETTIME\n               Get the current station time
    # < current-time
    # > EEBRD 2B 01\n           Get rain gauge size
    # < ACK
    # < rain gauge size
    # < CRC (two bytes)         (not currently checked)
    # > EEBRD 2D 01\n           Get archive interval
    # < ACK
    # < archive interval
    # < CRC (two bytes)         (not currently checked)
    # > EEBRD 12 1\n            Get daylight savings type
    # < ACK
    # < daylight-savings-type
    # < CRC (two bytes)
    # > EEBRD 13 1\n            Get daylight savings status
    # < ACK
    # < daylight-savings-status
    # < CRC (two bytes)

    def test_decodes_current_time(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        self.assertEqual(b'GETTIME\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Current time: 2021-08-16 22:21:37
        proc.data_received(b'%\x15\x16\x10\x08yHN')
        self.assertEqual(
            datetime.datetime(year=2021, month=8, day=16,
                              hour=22, minute=21, second=37),
            proc.CurrentStationTime)

    def test_point_01_inch_rain_gauge(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertEqual(b'EEBRD 2B 01\n', recv.Data)  # Read rain gauge size
        # Size is in setup bits 5 and 4. 0 = 0.01 inches. So a setup byte of 0.
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x00\x00\x00')
        self.assertEqual("0.01 Inches", proc.RainSizeString)
        self.assertEqual(0.254, proc.RainSizeMM)

    def test_point02_mm_rain_gauge(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertEqual(b'EEBRD 2B 01\n', recv.Data)  # Read rain gauge size
        # Size is in setup bits 5 and 4. 1 = 0.2mm. So a setup byte of 0x10.
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')
        self.assertEqual("0.2mm", proc.RainSizeString)
        self.assertEqual(0.2, proc.RainSizeMM)

    def test_point_01_mm_rain_gauge(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')
        self.assertEqual(b'EEBRD 2B 01\n', recv.Data)  # Read rain gauge size
        # Size is in setup bits 5 and 4. 2 = 0.1mm. So a setup byte of 0x30.
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b' $b')
        self.assertEqual("0.1mm", proc.RainSizeString)
        self.assertEqual(0.1, proc.RainSizeMM)

    def test_decodes_archive_intervals(self):
        recv = WriteReceiver()

        def test_interval(interval):
            proc = GetConsoleConfigurationProcedure(recv.write)

            proc.start()
            proc.data_received(b'\x06')  # ACK
            proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
            proc.data_received(b'\x06')  # ACK
            proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
            self.assertEqual(b"EEBRD 2D 01\n", recv.Data)  # Read archive interval
            proc.data_received(b'\x06')  # ACK
            interval_bytes = bytearray([interval])
            crc = CRC.calculate_crc(bytes(interval_bytes))
            interval_bytes.extend(struct.pack(CRC.FORMAT, crc))
            proc.data_received(bytes(interval_bytes))
            self.assertEqual(interval, proc.ArchiveIntervalMinutes)

        # These are all the valid archive intervals
        interval_options = [1, 5, 10, 15, 30, 60, 120]

        for option in interval_options:
            test_interval(option)

    def test_auto_dst(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        self.assertEqual(b'EEBRD 12 1\n', recv.Data)  # Get DST Mode
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x00\x00\x00')  # Auto DST = 0
        self.assertEqual(True, proc.AutoDSTEnabled)
        self.assertIsNone(proc.DSTOn)

    def test_finished_after_detecting_auto_dst(self):
        recv = WriteReceiver()
        fd = FinishedDetector()

        proc = GetConsoleConfigurationProcedure(recv.write)
        proc.finished += fd.finished

        proc.start()
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x00\x00\x00')  # Auto DST = 0

        # Procedure should finish after confirming DST mode is Auto
        self.assertTrue(fd.IsFinished)

    def test_manual_dst(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        self.assertEqual(b'EEBRD 12 1\n', recv.Data)  # Get DST Mode
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x01\x10!')  # Manual DST = 1
        self.assertFalse(proc.AutoDSTEnabled)

    def test_manual_dst_checks_dst_status(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x01\x10!')  # Manual DST = 1
        self.assertEqual(b'EEBRD 13 1\n', recv.Data)  # Get manual DST ON

    def test_manual_dst_on(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x01\x10!')  # Manual DST = 1
        self.assertEqual(b'EEBRD 13 1\n', recv.Data)  # Get manual DST ON
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x01\x10!')  # DST mode is ON
        self.assertTrue(proc.DSTOn)

    def test_manual_dst_off(self):
        recv = WriteReceiver()

        proc = GetConsoleConfigurationProcedure(recv.write)

        proc.start()
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x01\x10!')  # Manual DST = 1
        self.assertEqual(b'EEBRD 13 1\n', recv.Data)  # Get manual DST ON
        proc.data_received(b'\x06')  # ACK
        proc.data_received(b'\x00\x00\x00')  # DST mode is OFF
        self.assertFalse(proc.DSTOn)

    def test_finished_after_getting_manual_dst_status(self):
        recv = WriteReceiver()
        fd = FinishedDetector()

        proc = GetConsoleConfigurationProcedure(recv.write)
        proc.finished += fd.finished

        proc.start()
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x0f\x15\x06\x0e\x08r\t\xea')  # Time
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x10\x121')  # Rain gauge size (0.2mm)
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x05P\xa5')  # 5 minutes
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x01\x10!')  # Manual DST = 1
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        proc.data_received(b'\x00\x00\x00')  # DST mode is OFF

        self.assertTrue(fd.IsFinished)

class TestDmpProcedure(unittest.TestCase):
    # Basic procedure is:
    #       > Send to Station
    #       < Receive from Station
    # > DMPAFT\n                Get archive records since timestamp
    # < ACK
    # > Time+Date+CRC
    # If CRC Bad: < CANCEL      Console sends CANCEL on bad CRC. Go to start.
    # If bad TS:  < NAK         NAK if we didn't send a full timestamp+CRC
    # < ACK
    # < page count
    # < first record location
    # IF CRC Bad: go to start.
    # IF page count == 0: finished
    # < receive data

    def test_sends_start_time(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK

        # datetime.datetime(2021, 8, 18, 20, 42)
        # encoded date: 11026
        # encoded time: 2042
        # Packed: '\x12+\xfa\x07'
        # CRC: '\x0c\x15'
        # Full TS: '\x12+\xfa\x07\x0c\x15'
        self.assertEqual(b'\x12+\xfa\x07\x0c\x15', recv.Data)

    def test_retries_on_failed_timestamp_crc(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(DmpProcedure._CANCEL)  # Signal CRC error
        self.assertEqual(b'DMPAFT\n', recv.Data)

    def test_retries_on_nak(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(DmpProcedure._NAK)
        self.assertEqual(b'DMPAFT\n', recv.Data)

    def test_decodes_page_count(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(2, 3))
        self.assertEqual(2, proc._dmp_page_count)
        self.assertEqual(3, proc._dmp_first_record)

    def test_restarts_on_failed_page_count_crc(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        page_data = bytearray()
        page_data.extend(struct.pack('<HH', 2, 3))
        page_data.extend(struct.pack(CRC.FORMAT,
                                 CRC.calculate_crc(struct.pack('<HH', 3, 3))))
        proc.data_received(page_data)
        self.assertEqual(b'DMPAFT\n', recv.Data)

    def test_finishes_when_no_new_data(self):
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        self.assertFalse(fd.IsFinished)
        proc.start()
        self.assertFalse(fd.IsFinished)
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK
        self.assertFalse(fd.IsFinished)

        proc.data_received(TestDmpProcedure._page_count(0, 1))
        self.assertTrue(fd.IsFinished)

    @staticmethod
    def _make_archive_records(start_time, archive_interval, count, rain_collector_size):
        """
        Creates the specified number of Dmp records populated with random data.

        :param start_time: Timestamp for first record
        :type start_time: datetime.datetime
        :param archive_interval: Timestamp increment in minutes
        :type archive_interval: int
        :param count: Number of records to generate
        :type count: int
        :param rain_collector_size: Rain collector size (in mm)
        :type rain_collector_size: float
        :return: List of Dmp records
        :rtype: List of Dmp
        """
        records = []
        ts = start_time

        station_id = 1
        max_wind_samples = archive_interval * 60 / (41 + station_id - 1/16)

        for i in range(0, count):
            wind_speed = random.uniform(0, 22)
            wind_direction = None
            gust_wind_speed = random.uniform(0, 22)
            gust_direction = None

            if wind_speed > 0:
                wind_direction = random.choice(_compass_points)
            if gust_wind_speed > 0:
                gust_direction = random.choice(_compass_points)

            d = Dmp(dateStamp=ts.date(),
                    timeStamp=ts.time(),
                    timeZone=None,
                    dateInteger=encode_date(ts.date()),
                    timeInteger=encode_time(ts.time()),
                    outsideTemperature=round(random.uniform(-3, 35), 1),
                    highOutsideTemperature=round(random.uniform(-3, 35), 1),
                    lowOutsideTemperature=round(random.uniform(-3, 35), 1),
                    rainfall=round(random.uniform(0, 300), 1),
                    highRainRate=round(random.uniform(0, 500), 1),
                    barometer=round(random.uniform(985.9, 1039.3), 1),
                    solarRadiation=int(random.uniform(0, 1628)),
                    numberOfWindSamples=int(random.uniform(0, max_wind_samples)),
                    insideTemperature=round(random.uniform(-3, 35), 1),
                    insideHumidity=int(random.uniform(0, 100)),
                    outsideHumidity=int(random.uniform(0, 100)),
                    averageWindSpeed=wind_speed,
                    highWindSpeed=gust_wind_speed,
                    highWindSpeedDirection=gust_direction,
                    prevailingWindDirection=wind_direction,
                    averageUVIndex=round(random.uniform(0, 14), 1),
                    ET=round(random.uniform(0, 1), 1),
                    highSolarRadiation=int(random.uniform(0, 1628)),
                    highUVIndex=round(random.uniform(0, 14), 1),
                    forecastRule=int(random.uniform(0, 196)),
                    leafTemperature=[
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1)
                    ],
                    leafWetness=[
                        int(random.uniform(0, 15)),
                        int(random.uniform(0, 15))
                    ],
                    soilTemperatures=[
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1)
                    ],
                    extraHumidities=[
                        int(random.uniform(0, 100)),
                        int(random.uniform(0, 100))
                    ],
                    extraTemperatures=[
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1),
                        round(random.uniform(-3, 35), 1)
                    ],
                    soilMoistures=[
                        int(round(random.uniform(0, 254), 0)),
                        int(round(random.uniform(0, 254), 0)),
                        int(round(random.uniform(0, 254), 0)),
                        int(round(random.uniform(0, 254), 0))
                    ])

            # Pass the data through the DMP binary format which will result in
            # Metric -> US Imperial -> Metric conversion. We do this here so
            # we don't trip up any unit tests.
            d = deserialise_dmp(serialise_dmp(d, rain_collector_size), rain_collector_size)
            records.append(d)

            ts = ts + datetime.timedelta(minutes=archive_interval)
        return records

    def _assertDmpEqual(self, a, b, index):
        """
        Asserts the two Dmp records are equal. Floating point values within
        each Dmp record are checked using
        :param a: Dmp record A
        :type a: Dmp
        :param b: Dmp record B
        :type b: Dmp
        :param index: record ID
        :type index: int
        """

        def _assert_field_equal(i, field_name, a_value, b_value, places=1):
            if isinstance(a_value, float):
                self.assertAlmostEqual(
                    a_value, b_value,
                    places=places,
                    msg="index {0} field {1}:{5} differs to {4} places - a={2}, b={3}".format(
                        i, field_name, round(a_value, places),
                        round(b_value, places), places, type(a_value).__name__))
            else:
                self.assertEqual(a_value, b_value,
                                 msg="index {0} field {1}:{4} differs - a={2}, b={3}".format(
                                     i, field_name, a_value, b_value,
                                     type(a_value).__name__))

        _assert_field_equal(index, "dateStamp", a.dateStamp, b.dateStamp)
        _assert_field_equal(index, "timeStamp", a.timeStamp, b.timeStamp)
        _assert_field_equal(index, "timeZone", a.timeZone, b.timeZone)
        _assert_field_equal(index, "dateInteger", a.dateInteger, b.dateInteger)
        _assert_field_equal(index, "timeInteger", a.timeInteger, b.timeInteger)
        _assert_field_equal(index, "outsideTemperature", a.outsideTemperature, b.outsideTemperature)
        _assert_field_equal(index, "highOutsideTemperature", a.highOutsideTemperature, b.highOutsideTemperature)
        _assert_field_equal(index, "lowOutsideTemperature", a.lowOutsideTemperature, b.lowOutsideTemperature)
        _assert_field_equal(index, "rainfall", a.rainfall, b.rainfall)
        _assert_field_equal(index, "highRainRate", a.highRainRate, b.highRainRate)
        _assert_field_equal(index, "barometer", a.barometer, b.barometer)
        _assert_field_equal(index, "solarRadiation", a.solarRadiation, b.solarRadiation)
        _assert_field_equal(index, "numberOfWindSamples", a.numberOfWindSamples, b.numberOfWindSamples)
        _assert_field_equal(index, "insideTemperature", a.insideTemperature, b.insideTemperature)
        _assert_field_equal(index, "insideHumidity", a.insideHumidity, b.insideHumidity)
        _assert_field_equal(index, "outsideHumidity", a.outsideHumidity, b.outsideHumidity)
        _assert_field_equal(index, "averageWindSpeed", a.averageWindSpeed, b.averageWindSpeed)
        _assert_field_equal(index, "highWindSpeed", a.highWindSpeed, b.highWindSpeed)
        _assert_field_equal(index, "highWindSpeedDirection", a.highWindSpeedDirection, b.highWindSpeedDirection)
        _assert_field_equal(index, "prevailingWindDirection", a.prevailingWindDirection, b.prevailingWindDirection)
        _assert_field_equal(index, "highUVIndex", a.highUVIndex, b.highUVIndex)
        _assert_field_equal(index, "forecastRule", a.forecastRule, b.forecastRule)
        _assert_field_equal(index, "leafTemperature[0]", a.leafTemperature[0], b.leafTemperature[0])
        _assert_field_equal(index, "leafTemperature[1]", a.leafTemperature[1], b.leafTemperature[1])
        _assert_field_equal(index, "leafWetness[0]", a.leafWetness[0], b.leafWetness[0])
        _assert_field_equal(index, "leafWetness[1]", a.leafWetness[1], b.leafWetness[1])
        _assert_field_equal(index, "soilTemperatures[0]", a.soilTemperatures[0], b.soilTemperatures[0])
        _assert_field_equal(index, "soilTemperatures[1]", a.soilTemperatures[1], b.soilTemperatures[1])
        _assert_field_equal(index, "soilTemperatures[2]", a.soilTemperatures[2], b.soilTemperatures[2])
        _assert_field_equal(index, "soilTemperatures[3]", a.soilTemperatures[3], b.soilTemperatures[3])
        _assert_field_equal(index, "extraHumidities[0]", a.extraHumidities[0], b.extraHumidities[0])
        _assert_field_equal(index, "extraHumidities[1]", a.extraHumidities[1], b.extraHumidities[1])
        _assert_field_equal(index, "extraTemperatures[0]", a.extraTemperatures[0], b.extraTemperatures[0])
        _assert_field_equal(index, "extraTemperatures[1]", a.extraTemperatures[1], b.extraTemperatures[1])
        _assert_field_equal(index, "extraTemperatures[2]", a.extraTemperatures[2], b.extraTemperatures[2])
        _assert_field_equal(index, "soilMoistures[0]", a.soilMoistures[0], b.soilMoistures[0])
        _assert_field_equal(index, "soilMoistures[1]", a.soilMoistures[1], b.soilMoistures[1])
        _assert_field_equal(index, "soilMoistures[2]", a.soilMoistures[2], b.soilMoistures[2])
        _assert_field_equal(index, "soilMoistures[3]", a.soilMoistures[3], b.soilMoistures[3])

    def test_dmp_serialise_round_trip(self):
        """
        This test is to check we can encode and decode a DMP record and not find
        any differences in any fields with a precision of 1dp.

        This is mostly to test the _assertDmpEqual test function that is used
        by many of the other tests in this TestCase.
        """
        count = 500
        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, count, 0.2)
        for i, rec in enumerate(records):
            rec2 = deserialise_dmp(serialise_dmp(rec))
            self._assertDmpEqual(rec, rec2, i)

    @staticmethod
    def _page_count(page_count, first_record):
        page_data = bytearray()
        page_data.extend(struct.pack('<HH',
                                     page_count,  # Number of pages
                                     first_record))  # Location of first record in first page
        page_data.extend(struct.pack(CRC.FORMAT, CRC.calculate_crc(page_data)))
        return page_data

    def test_decodes_one_full_page(self):
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(1, 0))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 5, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page = build_page(
            1,  # Sequence
            encoded_records)

        self.assertFalse(fd.IsFinished)
        proc.data_received(page)
        self.assertTrue(fd.IsFinished)

        self.assertEqual(5, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i], proc.ArchiveRecords[i], i)

    def test_decodes_one_partial_page(self):
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(1, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 5, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page = build_page(
            1,  # Sequence
            encoded_records)

        self.assertFalse(fd.IsFinished)
        proc.data_received(page)
        self.assertTrue(fd.IsFinished)

        self.assertEqual(3, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i+2], proc.ArchiveRecords[i], i)

    def test_decodes_one_partial_and_one_complete_page(self):
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(2, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 10, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())

        self.assertFalse(fd.IsFinished)
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())
        self.assertTrue(fd.IsFinished)

        self.assertEqual(8, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_decodes_two_partial_pages_in_blank_logger(self):
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(2, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 8, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        empty_record = b'\xFF' * 52
        encoded_records.append(empty_record)
        encoded_records.append(empty_record)

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())

        self.assertFalse(fd.IsFinished)
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())
        self.assertTrue(fd.IsFinished)

        self.assertEqual(6, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_decodes_two_partial_pages(self):
        # The next page after the final data record in the last page
        # will have an older timestamp.
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(2, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 8, 0.2)
        records += TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 10, 20, 45), 5, 2, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())

        self.assertFalse(fd.IsFinished)
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())
        self.assertTrue(fd.IsFinished)

        self.assertEqual(6, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_decodes_page_containing_dst_clock_back(self):
        # Check it doesn't treat clocks being put back for daylight savings as
        # being the end of the data
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(2, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 8, 0.2)

        # Then we put the clock back an hour for the final two records
        last_ts = datetime.datetime.combine(
            records[-1].dateStamp, records[-1].timeStamp)
        records += TestDmpProcedure._make_archive_records(
            last_ts - datetime.timedelta(hours=1),
            5, 2, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())

        self.assertEqual(8, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_clock_back_end_must_be_in_final_page(self):
        # It should ignore clocks being put back in any page except the final.
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(3, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 8, 0.2)

        # Then we put the clock back an hour for the final two records
        last_ts = datetime.datetime.combine(
            records[-1].dateStamp, records[-1].timeStamp)
        records += TestDmpProcedure._make_archive_records(
            last_ts - datetime.timedelta(hours=3),
            5, 7, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:10]
        )
        page_3 = build_page(
            3,
            encoded_records[10:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())
        proc.data_received(page_3)
        self.assertEqual(b'\x06', recv.read())

        self.assertEqual(13, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_handling_of_null_record_before_end(self):
        # What happens if a null record is encountered in a page that isn't the
        # final page? This probably isn't a scenario that is possible with a
        # real console.

        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(3, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 8, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Stick a null record in slot 4 of the middle page
        encoded_records.append(b'\xFF' * 52)

        last_ts = datetime.datetime.combine(
            records[-1].dateStamp, records[-1].timeStamp)
        records_2 = TestDmpProcedure._make_archive_records(
            # +10 minutes to account for the null page
            last_ts + datetime.timedelta(minutes=10),
            5, 6, 0.2)
        encoded_records += [serialise_dmp(r) for r in records_2]
        records += records_2

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:10]
        )
        page_3 = build_page(
            3,
            encoded_records[10:]
        )

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())
        proc.data_received(page_3)
        self.assertEqual(b'\x06', recv.read())

        self.assertEqual(12, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_bad_page_crc_detected(self):
        # We should get a NAK back asking to retransmit.
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(3, 2))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 15, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # Builds a page complete with CRC. Handy!
        page_1 = build_page(
            1,  # Sequence
            encoded_records[0:5])
        page_2 = build_page(
            2,
            encoded_records[5:10]
        )
        page_3 = build_page(
            3,
            encoded_records[10:]
        )

        # Corrupt page 2:
        bad_page_2 = bytearray()
        bad_page_2.extend(page_2[0:10])
        bad_page_2.extend(b"Hello, World!")
        bad_page_2.extend(page_2[23:])

        proc.data_received(page_1)
        self.assertEqual(b'\x06', recv.read())

        # Send the corrupted page. We should get ! back to indicate its garbage
        proc.data_received(bad_page_2)
        self.assertEqual(b'!', recv.read())

        # Send the uncorrupted page
        proc.data_received(page_2)
        self.assertEqual(b'\x06', recv.read())

        proc.data_received(page_3)
        self.assertEqual(b'\x06', recv.read())

        self.assertEqual(13, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i + 2], proc.ArchiveRecords[i], i)

    def test_full_logger_download(self):
        # Check it properly handles a data logger full of data that needs
        # downloading in its entirety.
        recv = WriteReceiver()
        log = LogReceiver()
        fd = FinishedDetector()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.2)
        proc.finished += fd.finished

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        # The logger stores 512 pages of data. A Full download sends the first
        # page twice (once at the start and again at the end). So total pages
        # that will be sent is 513.
        proc.data_received(TestDmpProcedure._page_count(513, 2))

        # The logger stores up to 2560 records in 512 pages.
        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 2560, 0.2)
        encoded_records = [serialise_dmp(r) for r in records]

        # The first two records in the first page transmitted are 'old' records.
        # So we need to grab these from the end of the record set and move them
        # to the start.
        encoded_records.insert(0, encoded_records.pop(len(encoded_records) - 1))
        encoded_records.insert(0, encoded_records.pop(len(encoded_records) - 1))

        pages = []
        start_page = 5
        current_page = start_page
        while len(encoded_records) > 0:
            page_records = []
            while len(page_records) < 5:
                page_records.append(encoded_records.pop(0))

            pages.append(build_page(current_page, page_records))
            current_page += 1
            if current_page > 255:
                current_page = 0

        # The final page transmitted is a repeat of the first page. So:
        pages.append(pages[0])

        self.assertFalse(fd.IsFinished)

        for page in pages:
            proc.data_received(page)
            self.assertEqual(b'\x06', recv.read())

        self.assertEqual(2560, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i], proc.ArchiveRecords[i], i)

        self.assertTrue(fd.IsFinished)

    def test_point_01_mm_rain_size(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.1)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(1, 0))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 5, 0.1)
        encoded_records = [serialise_dmp(r, 0.1) for r in records]

        # Builds a page complete with CRC. Handy!
        page = build_page(
            1,  # Sequence
            encoded_records)

        proc.data_received(page)

        self.assertEqual(5, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i], proc.ArchiveRecords[i], i)

    def test_point_01_inch_rain_size(self):
        recv = WriteReceiver()
        log = LogReceiver()

        proc = DmpProcedure(recv.write, log.log,
                            datetime.datetime(2021, 8, 18, 20, 42),
                            0.254)

        proc.start()
        self.assertEqual(b'DMPAFT\n', recv.Data)
        proc.data_received(b'\x06')  # ACK
        # Receive: '\x12+\xfa\x07\x0c\x15' (timestamp)
        proc.data_received(b'\x06')  # ACK

        proc.data_received(TestDmpProcedure._page_count(1, 0))

        records = TestDmpProcedure._make_archive_records(
            datetime.datetime(2021, 8, 18, 20, 45), 5, 5, 0.254)
        encoded_records = [serialise_dmp(r, 0.254) for r in records]

        # Builds a page complete with CRC. Handy!
        page = build_page(
            1,  # Sequence
            encoded_records)

        proc.data_received(page)

        self.assertEqual(5, len(proc.ArchiveRecords))
        for i in range(0, len(proc.ArchiveRecords)):
            self._assertDmpEqual(records[i], proc.ArchiveRecords[i], i)

    def test_null_dmp_round_trip(self):
        ts = datetime.datetime(2021, 8, 18, 20, 42)

        # Specs only list the date stamp and time stamp fields as having no
        # 'dashed' value. We'll take that to mean everything else could come
        # back null. The deserialiser however doesn't always deserialise dashed
        # values to None for the purpose of making the database easy to deal
        # with. These situations are noted below.
        expected = Dmp(
            dateStamp=ts.date(),
            timeStamp=ts.time(),
            timeZone=None,
            dateInteger=encode_date(ts.date()),
            timeInteger=encode_time(ts.time()),
            outsideTemperature=None,
            highOutsideTemperature=None,
            lowOutsideTemperature=None,
            rainfall=0,  # Null is deserialised to 0
            highRainRate=0,  # Null is deserialised to 0
            barometer=0,  # Null is deserialised to 0
            solarRadiation=None,
            numberOfWindSamples=0,  # Null is deserialised to 0
            insideTemperature=None,
            insideHumidity=None,
            outsideHumidity=None,
            averageWindSpeed=None,
            highWindSpeed=0,  # Null is deserialised to 0
            highWindSpeedDirection=None,
            prevailingWindDirection=None,
            averageUVIndex=None,
            ET=None,
            highSolarRadiation=None,
            highUVIndex=0,  # Null is deserialised to 0
            forecastRule=193,  # Null is deserialised to 193 (a specific rule)
            leafTemperature=[
                None,
                None
            ],
            leafWetness=[
                None,
                None
            ],
            soilTemperatures=[
                None,
                None,
                None,
                None
            ],
            extraHumidities=[
                None,
                None
            ],
            extraTemperatures=[
                None,
                None,
                None
            ],
            soilMoistures=[
                None,
                None,
                None,
                None
            ])

        null_dmp = Dmp(
            dateStamp=ts.date(),
            timeStamp=ts.time(),
            timeZone=None,
            dateInteger=encode_date(ts.date()),
            timeInteger=encode_time(ts.time()),
            outsideTemperature=None,
            highOutsideTemperature=None,
            lowOutsideTemperature=None,
            rainfall=None,
            highRainRate=None,
            barometer=None,
            solarRadiation=None,
            numberOfWindSamples=None,
            insideTemperature=None,
            insideHumidity=None,
            outsideHumidity=None,
            averageWindSpeed=None,
            highWindSpeed=None,
            highWindSpeedDirection=None,
            prevailingWindDirection=None,
            averageUVIndex=None,
            ET=None,
            highSolarRadiation=None,
            highUVIndex=None,
            forecastRule=None,
            leafTemperature=[
                None,
                None
            ],
            leafWetness=[
                None,
                None
            ],
            soilTemperatures=[
                None,
                None,
                None,
                None
            ],
            extraHumidities=[
                None,
                None
            ],
            extraTemperatures=[
                None,
                None,
                None
            ],
            soilMoistures=[
                None,
                None,
                None,
                None
            ])

        serialised = serialise_dmp(null_dmp)
        deserialised = deserialise_dmp(serialised)

        self._assertDmpEqual(expected, deserialised, 0)


# TODO: Query console for enabled sensors & any other useful config
# TODO: Check test coverage for everything
# TODO: check CRC error handling for all procedures except DMP (it already handles it)
# TODO: LOOP
# TODO: DavisWeatherStation
