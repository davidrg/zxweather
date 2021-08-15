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
    Super class for procedures. A procedure represents a particular interaction
    with the station hardware. It has its own receive buffer which is used to
    receive data from the station and it is able to send data back to the
    station. Once complete it is expected to fire the finished event.
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
        self._str_buffer = ''
        self._handlers = []
        self._state = self._STATE_READY
        self._write = write_callback
        self.Name = "unknown procedure"

    def data_received(self, data):
        """
        Pass data into the procedure.
        :param data: Data from weather station
        :type data: bytes
        """
        self._buffer.extend(data)

        if isinstance(data, str):
            self._str_buffer += data
        else:
            self._str_buffer += str(data)

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


class SequentialProcedure(Procedure):
    """
    A procedure that works through its states sequentially.
    """

    def _transition(self, data=None):
        """
        Send the specified data to the station then transition to the next state
        :param data: Data to send
        :type data: bytes
        """
        # Completely linear!
        self._state += 1
        self._buffer = bytearray()
        self._str_buffer = ''

        if data is not None:
            self._write(data)


class DstSwitchProcedure(SequentialProcedure):
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
        self.Name = "DST Switch (to {0})".format(new_dst_value)

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


class GetConsoleInformationProcedure(SequentialProcedure):
    """
    Gets basic station information: Hardware type, firmware date and (if
    possbile) firmware version. It also indicates if, in the case of a Vantage
    Pro2, the firmware supports the LPS command and if it is new enough to be
    upgraded through software (rather than the special firmware upgrade device)
    """

    _STATE_CONSOLE_TYPE_REQUEST = 1
    _STATE_RECEIVE_CONSOLE_TYPE = 2
    _STATE_RECEIVE_VERSION_DATE = 3
    _STATE_RECEIVE_VERSION_NUMBER = 4
    _STATE_COMPLETE = 6

    _MONTHS = [
        'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct',
        'Nov', 'Dec'
    ]

    def __init__(self, write_callback):
        """
        :param write_callback: Function to send data to the weather station
        :type write_callback: callable
        :param new_dst_value: If DST should be turned on or off
        :type new_dst_value: bool
        """
        super(GetConsoleInformationProcedure, self).__init__(write_callback)

        self._handlers = {
            self._STATE_READY: None,
            self._STATE_CONSOLE_TYPE_REQUEST: self._send_console_type_request,
            self._STATE_RECEIVE_CONSOLE_TYPE: self._receive_console_type,
            self._STATE_RECEIVE_VERSION_DATE: self._receive_version_date,
            self._STATE_RECEIVE_VERSION_NUMBER: self._receive_version_number
        }

        self.hw_type = None  # Type: str
        self.version_date = None  # Type: str
        self.version_date_d = None  # Type: datetime.date
        self.version = None  # Type: str
        self.lps_supported = None  # Type: bool
        self.self_firmware_upgrade_supported = None  # Type: bool
        self.Name = "Get console type & firmware version"

    def start(self):
        self.hw_type = "Unknown"
        self._state = self._STATE_CONSOLE_TYPE_REQUEST
        self._send_console_type_request()

    def _send_console_type_request(self):
        self._transition(b'WRD\x12\x4D\n')

    def _receive_console_type(self):
        if len(self._buffer) == 2:
            assert self._buffer[0:1] == self._ACK

            self.station_type = self._buffer[1]
            self._buffer = bytearray()

            self.hw_type = "Unknown"
            if self.station_type == 0:
                self.hw_type = "Wizard III"
            elif self.station_type == 1:
                self.hw_type = "Wizard II"
            elif self.station_type == 2:
                self.hw_type = "Monitor"
            elif self.station_type == 3:
                self.hw_type = "Perception"
            elif self.station_type == 4:
                self.hw_type = "GroWeather"
            elif self.station_type == 5:
                self.hw_type = "Energy Enviromonitor"
            elif self.station_type == 6:
                self.hw_type = "Health Enviromonitor"
            elif self.station_type == 16:
                self.hw_type = "Vantage Pro, Vantage Pro2"
            elif self.station_type == 17:
                self.hw_type = "Vantage Vue"

            self._transition(b'VER\n')

    def _receive_version_date(self):
        if self._buffer.count(b'\n') == 3:
            str_buffer = self._buffer.decode('ascii')
            self.version_date = str_buffer.split('\n')[2].strip()
            self._buffer = bytearray()

            try:
                bits = self.version_date.split(" ")
                month_name = bits[0]
                day = int(bits[1])
                year = int(bits[2])
                month = self._MONTHS.index(month_name) + 1
                self.version_date_d = datetime.date(year=year, month=month, day=day)

                v190_date = datetime.date(year=2009, month=12, day=31)
                ancient_fw_date = datetime.date(year=2005, month=11, day=28)

                # Need firmware v1.90 (dated 31-DEC-2009) to support LPS
                self.lps_supported = self.version_date_d >= v190_date
                self.self_firmware_upgrade_supported = self.version_date_d >= ancient_fw_date
            except:
                self.version_date_d = None
                self.lps_supported = False
                self.self_firmware_upgrade_supported = None  # Don't know

            if self.station_type in [16, 17] and self.lps_supported:
                # NVER is only supported on the Vantage Vue or Vantage Pro2
                self._transition(b'NVER\n')
            else:
                # Done!
                self._state = self._STATE_READY
                self._complete()

    def _receive_version_number(self):
        if self._buffer.count(b'\n') == 3:
            self.version = self._buffer.decode('ascii').split('\n')[2].strip()
            self._state = self._STATE_READY
            self._complete()
