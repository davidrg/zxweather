# coding=utf-8
"""
The Davis module: Code for interfacing with a Davis weather station. The
Vantage Vue in particular (as I don't have access to anything else to test
with).
"""
import struct
import datetime
from twisted.python import log
from davis_logger.record_types.dmp import encode_date, encode_time, \
    split_page, deserialise_dmp
from davis_logger.record_types.loop import deserialise_loop
from davis_logger.record_types.util import CRC
from davis_logger.station_procedures import DstSwitchProcedure
from davis_logger.util import Event, toHexString

__author__ = 'david'

# Console is asleep and needs waking
STATE_SLEEPING = 0

# Console is awake and ready to receive commands. Note that this is only valid
# for two minutes from the last character sent to the console so act fast.
STATE_AWAKE = 1

# When the console is busy executing the LPS command. To abort run the wake up
# procedure.
STATE_LPS = 2

# First stage of executing DMPAFT: waiting for the ACK
STATE_DMPAFT_INIT_1 = 3

# Second stage of executing DMPAFT: timestamp sent, waiting for ACK, etc
STATE_DMPAFT_INIT_2 = 4

# Receiving dump pages.
STATE_DMPAFT_RECV = 5

# init-1: Getting station type
INIT_1_STATE_GET_STATION_TYPE = 6

# init-2: Get version date
INIT_2_GET_VERSION_DATE = 7

# init-3: Get version
INIT_3_GET_VERSION = 8

# init-4: Get time
INIT_4_GET_TIME = 9

# init-5: Get rain collector size.
INIT_5_GET_RAIN_SIZE = 10

# init-6: Get the archive interval
INIT_6_GET_ARCHIVE_INTERVAL = 11

# init-7: Get daylight savings type (automatic or manual)
INIT_7_GET_DAYLIGHT_SAVINGS_TYPE = 12

# init-8: Get daylight manual daylight savings mode (on, off or undefined)
INIT_8_GET_DAYLIGHT_SAVINGS_STATUS = 13

# For generic procedure type things
STATE_IN_PROCEDURE = 14


