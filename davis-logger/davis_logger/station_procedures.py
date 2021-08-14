# coding=utf-8
"""
Various procedures for interacting with davis weather stations
"""
import struct
import datetime
from twisted.python import log
from davis_logger.record_types.util import CRC
from davis_logger.util import Event, to_hex_string

__author__ = 'david'


class Procedure(object):
    """
    Super class for procedures.
    """

    _STATE_READY = 0
    _ACK = '\x06'

    def __init__(self, write_callback):
        self.finished = Event()
        self._buffer = ""
        self._handlers = []
        self._state = self._STATE_READY
        self._write = write_callback

    def data_received(self, data):
        """
        Pass data into the procedure.
        :param data: Data from weather station
        :type data: str
        """
        self._buffer += data

        handler = self._handlers[self._state]
        if handler is not None:
            handler()

    def _complete(self):
        self.finished.fire()


class DstSwitchProcedure(Procedure):
    """
    Handles the process of turning daylight savings on or off and adjusting
    the clock as required.
    """

    _STATE_SEND_NEW_EEPROM_VALUE = 1
    _STATE_GET_CURRENT_TIME = 2
    _STATE_CALCULATE_NEW_TIME = 3
    _STATE_SET_NEW_TIME = 4
    _STATE_FINISHED = 5
    _STATE_COMPLETE = 6

    def __init__(self, write_callback, new_dst_value):
        """

        :param write_callback: Function to send data to the weather station
        :type write_callback: callable
        :param new_dst_value: If DST should be turned on or off
        :type new_dst_value: bool
        """
        super(DstSwitchProcedure, self).__init__(write_callback)

        self._handlers = {
            self._STATE_READY: None,
            self._STATE_SEND_NEW_EEPROM_VALUE: self._send_new_eeprom_value,
            self._STATE_GET_CURRENT_TIME: self._get_current_time,
            self._STATE_CALCULATE_NEW_TIME: self._calculate_new_time,
            self._STATE_SET_NEW_TIME: self._set_new_time,
            self._STATE_FINISHED: self._finished,
        }

        self._new_dst_value = new_dst_value

    def _transition(self, data=None):
        # Completely linear!
        self._state += 1
        self._buffer = ""

        if data is not None:
            self._write(data)

    def start(self):
        """
        Starts the procedure
        """
        log.msg("Started changing DST setting")
        self._start_eeprom_write()

    def _start_eeprom_write(self):
        # Basic procedure to set DST:
        # > EEBWR 13 1   <-- This function
        # < ACK
        # > data
        # < ACK
        # > GETTIME
        # < ACK
        # < data
        # > SETTIME
        # < ACK
        # > data
        # < ACK

        self._transition("EEBWR 13 1\n")

    def _send_new_eeprom_value(self):

        if len(self._buffer) < 1:
            return  # Wait for data.

        # Basic procedure to set DST:
        # > EEBWR 13 1
        # < ACK          <-- This function
        # > data         <-- This function
        # < ACK
        # > GETTIME
        # < ACK
        # < data
        # > SETTIME
        # < ACK
        # > data
        # < ACK

        assert self._buffer[0] == self._ACK

        if self._new_dst_value:
            data = chr(1)
        else:
            data = chr(0)

        crc = CRC.calculate_crc(data)

        packed_crc = struct.pack(CRC.FORMAT, crc)

        data += packed_crc

        self._transition(data)

    def _get_current_time(self):

        if len(self._buffer) < 1:
            return  # Wait for data.

        # Basic procedure to set DST:
        # > EEBWR 13 1
        # < ACK
        # > data
        # < ACK          <-- This function
        # > GETTIME      <-- This function
        # < ACK
        # < data
        # > SETTIME
        # < ACK
        # > data
        # < ACK

        assert self._buffer[0] == self._ACK

        self._transition("GETTIME\n")

    def _calculate_new_time(self):

        if len(self._buffer) < 9:
            return  # Not enough data yet. Wait for more.

        # Basic procedure to set DST:
        # > EEBWR 13 1
        # < ACK
        # > data
        # < ACK
        # > GETTIME
        # < ACK          <-- This function
        # < data         <-- This function
        # > SETTIME      <-- This function
        # < ACK
        # > data
        # < ACK

        assert self._buffer[0] == self._ACK

        packet_crc = struct.unpack(CRC.FORMAT, self._buffer[7:9])[0]
        calculated_crc = CRC.calculate_crc(self._buffer[1:7])

        if packet_crc != calculated_crc:
            log.msg("CRC Error: {0} != {1}".format(packet_crc, calculated_crc))
            assert packet_crc != calculated_crc

        seconds = ord(self._buffer[1])
        minutes = ord(self._buffer[2])
        hour = ord(self._buffer[3])
        day = ord(self._buffer[4])
        month = ord(self._buffer[5])
        year = ord(self._buffer[6]) + 1900

        self._new_time = datetime.datetime(year=year, month=month, day=day,
                                           hour=hour, minute=minutes,
                                           second=seconds)

        if self._new_dst_value is True:
            # Turning DST ON. Put clock forward
            self._new_time += datetime.timedelta(hours=1)
        else:
            # Turning DST OFF. Put clock back
            self._new_time -= datetime.timedelta(hours=1)

        self._transition("SETTIME\n")

    def _set_new_time(self):

        if len(self._buffer) < 1:
            return  # Wait for data.

        # Basic procedure to set DST:
        # > EEBWR 13 1
        # < ACK
        # > data
        # < ACK
        # > GETTIME
        # < ACK
        # < data
        # > SETTIME
        # < ACK          <-- This function
        # > data         <-- This function
        # < ACK          <-- This function

        assert self._buffer[0] == self._ACK

        seconds = chr(self._new_time.second)
        minutes = chr(self._new_time.minute)
        hour = chr(self._new_time.hour)
        day = chr(self._new_time.day)
        month = chr(self._new_time.month)
        year = chr(self._new_time.year - 1900)

        data = seconds + minutes + hour + day + month + year
        data += struct.pack(CRC.FORMAT, CRC.calculate_crc(data))

        self._transition(data)

    def _finished(self):

        if len(self._buffer) < 1:
            return  # Wait for data.

        # Basic procedure to set DST:
        # > EEBWR 13 1
        # < ACK
        # > data
        # < ACK
        # > GETTIME
        # < ACK
        # < data
        # > SETTIME
        # < ACK
        # > data
        # < ACK          <-- This function

        assert self._buffer[0] == self._ACK
        log.msg("DST changed.")
        self._state = self._STATE_READY
        self._complete()
