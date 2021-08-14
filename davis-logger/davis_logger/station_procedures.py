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

    _STATE_READY = 0  # Type: int
    _ACK = b'\x06'  # Type: str

    def __init__(self, write_callback):
        """
        Constructs a new procedure
        :param write_callback: Function that receives data from the procedure
                and sends it to the weather station
        :type write_callback: Callable
        """
        self.finished = Event()
        self._buffer = bytearray()
        self._handlers = []
        self._state = self._STATE_READY
        self._write = write_callback

    def data_received(self, data):
        """
        Pass data into the procedure.
        :param data: Data from weather station
        :type data: bytes
        """
        self._buffer += data

        handler = self._handlers[self._state]
        if handler is not None:
            handler()

    def start(self):
        """
        Starts the procedure.
        """
        pass

    def _complete(self):
        """
        Call when the procedure is complete to fire the finished event.
        """
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
        """
        Send the specified data to the station then transition to the next state
        :param data: Data to send
        :type data: bytes
        """
        # Completely linear!
        self._state += 1
        self._buffer = bytearray()

        if data is not None:
            self._write(data)

    def start(self):
        """
        Starts the procedure
        """
        log.msg("Started changing DST setting")
        self._start_eeprom_write()

    def _start_eeprom_write(self):
        """
        Step 01: Starts the EEPROM write process to switch the DST setting on
        or off
        """
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

        self._transition(b"EEBWR 13 1\n")

    def _send_new_eeprom_value(self):
        """
        Step 02: Receives the ACK from the station acknowledging the EEPROM
        Write command. Then it sends the new data to be written to the EEPROM.
        """
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

        assert self._buffer[0:1] == self._ACK

        data = bytearray()

        if self._new_dst_value:
            data.append(1)
        else:
            data.append(0)

        crc = CRC.calculate_crc(data)

        packed_crc = struct.pack(CRC.FORMAT, crc)

        data.extend(packed_crc)

        self._transition(data)

    def _get_current_time(self):
        """
        Step 03: Receives the ACK from the station indicating the EEPROM write
        data was successfully received and committed, then asks the station for
        the current time.
        """
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

        assert self._buffer[0:1] == self._ACK

        self._transition(b"GETTIME\n")

    def _calculate_new_time(self):
        """
        Step 04: Receives the ACK from the station indicating it received our
        GETTIME request, then receives the current time from the station. It
        then calculates what the new time should be given the new DST setting
        and issues a SETTIME command to the station to begin the clock update
        process.
        """
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

        assert self._buffer[0:1] == self._ACK

        packet_crc = struct.unpack(CRC.FORMAT, self._buffer[7:9])[0]
        calculated_crc = CRC.calculate_crc(self._buffer[1:7])

        if packet_crc != calculated_crc:
            log.msg("CRC Error: {0} != {1}".format(packet_crc, calculated_crc))
            assert packet_crc != calculated_crc

        seconds = self._buffer[1]
        minutes = self._buffer[2]
        hour = self._buffer[3]
        day = self._buffer[4]
        month = self._buffer[5]
        year = self._buffer[6] + 1900

        self._new_time = datetime.datetime(year=year, month=month, day=day,
                                           hour=hour, minute=minutes,
                                           second=seconds)

        if self._new_dst_value is True:
            # Turning DST ON. Put clock forward
            self._new_time += datetime.timedelta(hours=1)
        else:
            # Turning DST OFF. Put clock back
            self._new_time -= datetime.timedelta(hours=1)

        self._transition(b"SETTIME\n")

    def _set_new_time(self):
        """
        Step 05: Receives an ACK from the station indicating it received the
        SETTIME command then sends the new time
        """
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
        # < ACK

        assert self._buffer[0:1] == self._ACK

        data = bytearray()
        data.append(self._new_time.second)
        data.append(self._new_time.minute)
        data.append(self._new_time.hour)
        data.append(self._new_time.day)
        data.append(self._new_time.month)
        data.append(self._new_time.year - 1900)

        data.extend(struct.pack(CRC.FORMAT, CRC.calculate_crc(data)))

        self._transition(data)

    def _finished(self):
        """
        Step 06: Receives the confirmation ACK from the station indicating the
        clock has been changed then completes the procedure.
        """
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

        assert self._buffer[0:1] == self._ACK
        log.msg("DST changed.")
        self._state = self._STATE_READY
        self._complete()
