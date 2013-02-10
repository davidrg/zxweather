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
    _NAK = '\x21'

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

    def __init__(self):
        # Setup events
        self.sendData = Event()
        self.loopDataReceived = Event()
        self.loopFinished = Event()

        # This event is raised when samples have finished downloading. The
        # sample records will be passed as a parameter.
        self.dumpFinished = Event()

        # This table controls which function handles received data in each
        # state.
        self._state_data_handlers = {
            STATE_SLEEPING: self._sleepingStateDataReceived,
            STATE_LPS: self._lpsStateDataReceived,
            STATE_DMPAFT_INIT_1: self._dmpaftInit1DataReceived,
            STATE_DMPAFT_INIT_2: self._dmpaftInit2DataReceived,
            STATE_DMPAFT_RECV: self._dmpaftRecvDataReceived
        }

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

    def _write(self, data):
        # TODO: store current time so we know if the console has gone back to
        # sleep.
        self.sendData.fire(data)

    def _wakeUp(self, callback=None):
        # This is so we can tell if the function which will be called later can
        # tell if the wake was successful.
        if self._wakeRetries == 0:
            self.state = STATE_SLEEPING

        if callback is not None:
            self._wakeCallback = callback

        # Only retry three times.
        self._wakeRetries += 1
        if self._wakeRetries > 3:
            self._wakeRetries = 0
            raise Exception('Console failed to wake')

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
                self._wakeCallback()

    def _wakeCheck(self):
        """
        Checks to see if the wake-up was successful. If it is not then it will
        trigger a retry.
        """
        if self._state == STATE_SLEEPING:
            self._wakeUp()

    def _lpsStateDataReceived(self, data):
        """ Handles data while in the LPS state (receiving LOOP packets)
        """

        if not self._lps_acknowledged and data[0] == self._ACK:
            self._lps_acknowledged = True
            data = data[1:]

        # The LPS command hasn't been acknowledged yet so we're not *really*
        # in LPS mode just yet. Who knows what data we received to end up
        # here but this is probably bad.
        assert self._lps_acknowledged

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
                log.msg('Warning: CRC validation failed for LOOP packet. '
                        'Discarding.')
            else:
                # CRC checks out. Data should be good
                loop = deserialise_loop(packet_data)

                self.loopDataReceived.fire(loop)

            if self._lps_packets_remaining == 0:
                self._state = STATE_AWAKE
                self.loopFinished.fire()

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

        payload = self._dmp_buffer[0:5]

        crc = struct.unpack(CRC.FORMAT, self._dmp_buffer[-2:])
        calculated_crc = CRC.calculate_crc(payload)

        if crc != calculated_crc:
            log.msg('CRC mismatch for DMPAFT page count')
            # I assume we just start again from scratch if this particular CRC
            # is bad. Thats what we have to do for the other one at least.
            self.getSamples(self._dmp_timestamp)
            return

        ack, page_count, first_record_location = struct.unpack(
            '<BHH', payload)

        assert ack == self._ACK

        self.state = STATE_DMPAFT_RECV
        self._dmp_page_count = page_count
        self._dmp_remaining_pages = page_count
        self._dmp_first_record = first_record_location
        self._dmp_buffer = ''
        self._dmp_records = []
        self._write(self._ACK)

    def _process_dmp_records(self):

        decoded_records = []

        last_ts = None

        for record in self._dmp_records:
            decoded = deserialise_dmp(record)

            if decoded.dateStamp is None or decoded.timeStamp is None:
                # We've gone past the last record in archive memory (which was
                # cleared sometime recently). This record is blank so we should
                # discard it and not bother looking at the rest.
                break

            decoded_ts = datetime.datetime.combine(decoded.dateStamp,
                                                   decoded.timeStamp)
            if last_ts is None:
                last_ts = decoded_ts

            if decoded_ts < last_ts:
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

                # If there are no more pages coming then its time to process
                # it all and reset our state.
                if self._dmp_remaining_pages == 0:
                    self._state = STATE_AWAKE
                    self._process_dmp_records()
            else:
                # CRC failed. Ask for the page to be sent again.
                self._write(self._NAK)
