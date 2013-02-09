# coding=utf-8
"""
The Davis module: Code for interfacing with a Davis weather station. The
Vantage Vue in particular (as I don't have access to anything else to test
with).
"""
from collections import namedtuple
import struct
import datetime
from twisted.python import log
from davis_logger.record_types.loop import deserialise_loop
from davis_logger.record_types.util import CRC
from davis_logger.util import Event

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

        # This table controls which function handles received data in each
        # state.
        self._state_data_handlers = {
            STATE_SLEEPING: self._sleepingStateDataReceived,
            STATE_LPS: self._lpsStateDataReceived,
            STATE_DMPAFT_INIT_1: self._dmpaftInit1DataReceived,
            STATE_DMPAFT_INIT_2: self._dmpaftInit2DataReceived,
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

    def dataReceived(self, data):
        """
        Call this with any data the weather station hardware supplies.
        :param data: Weather station data.
        """

        # Hand the data off to the function for the current state.
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

        self._wakeUp()

        self._lps_packets_remaining = packet_count
        self._state = STATE_LPS
        self._lps_acknowledged = False
        self._write('LPS 1 ' + str(packet_count) + '\n')

    def getSamples(self, timestamp):
        """
        Gets all new samples (archive records) logged after the supplied
        timestamp.
        :param timestamp: Timestamp to get all samples after
        :type timestamp: datetime.datetime
        """

        self._wakeUp()

        self._state = STATE_DMPAFT_INIT_1
        self._dmp_timestamp = timestamp
        self._write('DMPAFT\n')

    def _write(self, data):
        # TODO: store current time so we know if the console has gone back to
        # sleep.
        self.sendData.fire(data)

    def _wakeUp(self):
        self._write('\n')

        # TODO: Implement retry code. Make one attempt every 1.2 seconds
        # Don't bother making more than three attempts.
#        if self._wake_attempts > 3:
#            raise Exception('Failed to wake station after three attempts')

    def _sleepingStateDataReceived(self, data):
        """Handles data while in the sleeping state."""

        # After a wake request the console will respond with \r\n when it is
        # ready to go.
        if data == '\r\n':
            self._state = STATE_AWAKE

    def _lpsStateDataReceived(self, data):
        """ Handles data while in the LPS state (receiving LOOP packets)
        """

        if not self._lps_acknowledged and data[0] == self._ACK:
            self._lps_acknowledged = True
            data.pop(0)

        # The LPS command hasn't been acknowledged yet so we're not *really*
        # in LPS mode just yet. Who knows what data we received to end up
        # here but this is probably bad.
        assert self._lps_acknowledged

        if len(data) > 0:
            self._lps_buffer += data

        if self._lps_buffer > 98:
            # We have at least one full LOOP packet
            packet = self._lps_buffer[0:99]
            if len(self._lps_buffer) > 99:
                self._lps_buffer = self._lps_buffer[99:]

            packet_data = packet[0:97]
            packet_crc = packet[98:]

            self._lps_packets_remaining -= 1

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
        year = self._dmp_timestamp.year
        month = self._dmp_timestamp.month
        day = self._dmp_timestamp.day
        hour = self._dmp_timestamp.hour
        minute = self._dmp_timestamp.minute

        date_stamp = day + month*32 + (year-2000)*512
        time_stamp = 100*hour + minute

        packed = struct.pack('<HH', date_stamp, time_stamp)
        crc = CRC.calculate_crc(packed)
        packed_crc = struct.pack(CRC.FORMAT, crc)
        self._state = STATE_DMPAFT_INIT_2
        self._write(packed + packed_crc)

    def _dmpaftInit2DataReceived(self, data):

        if data[0] == self._CANCEL:
            # CRC check failed. Resend by pretending that we just sent the
            # DMPAFT command (which will get an ACK response)
            self._dmpaftInit1DataReceived(self._ACK)
            return
        elif data[0] == self._NAK:
            # The manuals says this happens if we don't send a full
            # timestamp+CRC. This should never happen. I hope.
            raise Exception('NAK received, DMPAFT state 2')

        self._dmp_buffer += data

        if len(self._dmp_buffer) < 6: return

        payload = self._dmp_buffer[0:4]

        crc = struct.unpack(CRC.FORMAT, self._dmp_buffer[-2:])
        calculated_crc = CRC.calculate_crc(payload)

        if crc != calculated_crc:
            log.msg('CRC mismatch for DMPAFT page count')
            # TODO: Do we send a NAK or something here?
            pass

        ack, page_count, first_record_location = struct.unpack(
            '<BHB', payload)