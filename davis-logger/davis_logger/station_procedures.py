# coding=utf-8
"""
Various procedures for interacting with davis weather stations
"""
import struct
import datetime
from twisted.python import log

from davis_logger.record_types.dmp import encode_date, encode_time, split_page, deserialise_dmp
from davis_logger.record_types.util import CRC
from davis_logger.util import Event

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

    def __init__(self, write_callback, log_callback=None):
        """
        Constructs a new procedure
        :param write_callback: Function that receives data from the procedure
                and sends it to the weather station
        :type write_callback: Callable
        :param log_callback: Function to output log messages
        :type log_callback: Callable
        """
        self.finished = Event()
        self._buffer = bytearray()
        self._str_buffer = ''
        self._handlers = []
        self._state = self._STATE_READY
        self._write = write_callback
        self.Name = "unknown procedure"

        if log_callback is not None:
            self._log = log_callback
        else:
            def log(msg):
                """Default no-op log function"""
                pass
            self._log = log

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
        """
        Starts the procedure
        """
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
                self.lps_supported = True
                self.self_firmware_upgrade_supported = True

            self._transition(b'VER\n')

    def _receive_version_date(self):
        if self._buffer.count(b'\n') == 3:
            str_buffer = self._buffer.decode('ascii')
            self.version_date = str_buffer.split('\n')[2].strip()
            self._buffer = bytearray()

            bits = self.version_date.split(" ")
            month_name = bits[0]
            day = int(bits[1])
            year = int(bits[2])
            month = self._MONTHS.index(month_name) + 1
            self.version_date_d = datetime.date(year=year, month=month, day=day)

            if self.lps_supported is None:
                v190_date = datetime.date(year=2009, month=12, day=31)
                ancient_fw_date = datetime.date(year=2005, month=11, day=28)

                # Need firmware v1.90 (dated 31-DEC-2009) to support LPS
                self.lps_supported = self.version_date_d >= v190_date
                self.self_firmware_upgrade_supported = self.version_date_d >= ancient_fw_date

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


class GetConsoleConfigurationProcedure(SequentialProcedure):
    """
    Gets basic station console configuration: rain gauge measurement size,
    archive interval, DST setting and current clock time.
    """

    _STATE_READY = 1
    _STATE_RECEIVE_TIME = 2
    _STATE_RECEIVE_RAIN_SIZE = 3
    _STATE_RECEIVE_ARCHIVE_INTERVAL = 4
    _STATE_RECEIVE_AUTO_DST_ENABLED = 5
    _STATE_RECEIVE_MANUAL_DST_STATUS = 6

    def __init__(self, write_callback):
        super(GetConsoleConfigurationProcedure, self).__init__(write_callback)

        self.CurrentStationTime = None

        self.RainSizeString = "Unknown"
        self.RainSizeMM = None
        self.ArchiveIntervalMinutes = None

        # If daylight savings mode is automatic or manual
        self.AutoDSTEnabled = None

        # If Daylight Savings Time is on or off
        self.DSTOn = None

        self.Name = "Get console configuration"

        self._handlers = {
            self._STATE_READY: None,
            self._STATE_RECEIVE_TIME: self._receive_time,
            self._STATE_RECEIVE_RAIN_SIZE: self._receive_rain_size,
            self._STATE_RECEIVE_ARCHIVE_INTERVAL: self._receive_archive_interval,
            self._STATE_RECEIVE_AUTO_DST_ENABLED: self._receive_auto_dst_enabled,
            self._STATE_RECEIVE_MANUAL_DST_STATUS: self._daylight_savings_status_received
        }

    def start(self):
        """
        Starts the procedure
        """
        self._transition(b'GETTIME\n')

    def _decodeTimeInBuffer(self):
        seconds = self._buffer[1]
        minutes = self._buffer[2]
        hour = self._buffer[3]
        day = self._buffer[4]
        month = self._buffer[5]
        year = self._buffer[6] + 1900
        return datetime.datetime(year=year, month=month, day=day, hour=hour,
                                 minute=minutes, second=seconds)

    def _receive_time(self):
        if len(self._buffer) == 9:
            assert self._buffer[0:1] == self._ACK
            self.CurrentStationTime = self._decodeTimeInBuffer()

            self._buffer = bytearray()
            self._transition(b'EEBRD 2B 01\n')

    def _receive_rain_size(self):
        if len(self._buffer) == 4:
            assert self._buffer[0:1] == self._ACK

            setup_byte = self._buffer[1]
            rainCollectorSize = (setup_byte & 0x30) >> 4
            self.RainSizeString = "Unknown - assuming 0.2mm"
            self.RainSizeMM = 0.2
            if rainCollectorSize == 0:
                self.RainSizeString = "0.01 Inches"
                self.RainSizeMM = 0.254
            elif rainCollectorSize == 1:
                self.RainSizeString = "0.2mm"
                self.RainSizeMM = 0.2
            elif rainCollectorSize == 2:
                self.RainSizeString = "0.1mm"
                self.RainSizeMM = 0.1

            self._buffer = bytearray()
            self._transition(b"EEBRD 2D 01\n")

    def _receive_archive_interval(self):
        if len(self._buffer) == 4:
            assert self._buffer[0:1] == self._ACK

            self.ArchiveIntervalMinutes = self._buffer[1]

            self._buffer = bytearray()
            self._transition(b"EEBRD 12 1\n")

    def _receive_auto_dst_enabled(self):
        if len(self._buffer) == 4:
            assert self._buffer[0:1] == self._ACK

            packet_crc = struct.unpack(CRC.FORMAT, self._buffer[2:])[0]
            calculated_crc = CRC.calculate_crc(bytes(self._buffer[1:2]))

            assert packet_crc == calculated_crc

            result = self._buffer[1]
            if result == 1:
                self.AutoDSTEnabled = False
            elif result == 0:
                self.AutoDSTEnabled = True
                self._complete()
                return

            # Auto DST is off, DST is being done manually. Check to see if
            # DST is turned on or off.
            self._buffer = bytearray()
            self._transition(b"EEBRD 13 1\n")

    def _daylight_savings_status_received(self):
        if len(self._buffer) == 4:
            assert self._buffer[0:1] == self._ACK

            packet_crc = struct.unpack(CRC.FORMAT, self._buffer[2:])[0]
            calculated_crc = CRC.calculate_crc(bytes(self._buffer[1:2]))

            assert packet_crc == calculated_crc

            result = self._buffer[1]

            if result == 1:
                self.DSTOn = True
            else:
                self.DSTOn = False

            self._complete()


