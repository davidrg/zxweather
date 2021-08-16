# coding=utf-8
"""
The Davis module: Code for interfacing with a Davis weather station. The
Vantage Vue and later firmwares for the Pro2 and Envoy. Firmware pre-1.9 for the
Vantage Pro2 ought to work too but its untested, as is the Vantage Pro (firmware
from 24-APR-2002 only - anything earlier certainly won't work)
"""
import struct
import datetime
from twisted.python import log
from davis_logger.record_types.dmp import encode_date, encode_time, \
    split_page, deserialise_dmp
from davis_logger.record_types.loop import deserialise_loop
from davis_logger.record_types.util import CRC
from davis_logger.station_procedures import DstSwitchProcedure, GetConsoleInformationProcedure, \
    GetConsoleConfigurationProcedure
from davis_logger.util import Event, to_hex_string

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

            STATE_IN_PROCEDURE: self._procedure_data_received,
        }

        self._reset_state()

    def _invalid_state(self):
        raise Exception("INVALID STATE: {0}".format(self._state))

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
                self._state, to_hex_string(data)))
            return

        self._state_data_handlers[self._state](data)

    def setManualDaylightSavingsState(self, new_dst_state, callback=None):
        log.msg("Switching DST mode to {0}".format(new_dst_state))
        procedure = DstSwitchProcedure(self._write, new_dst_state)
        self._run_procedure(procedure, lambda proc: callback)

    def _run_procedure(self, procedure, finished_callback):
        """
        Runs a Procedure subclass. These represent an interaction or sequence
        of interactions with the station hardware. Each procedure has its own
        buffer for reciving data from the console and, when finished, raises
        its finished event.
        :param procedure: The procedure to run
        :type procedure: Procedure
        :param finished_callback: Function to call on completion. The finished
            procedure is supplied as a parameter
        :type finished_callback: Callable
        """
        self._procedure = procedure

        # We don't just attach the finished callback to the procedure finished
        # event as the procedure needs to be cleaned up before the callback
        # runs (in case the callback wants to start another procedure)
        self._procedure_callback = finished_callback
        self._procedure.finished += self._procedure_finished

        self._wakeUp(self._start_procedure)

    def _start_procedure(self):
        self._state = STATE_IN_PROCEDURE
        self._procedure.start()

    def _procedure_data_received(self, data):
        if self._procedure is not None:
            # Procedures expect bytes - not strings
            if isinstance(data, str):
                try:
                    encoded = data.encode('ascii')
                except:
                    encoded = data

                self._procedure.data_received(encoded)
            else:
                self._procedure.data_received(data)


    def _procedure_finished(self):
        log.msg("Procedure completed: {0}".format(self._procedure.Name))
        self._state = STATE_AWAKE
        proc = self._procedure
        self._procedure = None

        # Call callback (if we have one) once the finished procedure has been
        # cleaned up.
        if self._procedure_callback is not None:
            cb = self._procedure_callback
            self._procedure_callback = None
            cb(proc)

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

        # The LPS command and its associated LOOP2 packets are only supported
        # on the Vantage Pro2 running firmware 1.9 or higher.
        #self._write('LPS 1 ' + str(self._lps_packets_remaining) + '\n')

        self._write('LOOP ' + str(self._lps_packets_remaining) + '\n')

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
        procedure = GetConsoleInformationProcedure(self._write)
        self._run_procedure(procedure, self._console_info_ready)

    def _console_info_ready(self, procedure):
        """
        Stage one initialisation finished: procedure to get basic console info.
        :param procedure: A completed console information procedure
        :type procedure: GetConsoleInformationProcedure
        :return:
        """

        self._version = procedure.version
        self._version_date = procedure.version_date
        self._hw_type = procedure.hw_type
        self._station_type = procedure.station_type
        self._lps_supported = procedure.lps_supported

        # TODO: Move these warnings out of here
        if procedure.station_type not in [16, 17]:
            log.msg("WARNING: Unsupported hardware type '{0}' - correct "
                    "functionality not guaranteed!".format(self._hw_type))

        if not procedure.lps_supported:
            if procedure.self_firmware_upgrade_supported:
                log.msg("WARNING: Console firmware is *very* old. Upgrade to "
                        "at least version 1.90 (released 31 December 2009) for "
                        "full functionality")
            else:
                log.msg("WARNING: Console firmware is *very* old. To support "
                        "full functionality at least version 1.90 is required. "
                        "Your console firmware is not recent enough to support "
                        "PC firmware upgrades - a special firmware upgrade "
                        "device must be obtained.")

        procedure = GetConsoleConfigurationProcedure(self._write)
        self._run_procedure(procedure, self._console_config_ready)

    def _console_config_ready(self, procedure):
        """
        Stage two initialisation finished: procedure to get console config.
        :param procedure: A completed console configuration procedure
        :type procedure: GetConsoleConfigurationProcedure
        """

        self._station_time = procedure.CurrentStationTime
        self._rainCollectorSizeName = procedure.RainSizeString
        self._rainCollectorSize = procedure.RainSizeMM
        self._archive_interval = procedure.ArchiveIntervalMinutes
        self._auto_dst_enabled = procedure.AutoDSTEnabled
        dst_on = procedure.DSTOn

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
                    'Trigger data was: {0}'.format(to_hex_string(data)))
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
                        packet_crc, crc, to_hex_string(packet_data),
                        to_hex_string(self._lps_buffer), self._crc_errors,
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
                to_hex_string(self._lps_buffer)))
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