class DavisWeatherStation(object):
    """
    A class for interfacing with a davis weather station.

    Users must subscribe to the sendData event and send all data it supplies
    to the station hardware *without buffering*.
    """

    # The ASCII 'ACK' character is used by the console to acknowledge a
    # recognised command.
    _ACK = '\x06'

    # The console uses '!' as a NAK character to signal invalid parameters.
    _NAK = '\x21'  # TODO: Some other software uses \x15 here claiming the
                   # documentation is wrong. Further investigation required.

    # Used to signal to the console that its response did not pass the CRC
    # check. Sending this back will make the console resend its last message.
    _CANCEL = '\x18'

    # Used to cancel the download of further DMP packets.
    _ESC = '\x1B'

    # Used to signal that the command was recognised. Like ACK I guess.
    _OK = '\n\rOK\n\r'

    # Used to signal that a long-running command has completed. When the
    # command is executed the console will respond with OK to signal that it
    # has started and DONE to signal it has finished.
    _DONE = 'DONE\n\r'

    _max_dst_offset = datetime.timedelta(hours=2)

    def __init__(self):
        # Setup events
        self.sendData = Event()
        self.loopDataReceived = Event()
        self.loopFinished = Event()
        self.InitCompleted = Event()

        # This event is raised when samples have finished downloading. The
        # sample records will be passed as a parameter.
        self.dumpFinished = Event()

        self._procedure = None
        self._procedure_callback = None

        # This table controls which function handles received data in each
        # state.
        self._state_data_handlers = {
            STATE_SLEEPING: self._sleepingStateDataReceived,
            STATE_LPS: self._lpsStateDataReceived,
            STATE_DMPAFT_INIT_1: self._dmpaftInit1DataReceived,
            STATE_DMPAFT_INIT_2: self._dmpaftInit2DataReceived,
            STATE_DMPAFT_RECV: self._dmpaftRecvDataReceived,
            INIT_1_STATE_GET_STATION_TYPE: self._stationTypeDataReceived,
            INIT_2_GET_VERSION_DATE: self._versionDateInfoReceived,
            INIT_3_GET_VERSION: self._versionInfoReceived,
            INIT_4_GET_TIME: self._timeInfoReceived,
            INIT_5_GET_RAIN_SIZE: self._rainSizeReceived,
            INIT_6_GET_ARCHIVE_INTERVAL: self._archiveIntervalReceived,
            INIT_7_GET_DAYLIGHT_SAVINGS_TYPE: self._daylightSavingsTypeReceived,
            INIT_8_GET_DAYLIGHT_SAVINGS_STATUS: self._daylightSavingsStatusReceived,
            STATE_IN_PROCEDURE: self._procedure_data_received,
        }

        self._reset_state()

    def _reset_state(self):
        # Initialise other fields.
        self._state = STATE_SLEEPING

        # Fields used by the LPS command.
        self._lps_packets_remaining = 0
        self._lps_acknowledged = False
        self._lps_buffer = ""

        # Fields used by the DMPAFT command
        self._dmp_timestamp = None
        self._dmp_buffer = ""

        # Misc
        self._wakeRetries = 0
        self._crc_errors = 0
        self._last_crc_error = None

    def reset(self):
        """
        Resets the data logger after a stall. Current state is wiped and the
        initialise() is called to get everything running again.
        """
        self._reset_state()
        self.initialise()

    def dataReceived(self, data):
        """
        Call this with any data the weather station hardware supplies.
        :param data: Weather station data.
        """

        # Hand the data off to the function for the current state.
        if self._state not in self._state_data_handlers:
            log.msg('WARNING: Data discarded for state {0}: {1}'.format(
                self._state, toHexString(data)))
            return

        self._state_data_handlers[self._state](data)

    def setManualDaylightSavingsState(self, new_dst_state, callback=None):
        log.msg("Switching DST mode to {0}".format(new_dst_state))
        self._procedure = DstSwitchProcedure(self._write, new_dst_state)
        self._procedure_callback = callback
        self._procedure.finished += self._procedure_finished
        self._wakeUp(self._start_procedure)

    def _start_procedure(self):
        self._state = STATE_IN_PROCEDURE
        self._procedure.start()

    def _procedure_data_received(self, data):
        if self._procedure is not None:
            self._procedure.data_received(data)

    def _procedure_finished(self):
        log.msg("Procedure completed.")
        self._state = STATE_AWAKE
        self._procedure = None
        if self._procedure_callback is not None:
            cb = self._procedure_callback
            self._procedure_callback = None
            cb()


    def getLoopPackets(self, packet_count):
        """
        Gets the specified number of LOOP packets one every 2.5 seconds. This
        requires either a Vantage Pro2 with firmware version 1.90 or higher,
        or a Vantage Vue.

        The loop packets will be broadcast using the loopDataReceived event.

        :param packet_count: The number of loop packets to to receive
        :type packet_count: int
        """
        self._lps_packets_remaining = packet_count

        self._wakeUp(self._getLoopPacketsCallback)

    def cancelLoop(self):
        """
        Cancels the current loop request (if any)
        """
        self._lps_packets_remaining = 0
        self._write('\n')
        self._state = STATE_AWAKE

    def _getLoopPacketsCallback(self):
        self._state = STATE_LPS
        self._lps_acknowledged = False
        self._write('LPS 1 ' + str(self._lps_packets_remaining) + '\n')

    def getSamples(self, timestamp):
        """
        Gets all new samples (archive records) logged after the supplied
        timestamp.
        :param timestamp: Timestamp to get all samples after
        :type timestamp: datetime.datetime or tuple
        """
        self._dmp_timestamp = timestamp

        self._wakeUp(self._getSamplesCallback)

    def _getSamplesCallback(self):
        self._state = STATE_DMPAFT_INIT_1

        # Reset everything else used by the various dump processing states.
        self._dmp_buffer = ''
        self._dmp_first_record = None
        self._dmp_records = []
        self._dmp_page_count = None
        self._dmp_remaining_pages = None

        self._write('DMPAFT\n')

    def initialise(self):
        """
        Gets the station type.
        :return:
        """
        self._wakeUp(self._getStationTypeCallback)

    def _getStationTypeCallback(self):
        self._state = INIT_1_STATE_GET_STATION_TYPE
        self._buffer = ''
        self._write('WRD\x12\x4D\n')

    def _stationTypeDataReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 2:
            assert self._buffer[0] == self._ACK

            station_type = ord(self._buffer[1])
            self._buffer = ''

            hw_type = "Unknown"
            if station_type == 0:
                hw_type = "Wizard III"
            elif station_type == 1:
                hw_type = "Wizard II"
            elif station_type == 2:
                hw_type = "Monitor"
            elif station_type == 3:
                hw_type = "Perception"
            elif station_type == 4:
                hw_type = "GroWeather"
            elif station_type == 5:
                hw_type = "Energy Enviromonitor"
            elif station_type == 6:
                hw_type = "Health Enviromonitor"
            elif station_type == 16:
                hw_type = "Vantage Pro, Vantage Pro2"
            elif station_type == 17:
                hw_type = "Vantage Vue"

            self._hw_type = hw_type
            self._station_type = station_type

            self._state = INIT_2_GET_VERSION_DATE
            self._write('VER\n')

    def _versionDateInfoReceived(self, data):
        self._buffer += data

        if self._buffer.count('\n') == 3:
            self._version_date = self._buffer.split('\n')[2].strip()
            self._buffer = ''

            if self._station_type in [16,17]:
                # NVER is only supported on the Vantage Vue or Vantage Pro2
                self._state = INIT_3_GET_VERSION
                self._write('NVER\n')
            else:
                self._version = None
                self._state = INIT_4_GET_TIME
                self._write('GETTIME\n')

    def _versionInfoReceived(self, data):
        self._buffer += data

        if self._buffer.count('\n') == 3:
            self._version = self._buffer.split('\n')[2].strip()
            self._buffer = ''
            self._state = INIT_4_GET_TIME
            self._write('GETTIME\n')

    def _decodeTimeInBuffer(self):
        seconds = ord(self._buffer[1])
        minutes = ord(self._buffer[2])
        hour = ord(self._buffer[3])
        day = ord(self._buffer[4])
        month = ord(self._buffer[5])
        year = ord(self._buffer[6]) + 1900
        return datetime.datetime(year=year, month=month, day=day, hour=hour,
                                 minute=minutes, second=seconds)

    def _timeInfoReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 9:
            self._station_time = self._decodeTimeInBuffer()

            self._buffer = ''
            self._state = INIT_5_GET_RAIN_SIZE
            self._write('EEBRD 2B 01\n')

    def _rainSizeReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 4:
            assert self._buffer[0] == self._ACK

            setup_byte = ord(data[1])
            rainCollectorSize = (setup_byte & 0x30) >> 4
            sizeString = "Unknown - assuming 0.2mm"
            self._rainCollectorSize = 0.2
            if rainCollectorSize == 0:
                sizeString = "0.01 Inches"
                self._rainCollectorSize = 0.254
            elif rainCollectorSize == 1:
                sizeString = "0.2mm"
                self._rainCollectorSize = 0.2
            elif rainCollectorSize == 2:
                sizeString = "0.1mm"
                self._rainCollectorSize = 0.1
            self._rainCollectorSizeName = sizeString

            self._buffer = ''
            self._state = INIT_6_GET_ARCHIVE_INTERVAL
            self._write("EEBRD 2D 01\n")

    def _archiveIntervalReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 4:
            assert self._buffer[0] == self._ACK

            self._archive_interval = ord(data[1])

            self._buffer = ''
            self._state = INIT_7_GET_DAYLIGHT_SAVINGS_TYPE
            self._write("EEBRD 12 1\n")

    def _daylightSavingsTypeReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 4:
            assert self._buffer[0] == self._ACK

            data = self._buffer[1]

            packet_crc = struct.unpack(CRC.FORMAT, self._buffer[2:])[0]
            calculated_crc = CRC.calculate_crc(data)

            assert packet_crc == calculated_crc

            result = ord(data)
            if result == 1:
                self._auto_dst_enabled = False
            elif result == 0:
                self._auto_dst_enabled = True

            self._buffer = ''
            self._state = INIT_8_GET_DAYLIGHT_SAVINGS_STATUS
            self._write("EEBRD 13 1\n")

    def _daylightSavingsStatusReceived(self, data):
        self._buffer += data

        if len(self._buffer) == 4:
            assert self._buffer[0] == self._ACK

            data = self._buffer[1]

            packet_crc = struct.unpack(CRC.FORMAT, self._buffer[2:])[0]
            calculated_crc = CRC.calculate_crc(data)

            assert packet_crc == calculated_crc

            result = ord(data)

            if result == 1:
                dst_on = True
            else:
                dst_on = False

            self._buffer = ''
            self._state = STATE_AWAKE
            self.InitCompleted.fire(self._station_type, self._hw_type,
                                    self._version, self._version_date,
                                    self._station_time,
                                    self._rainCollectorSizeName,
                                    self._archive_interval,
                                    self._auto_dst_enabled,
                                    dst_on)

    def _write(self, data):
        # TODO: store current time so we know if the console has gone back to
        # sleep.
        self.sendData.fire(data)

    def _wakeUp(self, callback=None):
        # This is so we can tell if the function which will be called later can
        # tell if the wake was successful.
        if self._wakeRetries == 0:
            self._state = STATE_SLEEPING
            self._wakeRetries = 0

        if callback is not None:
            self._wakeCallback = callback

        # Only retry three times.
        self._wakeRetries += 1
        if self._wakeRetries > 3:
            self._wakeRetries = 0
            raise Exception('Console failed to wake')

        if self._wakeRetries > 1:
            log.msg('Wake attempt {0}'.format(self._wakeRetries))

        self._write('\n')

        from twisted.internet import reactor
        reactor.callLater(2, self._wakeCheck)

    def _sleepingStateDataReceived(self, data):
        """Handles data while in the sleeping state."""

        # After a wake request the console will respond with \r\n when it is
        # ready to go.
        if data == '\n\r':
            self._state = STATE_AWAKE
            self._wakeRetries = 0
            if self._wakeCallback is not None:
                callback = self._wakeCallback
                self._wakeCallback = None
                callback()

    def _wakeCheck(self):
        """
        Checks to see if the wake-up was successful. If it is not then it will
        trigger a retry.
        """
        if self._state == STATE_SLEEPING:
            log.msg('WakeCheck: Retry...')
            self._wakeUp()

    def _lpsFaultReset(self):
        """
        Resets the current LPS command. For when things go badly wrong and there
        isn't any other safe way to recover.
        :return:
        """
        self._lps_buffer = ""
        self._state = STATE_AWAKE
        if self._lps_packets_remaining <= 0:
            self._lps_packets_remaining = 1
        self.getLoopPackets(self._lps_packets_remaining)

    def _lpsStateDataReceived(self, data):
        """ Handles data while in the LPS state (receiving LOOP packets)
        """

        if not self._lps_acknowledged and data[0] == self._ACK:
            self._lps_acknowledged = True
            data = data[1:]

        # The LPS command hasn't been acknowledged yet so we're not *really*
        # in LPS mode just yet. Who knows what data we received to end up
        # here but this is probably bad. We'll just try to enter LPS mode again.
        if not self._lps_acknowledged:
            # If we've gotten here that means an attempt has just been made to
            # enter LPS mode. The current value in _lps_packets_remaining should
            # be the number of LPS packets originally requested. So We'll just
            # say the attempt to enter LPS mode failed and we'll make another
            # attempt.
            log.msg('WARNING: LPS mode not acknowledged. Retrying...\n'
                    'Trigger data was: {0}'.format(toHexString(data)))
            self._state = STATE_AWAKE
            self.getLoopPackets(self._lps_packets_remaining)
            return

        if len(data) > 0:
            self._lps_buffer += data

        if len(self._lps_buffer) > 98:
            # log.msg('LOOP_DEC. BufferLen={0}, Pkt={1}, Buffer={2}'.format(
            #     len(self._lps_buffer), self._lps_packets_remaining,
            #     toHexString(self._lps_buffer)
            # ))

            # We have at least one full LOOP packet
            packet = self._lps_buffer[0:99]
            self._lps_buffer = self._lps_buffer[99:]

            self._lps_packets_remaining -= 1

            packet_data = packet[0:97]
            packet_crc = struct.unpack(CRC.FORMAT, packet[97:])[0]

            crc = CRC.calculate_crc(packet_data)
            if crc != packet_crc:
                self._crc_errors += 1
                if self._last_crc_error is None:
                    last_crc = 'Never'
                else:
                    last_crc = str(self._last_crc_error)
                log.msg('Warning: CRC validation failed for LOOP packet. '
                        'Discarding. Expected: {0}, got: {1}. '
                        'This is CRC Error #{4}, last was {5}\n'
                        'Packet data: {2}\nBuffer data: {3}'.format(
                        packet_crc, crc, toHexString(packet_data),
                        toHexString(self._lps_buffer), self._crc_errors,
                        last_crc))
                self._last_crc_error = datetime.datetime.now()

                # Now, at this point its possible a mess may have been made. The
                # errors I see come through are where some data was lost in
                # transmission resulting in a very short loop packet. Part of
                # the next loop packet ends up being stolen by the corrupt one
                # resulting in all subsequent loop packets being broken too.
                # So to take care of this situation we will search back through
                # the bad loop packet looking for the string "LOO" and insert
                # everything from that point back into the start of the buffer.

                # Every LOOP packet should end with \n\r followed by its
                # two-byte CRC. If this packet doesn't do that then some of it
                # was probably lost in transmission and part of it actually
                # belongs to the next packet.
                if packet[-4:-2] != '\n\r':
                    # the packet is short. Go looking a second packet header.

                    log.msg('WARNING! End of current packet is corrupt.')
                    end_of_packet = packet.find('\n\r')
                    next_packet = packet.find('LOO', end_of_packet)

                    if next_packet >= 0:
                        log.msg("WARNING! Found next packet data within current "
                                "packet at position {0}. Current packet is {1} "
                                "bytes short. Attempting to patch up buffer."
                                .format(next_packet, (99 - next_packet)))
                        next_packet_data = packet[next_packet:]
                        self._lps_buffer = next_packet_data + self._lps_buffer

                    # If we didn't find a second packet header then something
                    # is properly wrong. Just restart the LOOP command and
                    # everything *should* be OK.
                    if len(self._lps_buffer) < 3 or \
                            self._lps_buffer[0:3] != "LOO":
                        log.msg('WARNING! Buffer is corrupt. Attempting to '
                                'recover by restarting LOOP command.')
                        self._lpsFaultReset()
                        return

            else:
                # CRC checks out. Data should be good
                loop = deserialise_loop(packet_data, self._rainCollectorSize)

                self.loopDataReceived.fire(loop)

            if self._lps_packets_remaining <= 0:
                self._state = STATE_AWAKE
                self.loopFinished.fire()
        elif len(self._lps_buffer) > 3 and self._lps_buffer[0:3] != "LOO":
            # The packet buffer is corrupt. It should *always* start with the
            # string "LOO" (as all LOOP packets start with that).
            # It isn't really safe to assume anything about the state of the
            # LOOP command at this point. Best just to reset it and continue
            # on.
            log.msg("WARNING: Buffer contents is invalid. Discarding and "
                    "resetting LOOP process...")
            log.msg("Buffer contents: {0}".format(
                toHexString(self._lps_buffer)))
            self._lpsFaultReset()
            return
        elif self._lps_buffer.find("\r\n") >= 0 or \
                self._lps_buffer[3:].find("LOO") >= 0:
            # 1. The current packet terminated early (length is <98 and we found
            #    the end-of-packet marker)
            # OR
            # 2. We found the start of a second packet in the current packet
            #    buffer
            # Either way some data has gone missing and the packet buffer
            # is full of garbage. We'll try to throw out the first (bad) packet.

            # Its not safe to try and patch if we're running out of LOOP
            # packets.
            if self._lps_packets_remaining < 5:
                log.msg("WARNING: A second packet header was detected in the"
                        "buffer. Resetting LOOP process...")
                self._lpsFaultReset()
                return

            log.msg("WARNING! A second packet header was detected in the "
                    "buffer. First packet is corrupt. Attempting to fix "
                    "buffer...")

            next_packet = self._lps_buffer.find('LOO', 3)
            if next_packet >= 0:
                log.msg("Found second packet in buffer at position {0}. "
                        "Current packet is {1} bytes short.".format(
                        next_packet, (99-next_packet)))
                self._lps_buffer = self._lps_buffer[next_packet:]
                self._lps_packets_remaining -= 1

            if len(self._lps_buffer) < 3 or self._lps_buffer[0:3] != "LOO":
                log.msg("Recovery failed. Resetting LOOP process...")
                self._lpsFaultReset()
                return

    def _dmpaftInit1DataReceived(self, data):
        if data != self._ACK:
            log.msg('Warning: Expected ACK')
            return

        # Now we send the timestamp.
        if isinstance(self._dmp_timestamp, datetime.datetime):
            date_stamp = encode_date(self._dmp_timestamp.date())
            time_stamp = encode_time(self._dmp_timestamp.time())
            packed = struct.pack('<HH', date_stamp, time_stamp)
        else:
            packed = struct.pack('<HH',
                                 self._dmp_timestamp[0],
                                 self._dmp_timestamp[1])

        crc = CRC.calculate_crc(packed)
        packed += struct.pack(CRC.FORMAT, crc)
        self._state = STATE_DMPAFT_INIT_2
        self._write(packed)

    def _dmpaftInit2DataReceived(self, data):

        if data[0] == self._CANCEL:
            # CRC check failed. Try again.
            self.getSamples(self._dmp_timestamp)
            return
        elif data[0] == self._NAK:
            # The manuals says this happens if we didn't send a full
            # timestamp+CRC. This should never happen. I hope.
            raise Exception('NAK received, DMPAFT state 2')

        self._dmp_buffer += data

        if len(self._dmp_buffer) < 7:
            return

        # Consume the ACK
        assert self._dmp_buffer[0] == self._ACK
        self._dmp_buffer = self._dmp_buffer[1:]

        payload = self._dmp_buffer[0:4]

        crc = struct.unpack(CRC.FORMAT, self._dmp_buffer[4:])[0]
        calculated_crc = CRC.calculate_crc(payload)  # Skip over the ACK

        if crc != calculated_crc:
            log.msg('CRC mismatch for DMPAFT page count')
            # I assume we just start again from scratch if this particular CRC
            # is bad. Thats what we have to do for the other one at least.
            self.getSamples(self._dmp_timestamp)
            return

        page_count, first_record_location = struct.unpack(
            '<HH', payload)

        log.msg('Pages: {0}, First Record: {1}'.format(
            page_count, first_record_location))

        self._dmp_page_count = page_count
        self._dmp_remaining_pages = page_count
        self._dmp_first_record = first_record_location
        self._dmp_buffer = ''
        self._dmp_records = []

        if page_count > 0:
            self._state = STATE_DMPAFT_RECV
        else:
            # Nothing to download
            self._state = STATE_AWAKE

        self._write(self._ACK)

        if page_count <= 0:
            # Let everyone know we're done.
            self.dumpFinished.fire([])

    def _process_dmp_records(self):

        decoded_records = []

        last_ts = None

        for record in self._dmp_records:
            decoded = deserialise_dmp(record, self._rainCollectorSize)

            if decoded.dateStamp is None or decoded.timeStamp is None:
                # We've gone past the last record in archive memory (which was
                # cleared sometime recently). This record is blank so we should
                # discard it and not bother looking at the rest.
                break

            decoded_ts = datetime.datetime.combine(decoded.dateStamp,
                                                   decoded.timeStamp)
            if last_ts is None:
                last_ts = decoded_ts

            # We will ignore it if the time jumps back a little bit as its
            # probably just daylight savings ending.
            if decoded_ts < (last_ts - self._max_dst_offset):
                # We've gone past the last record in archive memory and now
                # we're looking at old data. We should discard this record
                # and not bother looking at the rest.
                break

            # Record seems OK. Add it to the list and continue on.
            last_ts = decoded_ts
            decoded_records.append(decoded)

        self.dumpFinished.fire(decoded_records)

    def _dmpaftRecvDataReceived(self, data):
        self._dmp_buffer += data

        if len(self._dmp_buffer) >= 267:
            page = self._dmp_buffer[0:267]
            self._dmp_buffer = self._dmp_buffer[267:]
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
                    self._state = STATE_AWAKE
                    self._process_dmp_records()
            else:
                log.msg('CRC Failed')
                # CRC failed. Ask for the page to be sent again.
                self._write(self._NAK)

