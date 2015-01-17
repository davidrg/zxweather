# coding=utf-8
"""
A client for uploading weather data to a remote server.
"""
from collections import deque
import json
from twisted.internet import reactor
from twisted.python import log

__author__ = 'david'

# Init mode: We're just initialising the connection. This prevents line
# buffering as we need to respond to the '$' prompt first.
MODE_INIT = 0

# This configures the remote session by changing the terminal configuration,
# setting client name, etc.
MODE_SETUP = 1

# We're waiting for a response to the show latest command.
MODE_GET_LATEST = 2

# Ready to execute the upload command
MODE_UPLOAD_RDY = 3

# This is when we're sending data to the remote host.
MODE_UPLOAD = 4

# disconnecting, etc.
MODE_FINISHED = 5

# Done
MODE_DONE = 6


class UploadClient(object):
    """
    This client handles uploading data to the
    """

    # Format strings for sending data.
    live_format = "l,{station_code},{download_timestamp},{indoor_humidity}," \
                  "{indoor_temperature},{temperature},{humidity},{pressure}," \
                  "{average_wind_speed},{gust_wind_speed},{wind_direction}"
    davis_live_format = ",{bar_trend},{rain_rate},{storm_rain}," \
                        "{current_storm_start_date},{transmitter_battery}," \
                        "{console_battery_voltage},{forecast_icon}," \
                        "{forecast_rule_id},{uv_index},{solar_radiation}"
    base_sample_format = "s,{station_code},{temperature},{humidity}," \
                         "{indoor_temperature},{indoor_humidity},{pressure}," \
                         "{average_wind_speed},{gust_wind_speed}," \
                         "{wind_direction},{rainfall},{download_timestamp}," \
                         "{time_stamp}"
    wh1080_sample_format = ",{sample_interval},{record_number}," \
                           "{last_in_batch},{invalid_data}," \
                           "{wh1080_wind_direction},{total_rain}," \
                           "{rain_overflow}"
    davis_sample_format = ",{record_time},{record_date},{high_temperature}," \
                          "{low_temperature},{high_rain_rate}," \
                          "{solar_radiation},{wind_sample_count}," \
                          "{gust_wind_direction},{average_uv_index}," \
                          "{evapotranspiration},{high_solar_radiation}," \
                          "{high_uv_index},{forecast_rule_id}"

    def __init__(self, finished_callback, ready_callback, client_name, client_version=None):
        """
        Sets up the upload client.
        :param finished_callback: Called when the upload client has
        successfully disconnected from the server
        :type finished_callback: callable
        :param ready_callback: Called when the upload client is ready.
        :type ready_callback: callable
        :param client_name: The name of the client application.
        :type client_name: str
        :param client_version: The version string of the client application
        :type client_version: str or None
        """
        self._line_buffer = ""
        self._mode = MODE_INIT
        self.latestSampleInfo = None

        self._sample_buffer = []

        self._client_name = client_name
        self._client_version = client_version
        self._finished_callback = finished_callback
        self._client_ready = ready_callback

        self._sent_data = deque([], maxlen=50)

    def makeConnection(self, channel):
        self._channel = channel
        self.write = lambda data: self._channel.write(data)

    def dataReceived(self, data):
        """
        Called when ever new data comes in. As this could be anything from a
        few characters to several lines we'll buffer it up and call
        lineReceived for each line of data we get.
        :param data: The incoming data
        """
        for char in data:
            self._line_buffer += char
            if self._line_buffer[-2:] == '\r\n':
                # The buffer consists of a full line of data. Send it through
                # (without EOL sequence) for processing.
                self._lineReceived(self._line_buffer[:-2])
                self._line_buffer = ""

        if self._mode == MODE_INIT and '$' in self._line_buffer:
            # A '$' symbol has come through. This is likely the remote shells
            # prompt. We'll try changing it to something a little bit easier
            # to identify.
            self._changePrompt(r"_ok\n")
            self._mode = MODE_SETUP
            self._setup_state = 1
            self._abend = False

    def _lineReceived(self, data):
        """
        Called when ever a full line of data is received.
        :param data:
        :return:
        """
        if data == '': return

        if self._mode == MODE_INIT:
            # Not our job to deal with input here(we shouldn't even get any
            # in init mode).
            return
        elif data == '_ok':
            # We're at a shell prompt. Time to send the next command.
            if self._mode == MODE_SETUP:
                self._setup_environment()
            elif self._mode == MODE_FINISHED:
                self._writeLine("logout")
                self._mode = MODE_DONE
            elif self._mode == MODE_UPLOAD_RDY:
                self._writeLine("upload")
                self._mode = MODE_UPLOAD
        elif self._mode == MODE_GET_LATEST:
            # This should be some JSON data containing details about the last
            # sample in the remote database.
            self._processLatestInfo(data)
            self._mode = MODE_UPLOAD_RDY
        elif self._mode == MODE_UPLOAD:
            # The upload command should be running now. Messages sent back
            # to us will start with the '#' character. Errors will start with
            # '# ERR'. We can ignore informational messages but errors probably
            # mean we should stop.

            if data.startswith("# ERR"):
                # Something went wrong.
                self._handleUploadError(data)
            elif data[0] == '#':
                # The upload command has sent a message saying its ready.
                # Let what ever is managing this object know so it can start
                # giving us data to send.
                log.msg('Server message: ' + data[1:])
                self._client_ready()

        elif self._mode == MODE_DONE:
            # We've logged out.
            if data == "bye.":
                # Logout has completed.
                if self._abend:
                    # Abnormal end. Stop the reactor.
                    reactor.stop()
                else:
                    # Normal end: Something else asked us to disconnect. Tell
                    # them we've done as requested.
                    self._finished_callback()

    def _writeLine(self, data):
        self.write(data + "\n")

    def _setup_environment(self):
        """ Configures the remote environment (turns off fancy terminal
        features, etc) """
        if self._setup_state == 1:
            # This will reduce the amount of crap we get sent.
            self._writeLine('set terminal/basic/echo=false')
        elif self._setup_state == 2:
            self._setClient()
        elif self._setup_state == 3:
            self._writeLine('show latest/json')
            self._mode = MODE_GET_LATEST
        self._setup_state += 1

    def _changePrompt(self, prompt_string):
        self._writeLine('SET PROMPT "{0}"'.format(prompt_string))

    def _setClient(self):
        command = 'set client "{0}"'.format(self._client_name)
        if self._client_version is not None:
            command += '/version="{0}"'.format(self._client_version)
        self._writeLine(command)

    def _handleUploadError(self, error):
        # The upload command rejected the data we sent it for some reason.
        # We should not send any more data otherwise we could end up with
        # gaps in the remote database. Such gaps are still possible in this
        # case however:
        #     Insert A - succeeds
        #     Insert B - fails
        #     Insert C - succeeds.
        #     -- error reported for insert B here. weather-push halted.
        #     -- weather push restarted
        #     Insert D - succeeds
        # here record B would never be re-pushed because the remote database
        # is always updated from the timestamp of the most recent record
        # (meaning gaps will never be filled in)

        log.msg("--------------------------------------------------------")
        log.msg("Upload error encountered.")
        log.msg("Dumping last {0} samples sent".format(self._sent_data.maxlen))

        i = 0
        for value in self._sent_data:
            log.msg("{0}: {1}".format(i, value))
            i += 1

        i = 0
        log.msg("Contents of output buffer:")
        for value in self._sample_buffer:
            log.msg("{0}: {1}".format(i, value))
            i += 1

        log.msg('Upload error: ' + error[2:])
        log.msg('Upload canceled to prevent remote database corruption. '
                'Weather-push will now stop. Check remote database for gaps '
                ' before restarting.')

        # We don't want to just throw an exception here as it won't bring the
        # reactor to a stop, etc. We want to cleanly logout and then exit.

        # This will cause the reactor to be stopped once the logout has
        # completed.
        self._abend = True

        self.quit()

    def _processLatestInfo(self, latest_sample_json):
        result = json.loads(latest_sample_json)

        if 'err' not in result:
            # No errors. We're good to go.

            # Convert the result from a list to a dict so we can look stuff
            # up by station code.
            self.latestSampleInfo = {}

            log.msg("---- Station info ----")
            for station_info in result:
                self.latestSampleInfo[station_info['station']] = station_info
                log.msg("Station: " + station_info['station'])
                log.msg(" - Timestamp: " + station_info['timestamp'])

                if 'wh1080_record_number' in station_info:
                    log.msg(" - WH1080 Record: " + str(station_info['wh1080_record_number']))


    def quit(self):
        """
        Disconnects from the remote host.
        """
        if self._mode == MODE_UPLOAD:
            # Send ^C to kill the upload command
            self.write('\x03')
        self._mode = MODE_FINISHED

    def _transmit_samples(self):
        """
        Transmits any live data or samples that are ready to go.
        :return:
        """
        if self._mode != MODE_UPLOAD:
            return

        sample_count = len(self._sample_buffer)

        if sample_count > 50:
            log.msg("Transmitting {0} samples...".format(sample_count))

        while len(self._sample_buffer) > 0:
            value = self._sample_buffer.pop(0)
            self._sent_data.append(value)
            self._writeLine(value)

        if sample_count > 50:
            log.msg("Transmission complete.")

    def sendLive(self, live_data, hardware_type):
        """
        Sends live data to the remote system. If the remote system is not ready
        to receive live data it will be discarded.
        :param live_data: Live data to send
        :type live_data: dict
        """

        if self._mode == MODE_UPLOAD:
            value = self.live_format.format(**live_data)

            if hardware_type == 'DAVIS':
                value += self.davis_live_format.format(**live_data)

            #print('Live: ' + value)
            self._writeLine(value)

    def sendSample(self, sample_data, hardware_type, hold=False):
        """
        Sends sample data to the remote system. If the remote system is not
        ready to receive new samples they will be queued and sent when the
        remote system becomes ready.
        :param sample_data: Base sample data
        :type sample_data: dict
        :param hardware_type: The hardware type code for the station.
        :type hardware_type: str
        """

        value = self.base_sample_format.format(**sample_data)

        #log.msg('Sample: ' + value)

        if hardware_type == 'FOWH1080':
            wh1080_value = self.wh1080_sample_format.format(**sample_data)
            value += wh1080_value
        elif hardware_type == 'DAVIS':
            davis_value = self.davis_sample_format.format(**sample_data)
            value += davis_value
        # elif GENERIC: doesn't have any extra stuff not already in value.

        self._sample_buffer.append(value)
        if not hold:
            self._transmit_samples()

    def flushSamples(self):
        """
        Flushes any buffered samples.
        """
        self._transmit_samples()

    def reset(self):
        """
        Called to reset the client. This is done when the connection to the
        server is reestablished after being lost
        :return:
        """
        self._mode = MODE_INIT
        self._line_buffer = ""

        self.latestSampleInfo = None
        self._sample_buffer = []