class DmpProcedure(SequentialProcedure):
    """
    Retrieves archive records from the console starting from the specified
    time.
    """
    _STATE_READY = 1
    _STATE_SEND_START_TIME = 2
    _STATE_RECEIVE_PAGE_COUNT = 3
    _RECEIVE_ARCHIVE_RECORDS = 4

    # The console uses '!' as a NAK character to signal invalid parameters.
    _NAK = b'\x21'  # TODO: Some other software uses \x15 here claiming the
                   # documentation is wrong. Further investigation required.

    # Used to signal to the console that its response did not pass the CRC
    # check. Sending this back will make the console resend its last message.
    _CANCEL = b'\x18'

    # Used to cancel the download of further DMP packets.
    _ESC = b'\x1B'

    def __init__(self, write_callback, log_callback, from_time,
                 rain_collector_size):
        """
        Fetches archive records from the station console/envoy
        :param write_callback: Callback to send data to the station
        :type write_callback: Callable
        :param log_callback: Callback to produce log messages
        :type log_callback: Callable
        :param from_time: Timestamp for first record to download
        :type from_time: datetime.datetime
        :param rain_collector_size: Rain collector size in millimeters
        :type rain_collector_size: float
        """
        super(DmpProcedure, self).__init__(write_callback, log_callback)

        self.ArchiveRecords = None
        self._from_time = from_time
        self._rain_collector_size = rain_collector_size
        self._max_dst_offset = datetime.timedelta(hours=2)

        self.Name = "Get all archive records since: {0}".format(from_time)

        self._handlers = {
            self._STATE_READY: None,
            self._STATE_SEND_START_TIME: self._send_start_time,
            self._STATE_RECEIVE_PAGE_COUNT: self._receive_page_count,
            self._RECEIVE_ARCHIVE_RECORDS: self._receive_archive_records
        }

    def start(self):
        """Starts the procedure."""
        self._transition(b'DMPAFT\n')

    def _send_start_time(self):
        if self._buffer != self._ACK:
            self._log('Warning: Expected ACK')
            # TODO: What should we do here? Retry? Signal failure?
            return

        assert(isinstance(self._from_time, datetime.datetime))
            # Now we send the timestamp.
        #if isinstance(self._dmp_timestamp, datetime.datetime):
        date_stamp = encode_date(self._from_time.date())
        time_stamp = encode_time(self._from_time.time())
        packed = struct.pack('<HH', date_stamp, time_stamp)
        # else:
        #     packed = struct.pack('<HH',
        #                          self._dmp_timestamp[0],
        #                          self._dmp_timestamp[1])

        self._buffer = bytearray()
        crc = CRC.calculate_crc(packed)
        packed += struct.pack(CRC.FORMAT, crc)
        self._transition(packed)

    def _receive_page_count(self):
        if self._buffer[0:1] == self._CANCEL:
            # CRC check failed. Try again.
            self._log("CRC check failed - retrying.")
            self.start()
            return
        elif self._buffer[0:1] == self._NAK:
            # The manuals says this happens if we didn't send a full
            # timestamp+CRC. This should never happen. I hope.
            self._log("NAK received, DMPAFT state 2 - retrying.")
            self.start()
            return

        if len(self._buffer) < 7:
            return

        # Consume the ACK
        assert self._buffer[0:1] == self._ACK
        self._buffer = self._buffer[1:]

        payload = self._buffer[0:4]

        crc = struct.unpack(CRC.FORMAT, self._buffer[4:])[0]
        calculated_crc = CRC.calculate_crc(payload)  # Skip over the ACK

        if crc != calculated_crc:
            self._log('CRC mismatch for DMPAFT page count')
            # I assume we just start again from scratch if this particular CRC
            # is bad. Thats what we have to do for the other one at least.
            self.start()
            return

        page_count, first_record_location = struct.unpack('<HH', payload)

        self._log('Pages: {0}, First Record: {1}'.format(
            page_count, first_record_location))

        self._dmp_page_count = page_count
        self._dmp_remaining_pages = page_count
        self._dmp_first_record = first_record_location
        self._buffer = bytearray()
        self._dmp_records = []

        if page_count > 0:
            self._transition()

        # Request the console start streaming data to us (at this point we
        # could instead cancel the download by sending 0x1B)
        self._write(self._ACK)

        if page_count <= 0:
            # Let everyone know we're done.
            self._complete()

    def _receive_archive_records(self):

        if len(self._buffer) >= 267:
            page = self._buffer[0:267]
            self._buffer = self._buffer[267:]
            page_number, records, crc = split_page(page)
            if crc == CRC.calculate_crc(page[0:265]):
                # CRC checks out.

                if self._dmp_remaining_pages == self._dmp_page_count:
                    # Skip any 'old' records if the first record of the dump
                    # isn't also the first record of the first page.
                    self._dmp_records += records[self._dmp_first_record:]
                else:
                    self._dmp_records += records
                self._dmp_remaining_pages -= 1

                # Ask for the next page.
                self._write(self._ACK)

                # If there are no more pages coming then its time to process
                # it all and reset our state.
                if self._dmp_remaining_pages == 0:
                    self._state = self._STATE_READY
                    self._process_dmp_records()
            else:
                log.msg('CRC Failed')
                # CRC failed. Ask for the page to be sent again.
                self._write(self._NAK)

    def _process_dmp_records(self):
        decoded_records = []
        last_ts = None

        for index, record in enumerate(self._dmp_records):
            decoded = deserialise_dmp(record, self._rain_collector_size)

            if decoded.dateStamp is None or decoded.timeStamp is None:
                # An empty record indicates we've gone past the last record
                # in archive memory (which has been cleared sometime recently).
                # We should just discard it and not bother looking at the rest.
                if index >= len(self._dmp_records) - 5:
                    break
                else:
                    # Except we're not in the final page of data! The station
                    # thinks there is still more 'new' data for us to receive.
                    # This should never happen - are we actually talking to
                    # a real Vantage console or envoy? We'll just throw this
                    # record away and continue processing just in case.
                    self._log(
                        "WARNING: Encountered an empty record before the final "
                        "page of data. This should not happen. Empty record "
                        "will be ignored. Check database to ensure any further"
                        " data received is valid.")
                    continue

            decoded_ts = datetime.datetime.combine(decoded.dateStamp,
                                                   decoded.timeStamp)

            if last_ts is None:
                last_ts = decoded_ts

            # We will ignore it if the time jumps back a little bit as its
            # probably just daylight savings ending.
            if decoded_ts < (last_ts - self._max_dst_offset):
                # The clock has jumped back more than we'd expect for a daylight
                # savings change. We've likely gone past the last new record
                # in the archive memory and now we're looking at old data.

                if index >= len(self._dmp_records) - 5:
                    # We're within the final page of data so yep, its old data.
                    # We'll just discard it and not bother looking at the rest.
                    break
                else:
                    # Nope - we're not in the final page. The weather station
                    # still thinks there is more 'new' data for us to download.
                    # probably the user just put the clock back. We'll ignore
                    # it.
                    self._log(
                        "Time for record {0}/{4} is set back {1} from previous "
                        "record ({2} to {3}). This usually signals end of data "
                        "but that should only occur within the final five "
                        "records received from the station. Perhaps the user "
                        "has set the clock back? We'll just ignore this."
                        .format(index,
                                decoded_ts - last_ts,
                                last_ts,
                                decoded_ts,
                                len(self._dmp_records)))

            # Record seems OK. Add it to the list and continue on.
            last_ts = decoded_ts
            decoded_records.append(decoded)

        self.ArchiveRecords = decoded_records
        self._complete()
