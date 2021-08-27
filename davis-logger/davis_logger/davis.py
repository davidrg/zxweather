# coding=utf-8
"""
The Davis module: Code for interfacing with a Davis weather station. The
Vantage Vue and later firmwares for the Pro2 and Envoy. Firmware pre-1.9 for the
Vantage Pro2 ought to work too but its untested, as is the Vantage Pro (firmware
from 24-APR-2002 only - anything earlier certainly won't work)
"""
import time

import datetime
from twisted.python import log
from davis_logger.station_procedures import DstSwitchProcedure, GetConsoleInformationProcedure, \
    GetConsoleConfigurationProcedure, DmpProcedure, LpsProcedure
from davis_logger.util import Event, to_hex_string

__author__ = 'david'

# Console is asleep and needs waking
STATE_SLEEPING = 0

# Console is awake and ready to receive commands. Note that this is only valid
# for two minutes from the last character sent to the console so act fast.
STATE_AWAKE = 1

# When the console is busy executing the LPS command. Call _cancel_looper to
# stop until the next procedure finishes or stop_live_data to disable entirely.
STATE_LOOP = 2

# For generic procedure type things
STATE_IN_PROCEDURE = 14


class DavisWeatherStation(object):
    """
    A class for interfacing with a davis weather station.

    Users must subscribe to the sendData event and send all data it supplies
    to the station hardware *without buffering*.
    """

    _max_dst_offset = datetime.timedelta(hours=2)

    def __init__(self):
        # Setup events
        self.sendData = Event()
        self.loopDataReceived = Event()
        self.loop2DataReceived = Event()
        self.InitCompleted = Event()

        self._log_data = False

        # This event is raised when samples have finished downloading. The
        # sample records will be passed as a parameter.
        self.dumpFinished = Event()

        self._looper = None
        self._loop_enable = False
        self._looper_cancel_callback = None

        self._procedure = None
        self._procedure_callback = None

        # Initialise to some big number so the console will initially considered
        # to be sleeping.
        self._last_write_time = 500
        self._wake_buffer = bytearray()

        # This table controls which function handles received data in each
        # state.
        self._state_data_handlers = {
            STATE_SLEEPING: self._sleeping_state_data_received,
            STATE_LOOP: None,  # Set when the looper is constructed
            STATE_IN_PROCEDURE: self._procedure_data_received,
        }
        self._state = STATE_SLEEPING

        self._reset_state()

    def _invalid_state(self, data):
        raise Exception("INVALID STATE: {0}".format(self._state))

    def _reset_state(self):
        # Initialise other fields.
        self._state = STATE_SLEEPING

        # Misc
        self._wakeRetries = 0

    def reset(self):
        """
        Resets the data logger after a stall. Current state is wiped and the
        initialise() is called to get everything running again.
        """
        if self._looper is not None:
            self._loop_enable = False
            self._looper.cancel()
            self._looper = None

        self._reset_state()
        self.initialise()

    def data_received(self, data):
        """
        Call this with any data the weather station hardware supplies.
        :param data: Weather station data.
        """

        if self._log_data:
            log.msg("Receive< {0}".format(to_hex_string(data)))

        # Hand the data off to the function for the current state.
        if self._state not in self._state_data_handlers:
            log.msg('WARNING: Data discarded for state {0}: {1}'.format(
                self._state, to_hex_string(data)))
            return

        self._state_data_handlers[self._state](data)

    def set_manual_daylight_savings_state(self, new_dst_state, callback=None):
        """
        Turns manual daylight savings on or off. If the station is currently
        configured for automatic daylight savings this does nothing.
        :param new_dst_state: New daylight savings state - on or off
        :type new_dst_state: bool
        :param callback: Function to call once the operation has completed
        :type callback: callable
        """
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

        # Make sure the looper is not running
        if self._state == STATE_LOOP:
            assert self._looper is not None
            # log.msg("Stopping loop to run procedure...")

            # We'll run this function again once the looper has confirmed its
            # stopped.
            self._cancel_looper(lambda: self._run_procedure(
                procedure, finished_callback))
            return

        if self._procedure is not None:
            raise Exception(
                "Procedure of type {0} started while procedure of type {1} is "
                "still running".format(type(procedure), type(self._procedure)))

        self._procedure = procedure

        # We don't just attach the finished callback to the procedure finished
        # event as the procedure needs to be cleaned up before the callback
        # runs (in case the callback wants to start another procedure)
        self._procedure_callback = finished_callback
        self._procedure.finished += self._procedure_finished

        self._wake_up(self._start_procedure)

    def _start_procedure(self):
        self._state = STATE_IN_PROCEDURE
        log.msg("Starting procedure: {0}".format(self._procedure.Name))
        self._procedure.start()

    def _procedure_data_received(self, data):
        if self._procedure is not None:
            self._procedure.data_received(data)

    def _procedure_finished(self):
        # log.msg("Procedure completed: {0}".format(self._procedure.Name))
        self._state = STATE_AWAKE
        proc = self._procedure
        self._procedure = None

        # Call callback (if we have one) once the finished procedure has been
        # cleaned up.
        if self._procedure_callback is not None:
            cb = self._procedure_callback
            self._procedure_callback = None
            cb(proc)

        if self._loop_enable and self._state != STATE_LOOP:
            # We were looping before this procedure started. Resume looping.
            # log.msg("Procedure finished - resuming loop")
            self.start_live_data()

    def start_live_data(self):
        """
        Starts receipt of loop packets. This will emit LOOP packets continuously
        except when interrupted by other requests. Once an interrupting request
        has completed LOOP packets will resume.
        """
        # log.msg("Starting live data subscription...")
        self._loop_enable = True
        if self._state == STATE_LOOP:
            # log.msg("Already looping")
            pass
        elif self._state == STATE_IN_PROCEDURE:
            # log.msg("Unable to start live data at this time: in procedure. Loop will start when procedure complets.")
            pass
        else:
            self._state = STATE_LOOP
            self._looper.start()

    def stop_live_data(self):
        """
        Stops receipt of loop packets.
        """
        # This will first disable the looper (so it doesn't automatically
        # restart when a procedure finishes) then tell it to cancel so the
        # station stops sending LOOP packets

        self._loop_enable = False
        self._cancel_looper()

    def get_samples(self, timestamp):
        """
        Gets all new samples (archive records) logged after the supplied
        timestamp.
        :param timestamp: Timestamp to get all samples after
        :type timestamp: datetime.datetime
        """
        log.msg("Getting samples since: {0}".format(timestamp))
        procedure = DmpProcedure(self._write, log.msg, timestamp,
                                 self._rainCollectorSize,
                                 self._revision_b_firmware)

        def _samples_ready(proc):
            """
            :param proc: DmpProcedure
            :type proc: DmpProcedure
            """
            assert proc.ArchiveRecords is not None

            self.dumpFinished.fire(proc.ArchiveRecords)

        self._run_procedure(procedure, _samples_ready)

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
        self._revision_b_firmware = procedure.revision_b_firmware

        # TODO: Move these warnings out of here
        if procedure.station_type not in [16, 17]:
            log.msg("WARNING: Unsupported hardware type '{0}' - correct "
                    "functionality unlikely!".format(self._hw_type))

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

        if not procedure.revision_b_firmware:
            log.msg("WARNING: Antique firmware (Rev. A) detected! This is an "
                    "untested configuration and correct functionality is not "
                    "guaranteed. Additionally this implies station hardware is "
                    "a Vantage Pro - this is not properly supported by "
                    "zxweather at this time. ISS Reception calculations assume "
                    "a Vantage Vue or Pro2 and will not be correct for the "
                    "original Vantage Pro.")

        procedure = GetConsoleConfigurationProcedure(
            self._write,
            procedure.station_type == procedure.STATION_TYPE_VANTAGE_VUE,
            procedure.revision_b_firmware)
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
        self._station_list = procedure.ConfiguredStations
        dst_on = procedure.DSTOn

        # Setup the LPS procedure. Apparently there is an undocumented maximum
        # request size of somewhere around 220. So we'll stick to 200 at a time.
        # We give it twisteds callLater function so the LpsProcedure can
        # automatically retry failed cancellations in the event of an error.
        from twisted.internet import reactor
        self._looper = LpsProcedure(self._write, log.msg, self._lps_supported,
                                    self._rainCollectorSize, 200,
                                    reactor.callLater)
        self._looper.loopDataReceived += self.loopDataReceived.fire
        self._looper.loop2DataReceived += self.loop2DataReceived.fire
        self._looper.finished += self._looper_restart
        self._looper.canceled += self._looper_canceled
        self._state_data_handlers[STATE_LOOP] = self._looper.data_received

        self.InitCompleted.fire(self._station_type, self._hw_type,
                                self._version, self._version_date,
                                self._station_time,
                                self._rainCollectorSizeName,
                                self._archive_interval,
                                self._auto_dst_enabled,
                                dst_on,
                                self._station_list,
                                self._lps_supported)

    def _write(self, data):
        if self._log_data:
            log.msg("WRITE> {0}".format(to_hex_string(data)))
        self._last_write_time = time.time()
        self.sendData.fire(data)

    def _wake_up(self, callback=None):
        seconds_since_last_write = time.time() - self._last_write_time

        # The console will stay awake for 120 seconds since it last received
        # a character. To be safe (in case it missed a character) we'll use a
        # shorter threshold
        console_sleeping = seconds_since_last_write > 60

        if not console_sleeping:
            # Console isn't asleep - no need to wake it.
            self._state = STATE_AWAKE
            self._wakeRetries = 0
            if callback is not None:
                callback()
            return

        log.msg("Console may be sleeping - attempting to wake...")

        # This is so we can tell if the function which will be called later can
        # tell if the wake was successful.
        if self._wakeRetries == 0:
            self._state = STATE_SLEEPING

        if callback is not None:
            self._wakeCallback = callback

        # Only retry three times.
        self._wakeRetries += 1
        if self._wakeRetries > 3:
            self._wakeRetries = 0
            raise Exception('Console failed to wake')

        if self._wakeRetries > 1:
            log.msg('Wake attempt {0}'.format(self._wakeRetries))

        self._write(b'\n')

        from twisted.internet import reactor
        reactor.callLater(2, self._wake_check)

    def _sleeping_state_data_received(self, data):
        """Handles data while in the sleeping state."""

        self._wake_buffer.extend(data)

        # After a wake request the console will respond with \r\n when it is
        # ready to go.
        if data.endswith(b'\n\r'):
            log.msg("Station is now awake.")
            self._wake_buffer = bytearray()
            self._state = STATE_AWAKE
            self._wakeRetries = 0
            if self._wakeCallback is not None:
                callback = self._wakeCallback
                self._wakeCallback = None
                callback()

    def _wake_check(self):
        """
        Checks to see if the wake-up was successful. If it is not then it will
        trigger a retry.
        """
        if self._state == STATE_SLEEPING:
            log.msg('WakeCheck: Retry...')
            self._wake_up()

    def _looper_restart(self):
        # This is bound to the loopers finished event. Here we just tell the
        # looper to go again if it hasn't been disabled.
        if self._loop_enable:
            self._looper.start()

    def _cancel_looper(self, callback=None):
        # This simply tells the looper to cancel. The looper canceled event
        # handler will update our current state then run the callback if there
        # is one.
        self._looper_cancel_callback = callback
        self._looper.cancel()

    def _looper_canceled(self):
        # This is bound to the looper canceled event and runs whenever the
        # station confirms the LPS or LOOP command has been canceled.

        if self._state == STATE_LOOP:
            self._state = STATE_SLEEPING

        if self._looper_cancel_callback is not None:
            cb = self._looper_cancel_callback
            self._looper_cancel_callback = None
            cb()
