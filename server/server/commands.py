# coding=utf-8
"""
Some basic commands
"""
import json
import pytz
from datetime import datetime, timedelta
from twisted.conch.insults import insults
from server import subscriptions
from server.command import Command
from server.database import get_station_list, get_station_info, get_sample_csv, station_exists, get_latest_sample_info
from server.session import get_session_value, update_session, get_session_counts, get_session_id_list, session_exists
import dateutil.parser

__author__ = 'david'

TERM_CRT = 0
TERM_BASIC = 1

class ShowUserCommand(Command):
    """
    Shows the currently logged in user. Operated through the SHOW USER
    verb+keyword combination.
    """
    def main(self):
        """ Executes the command """

        username = get_session_value(self.environment["sessionid"], "username")

        self.writeLine(username)


class SetClientCommand(Command):
    """
    Sets the currently connected clients name and, optionally, verison.
    Operated through the SET CLIENT verb+keyword combination.
    """
    def main(self):
        """ Executes the command """
        client_name = self.parameters[1]
        if "version" in self.qualifiers:
            client_version = self.qualifiers["version"]
        else:
            client_version = "Not specified"

        version_info = {
            "name": client_name,
            "version": client_version
        }

        update_session(
            self.environment["sessionid"],
            "client",
            version_info
        )

class SetPromptCommand(Command):
    """
    Allows the user to change the prompt.
    """
    def main(self):
        """ Executes the command """
        prompt = self.parameters[1]

        self.environment["prompt"][0] = prompt

class SetTerminalCommand(Command):
    """
    Allows the type of the terminal to be switched between CRT or BASIC.
    """

    def main(self):
        """ Executes the command """
        if "video" in self.qualifiers:

            # Only allow the VIDEO terminal type if we have a terminal object.
            if self.environment["terminal"] is None:
                self.writeLine("Error: VIDEO terminal type not supported by this protocol")
                return

            self.environment["term_mode"] = TERM_CRT
        elif "basic" in self.qualifiers:
            self.environment["term_mode"] = TERM_BASIC

        if "echo" in self.qualifiers:
            if self.qualifiers["echo"] == "true":
                self.environment["term_echo"] = True
            else:
                self.environment["term_echo"] = False


class SetInterfaceCommand(Command):
    """
    Allows the type of the terminal to be switched between CRT or BASIC.
    """

    def main(self):
        """ Executes the command """
        if "json" in self.qualifiers:
            if self.qualifiers["json"] == "true":
                self.environment["ui_json"] = True
            else:
                self.environment["ui_json"] = False


class ShowClientCommand(Command):
    """
    Shows information previously set using SET CLIENT. Operated through the
    SHOW CLIENT verb+keyword combination.
    """
    def main(self):
        """ Executes the command """

        client_info = get_session_value(
            self.environment["sessionid"],
            "client"
        )

        if client_info is None:
            self.writeLine("Unknown client")
            return

        self.writeLine("Client: {0}".format(client_info["name"]))
        self.writeLine("Version: {0}".format(client_info["version"]))


class ShowEnvironmentCommand(Command):
    """
    Allows environment variables to be inspected
    """

    # These are hidden from the user
    ReservedVariableNames = [
        'prompt',
        'authenticated',
        'terminal',
        'sessionid',
        'f_logout'
    ]

    # These are shown but can not be changed
    ReadOnlyVariables = [
        'ui_json',
        'term_mode',
        'protocol',
        'term_echo',
        'json_mode',
        'term_type'
    ]

    SystemVariables = ReservedVariableNames + ReadOnlyVariables

    def _print_variable(self, variable):
        if variable in self.ReservedVariableNames:
            return

        self.writeLine("{0} = {1}".format(variable,
                                          repr(self.environment[variable])))

    def main(self):
        """
        Prints out the value for one environment variable or all variables
        """
        if "variable" in self.qualifiers:
            var = self.qualifiers["variable"]
            self._print_variable(var)
        else:
            for key in self.environment:
                self._print_variable(key)


class SetEnvironmentCommand(Command):
    """
    Allows environment variables to be set
    """

    def main(self):
        """
        Prints out the value for one environment variable or all variables
        """

        variable = self.parameters[1]
        value = self.parameters[2]

        if variable in ShowEnvironmentCommand.ReservedVariableNames:
            self.write("Can not set reserved variable name: {0}\r\n"
                .format(variable))
        elif variable in ShowEnvironmentCommand.ReadOnlyVariables:
            self.write("Can not set read-only variable: {0}\r\n"
                .format(variable))
        else:
            self.environment[variable] = value


class LogoutCommand(Command):
    """
    A command that attempts to log the user out using a function stored in
    the environment.
    """

    def main(self):
        """ Logs the user out """
        self.auto_exit = False

        if "f_logout" not in self.environment:
            self.write("Error: unable to logout")
            return
        self.environment["f_logout"]()

class ListSessionsCommand(Command):
    """
    Lists all active sessions on the system. If we're using a video terminal
    then the session list will be drawn as a table using VT100 line drawing
    characters.
    """

    def _table_line(self, connect_time_max_len, format_string, protocol_max_len,
                   sid_max_len, username_max_len, join_char):


        if self.environment["term_mode"] == TERM_CRT:

            if join_char == '\x77':
                # top
                left_char = '\x6c'
                right_char = '\x6b'
            elif join_char == '\x6e':
                # middle
                left_char = '\x74'
                right_char = '\x75'
            else:
                # bottom
                left_char = '\x6d'
                right_char = '\x6a'

            self.write("\033(0")
            self.write(left_char + "\x71" * sid_max_len + join_char)
            self.write("\x71" * username_max_len + join_char)
            self.write("\x71" * protocol_max_len + join_char)
            self.write("\x71" * connect_time_max_len + right_char)
            self.writeLine("\033(B")
        else:
            self.writeLine(format_string.format(
                '-' * sid_max_len,
                '-' * username_max_len,
                '-' * protocol_max_len,
                '-' * connect_time_max_len))

    def main(self):
        if not self.authenticated(): return

        sessions = get_session_id_list()

        session_table = []
        sid_max_len = 36
        username_max_len = 8
        protocol_max_len = 8
        connect_time_max_len = 12

        for sid in sessions:
            username = get_session_value(sid, "username")
            connect_time = get_session_value(sid, "connected")
            protocol = get_session_value(sid, "protocol")
            length = str(datetime.now() - connect_time)

            if len(username) > username_max_len:
                username_max_len = len(username)
            if len(protocol) > protocol_max_len:
                protocol_max_len = len(protocol)
            if len(length) > connect_time_max_len:
                connect_time_max_len = len(length)

            session_table.append((sid, username, protocol, length))

        if self.environment["term_mode"] == TERM_CRT:
            format_string = "\033(0\x78\033(B{{0:<{0}}}" \
                            "\033(0\x78\033(B{{1:<{1}}}" \
                            "\033(0\x78\033(B{{2:<{2}}}" \
                            "\033(0\x78\033(B{{3:<{3}}}" \
                            "\033(0\x78\033(B" \
                .format(
                    sid_max_len, username_max_len, protocol_max_len,
                    connect_time_max_len)
        else:
            format_string = "{{0:<{0}}}  {{1:<{1}}}  {{2:<{2}}}  {{3:<{3}}}"\
                .format(
                    sid_max_len, username_max_len, protocol_max_len,
                    connect_time_max_len)

        if self.environment["term_mode"] == TERM_CRT:
            self._table_line(connect_time_max_len, format_string,
                             protocol_max_len, sid_max_len, username_max_len,
                             '\x77')

        self.writeLine(format_string.format(
            "Session Id", "Username", "Protocol", "Connect time"))

        self._table_line(connect_time_max_len, format_string, protocol_max_len,
                         sid_max_len, username_max_len, '\x6e')

        for entry in session_table:
            self.writeLine(format_string.format(*entry))

        self._table_line(connect_time_max_len, format_string, protocol_max_len,
                        sid_max_len, username_max_len, '\x76')

class ShowSessionCommand(Command):
    """
    Shows various information about sessions
    """

    def show_session_statistics(self):
        """
        Shows various statistics about all sessions.
        """
        current, total = get_session_counts()
        self.writeLine("Current sessions: {0}".format(current))
        self.writeLine("Total sessions: {0}".format(total))


    def show_session(self, sid):
        """
        shows information about a specific session.
        :param sid: The session ID.
        """

        if not session_exists(sid):
            self.writeLine("Invalid session id")
            return

        username = get_session_value(sid, "username")
        client_info = get_session_value(sid, "client")
        command = get_session_value(sid, "command")
        connect_time = get_session_value(sid, "connected")
        protocol = get_session_value(sid, "protocol")
        length = datetime.now() - connect_time

        self.writeLine("Username: {0}".format(username))
        self.writeLine("Connected: {0} ({1} ago)".format(connect_time, length))
        self.writeLine("Protocol: {0}".format(protocol))

        if client_info is not None:
            name = client_info["name"]
            version = client_info["version"]
            self.writeLine("Client: {0}".format(name))
            self.writeLine("Version: {0}".format(version))
        else:
            self.writeLine("Unknown client (interactive?)")

        self.writeLine("Current Command:")
        self.writeLine(command)
        self.writeLine("Custom Environment Variables:")
        client_env = get_session_value(sid, "environment")
        for key in client_env:
            if key not in ShowEnvironmentCommand.SystemVariables:
                self.writeLine("{0} = {1}".format(
                    key, repr(client_env[key])))


    def main(self):
        if not self.authenticated(): return

        if "id" in self.qualifiers:
            self.show_session(self.qualifiers["id"])
        else:
            self.show_session_statistics()

class TestCommand(Command):

    def print_lines(self):
        self.write("You entered:\r\n")
        i = 0
        for line in self.lines:
            self.write("{0}: {1}\r\n".format(i,line))
            i += 1

        self.finished()

    def add_line(self, line):
        self.lines.append(line)
        self.write("> ")
        if len(self.lines) < 5:
            self.get_line()
        else:
            self.print_lines()

    def get_line(self):
        self.readLine().addCallback(self.add_line)

    def exit(self):
        """
        Exits the app thing.
        """
        self.finished()

    def main(self):
        if not self.authenticated(): return

        self.lines = []
        self.auto_exit = False
        self.write("Enter 5 lines of text:\r\n> ")

        self.get_line()

class StreamCommand(Command):
    """
    This command streams live data and new samples back to the client as they
    arrive either in the database or from other clients.
    """

    def __init__(self, output_callback, finished_callback, halt_input_callback,
                 resume_input_callback, environment, parameters, qualifiers):

        super(StreamCommand, self).__init__(output_callback, finished_callback,
                                            halt_input_callback,
                                            resume_input_callback, environment,
                                            parameters, qualifiers)
        self.subscribed_station = None
        self.current_live = None
        self.sample_buffer = []
        self.buffer_data = False
        self.subscribe_live = False
        self.subscribe_samples = False
        self.subscribe_images = False

    def _send_catchup(self, data):
        for row in data:
            # We use ISO 8601 date formatting for output.
            csv_data = 's,"{0}",{1}'.format(
                row[0].strftime("%Y-%m-%d %H:%M:%S"), row[1])
            self.writeLine(csv_data)

        # Turn data buffering back off so streaming will resume.
        self.buffer_data = False

    def perform_catchup(self, station, start_timestamp, end_timestamp):
        """
        Fetches all data from the specified timestamp and returns it.
        :param station: The station to do the catchup for
        :type station: str
        :param start_timestamp: The timestamp for the first sample
        :type start_timestamp: datetime
        :param end_timestamp: The timestamp after the last sample (samples with
        this timestamp will not be included).
        :type end_timestamp: datetime
        """

        get_sample_csv(station, start_timestamp, end_timestamp).addCallback(
            self._send_catchup)

    def subscribe(self, station, live, samples, images):
        """
        Subscribes to a data feed for the specified station
        :param station: Station to subscribe to data for
        :type station: str
        :param live: If live data should be included in the subscription
        :type live: bool
        :param samples: If samples should be include in the subscription
        :type samples: bool
        :param images: If images should be included in the subscription
        ;type images: bool
        :return: Current time at UTC
        :rtype: datetime
        """
        self.subscribed_station = station
        self.subscribe_live = live
        self.subscribe_samples = samples
        self.subscribe_images = images

        subscriptions.subscribe(self, station, live, samples, images)

        return datetime.utcnow().replace(tzinfo=pytz.utc)

    def unsubscribe(self):
        """
        Unsubscribes from any current subscriptions.
        """
        if self.subscribed_station is not None:
            subscriptions.unsubscribe(self, self.subscribed_station)

    def live_data(self, data):
        """
        Called by the subscription stuff when ever new live data is available
        :param data: The new data
        :type data: str
        """
        self.writeLine(data)

    def sample_data(self, data):
        """
        Called by the subscription stuff when ever new samples are available.
        :param data: The new data
        :type data: str
        """
        if not self.buffer_data:
            while len(self.sample_buffer) > 0:
                if self.terminated:
                    self.finished()
                    return
                self.writeLine(self.sample_buffer.pop(0))

            self.writeLine(data)
        else:
            self.sample_buffer.append(data)

    def image_data(self, data):
        """
        Called by the subscription stuff when ever new images are available.
        :param data: The new data
        :type data: str
        """
        self.writeLine(data)

    def main(self):
        """
        Sets up subscriptions.
        """
        self.haltInput()  # This doesn't take any user input.

        station_code = self.parameters[0]
        stream_live = "live" in self.qualifiers
        stream_samples = "samples" in self.qualifiers
        stream_images = "images" in self.qualifiers
        from_timestamp = None
        if stream_live is False and stream_samples is False \
                and stream_images is False:
            self.writeLine("Nothing to stream.")
            return

        if "from_timestamp" in self.qualifiers:

            try:
                from_timestamp = dateutil.parser.parse(
                        self.qualifiers["from_timestamp"])
            except Exception as e:
                self.writeLine("Error: {0}".format(e.message))
                return

            now = datetime.utcnow().replace(tzinfo=pytz.utc)
            if from_timestamp < now - timedelta(hours=2):
                self.writeLine("Error: Catchup only allows a maximum of "
                               "2 hours of data.")
                return

        components = []
        if stream_live:
            components.append("live")
        if stream_samples:
            components.append("samples")
        if stream_images:
            components.append("images")
        subset = " and ".join(components)

        catchup = ""
        if from_timestamp is not None:
            catchup = " catching up from {0}".format(from_timestamp)

        self.writeLine("# Streaming {0} data for station '{1}'{2}. "
                       "Send ^C to stop.".format(subset, station_code, catchup))

        # Turn on data buffering (in case we need to catchup first) and
        # subscribe to the data feed. This will return the time when our
        # subscription started.
        self.buffer_data = True
        subscription_start = self.subscribe(station_code, stream_live,
                                            stream_samples, stream_images)

        # If we're supposed to catchup then grab all data for the station
        # from the catchup time through to when we started our subscription.
        # Anything that falls after our subscription will be delivered as soon
        # as the catchup has finished and data buffering gets disabled again.
        if from_timestamp is not None:
            self.perform_catchup(station_code, from_timestamp,
                                 subscription_start)
        else:
            # We're not doing a catchup. No need to buffer data.
            self.buffer_data = False

        # Turn it off here so that early returns above will terminate the
        # command automatically. If we get this far then everything is OK and
        # we don't want to auto-terminate.
        self.auto_exit = False

    def cleanUp(self):
        """
        Clean up
        """
        self.unsubscribe()
        self.writeLine("# Finished")

class ShowLiveCommand(Command):
    """
    A command that shows live data on a VT100-type terminal.
    """

    wind_directions = [
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW",
        "WSW", "W", "WNW", "NW", "NNW"
    ]

    def unsubscribe(self):
        """
        Unsubscribes from any current subscriptions.
        """
        if self.subscribed_station is not None:
            subscriptions.unsubscribe(self, self.subscribed_station)

    def cleanUp(self):
        """ Clean Up """

        if self.subscribed_station is not None:
            self.unsubscribe()

        if self.environment["term_mode"] == TERM_CRT:
            self.terminal.setModes([insults.modes.IRM])
            self.terminal.cursorPosition(80,25)
            self.terminal.write("\r\n")


    def live_data(self, data):
        """
        Called when live data is ready
        :param data: The data in CSV format
        :type data: str
        """

        if not self.ready:
            return

        bits = data.split(',')
        bits.pop(0) #first one is 'l'
        temperature = bits.pop(0) + " C"
        dew_point = bits.pop(0) + " C"
        apparent_temperature = bits.pop(0) + " C"
        wind_chill = bits.pop(0) + " C"
        humidity = bits.pop(0) + "%"
        indoor_temp = bits.pop(0) + " C"
        indoor_humidity = bits.pop(0) + "%"
        pressure = bits.pop(0) + " hPa"

        avg_wind_val = float(bits.pop(0))
        avg_wind = str(avg_wind_val) + " m/s"

        if avg_wind_val == 0.0:
            wind_direction = '--'
            bits.pop(0)  # Throw away the value.
        else:
            wind_direction_val = int(bits.pop(0))

            index = ((wind_direction_val * 100 + 1125) % 36000) / 2250

            wind_direction = str(wind_direction_val) + " degrees (" + \
                self.wind_directions[index] + ")"

        values = [
            temperature, apparent_temperature, wind_chill, dew_point, humidity,
            indoor_temp, indoor_humidity
        ]

        d_bar_trend = ''

        if self.davis_mode:
            bar_trend = int(bits.pop(0))

            if bar_trend == -60:
                d_bar_trend = ' (falling rapidly)'
            elif bar_trend == -20:
                d_bar_trend = ' (falling slowly)'
            elif bar_trend == 0:
                d_bar_trend = ' (steady)'
            elif bar_trend == 20:
                d_bar_trend = ' (rising slowly)'
            elif bar_trend == 60:
                d_bar_trend = ' (rising rapidly)'

        values.append(pressure + d_bar_trend)
        values.append(avg_wind)
        values.append(wind_direction)

        if self.davis_mode:
            values.append(bits.pop(0) + "mm/h")  # Rain Rate
            values.append(bits.pop(0) + "mm")  # Storm Rain

            storm_start = bits.pop(0)
            if storm_start == 'None':
                values.append('--')
            else:
                values.append(storm_start)

            tx_batt = int(bits.pop(0))

            if tx_batt == 0:
                values.append("ok")
            else:
                values.append("warning")

            values.append(bits.pop(0) + "V")  # Console battery voltage
            #forecast_icons = bits.pop(0)
            #forecast_rule = bits.pop(0)

        values.append(str(datetime.now()))

        pos = 2

        for value in values:
            self.terminal.cursorPosition(26, pos)
            self.terminal.write("{0:<40}".format(value))
            pos += 1

        self.terminal.cursorPosition(80, 25)

    def _setup(self, station_info):

        if station_info is None:
            self.writeLine("Invalid station code")
            self.finished()
            return

        if station_info.station_type_code == 'DAVIS':
            self.davis_mode = True
        else:
            self.davis_mode = False

        self.terminal.eraseDisplay()
        self.terminal.resetModes([insults.modes.IRM])

        # Top line
        self.terminal.cursorPosition(0, 0)
        self.write("\033[7m {0:<72} {1:>5} \033[m".format(station_info.title, self.subscribed_station))

        format_str = "{0:>24}:\r\n"
        self.terminal.cursorPosition(0, 2)
        self.terminal.write(format_str.format("Temperature"))
        self.terminal.write(format_str.format("Apparent Temperature"))
        self.terminal.write(format_str.format("Wind Chill"))
        self.terminal.write(format_str.format("Dew Point"))
        self.terminal.write(format_str.format("Humidity"))
        self.terminal.write(format_str.format("Indoor Temperature"))
        self.terminal.write(format_str.format("Indoor Humidity"))
        self.terminal.write(format_str.format("Pressure"))
        self.terminal.write(format_str.format("Wind Speed"))
        self.terminal.write(format_str.format("Wind Direction"))

        if self.davis_mode:
            self.terminal.write(format_str.format("Rain Rate"))
            self.terminal.write(format_str.format("Current Storm Rain"))
            self.terminal.write(format_str.format("Current Storm Start Date"))
            self.terminal.write(format_str.format("Transmitter Battery"))
            self.terminal.write(format_str.format("Console Battery"))

        self.terminal.write(format_str.format("Time Stamp"))

        # Bottom line
        self.terminal.cursorPosition(0, 25)
        self.write("\033[7m {0:<39}{1:>39} \033[m".format(
            "Live Data - Ctrl+C to exit", "zxweather"))

        #
        self.ready = True

    def main(self):
        """
        Sets up subscriptions.
        """
        if self.environment["term_mode"] != TERM_CRT:
            self.writeLine(
                "Error: This command is only available for VT-style terminals.")
            self.writeLine("Use SET TERMINAL /VIDEO to change terminal type.")
            self.subscribed_station = None
            return

        self.subscribed_station = self.parameters[1]
        self.haltInput() # This doesn't take any user input.
        self.terminal = self.environment["terminal"]

        self.ready = False
        subscriptions.subscribe(self, self.subscribed_station, True, False,
                                False)

        get_station_info(self.subscribed_station).addCallback(self._setup)

        # Turn it off here so that early returns above will terminate the
        # command automatically. If we get this far then everything is OK and
        # we don't want to auto-terminate.
        self.auto_exit = False


class ShowLatestCommand(Command):
    """
    Shows the timestamp for the latest sample in the database for the specified
    station.
    """

    def _handle_result(self, result):
        if self._json_mode:
            self.writeLine(json.dumps(result))
        else:
            first = True
            for station in result:
                if not first: self.writeLine("")
                first = False
                self.writeLine("Station: {0}".format(station["station"]))
                self.writeLine("Timestamp: {0}".format(station["timestamp"]))
                if "wh1080_record_number" in station:
                    self.writeLine(
                        "WH1080 Record Number: {0}".format(
                            station["wh1080_record_number"]))
        self.finished()

    def main(self):
        self.auto_exit = False
        if "json" in self.qualifiers:
            self._json_mode = True
        else:
            self._json_mode = self.environment["json_mode"]


        get_latest_sample_info().addCallback(self._handle_result)


class ShowStationCommand(Command):
    """
    Shows information about a station
    """

    def _output_json(self, station_info):
        result = {
            "code": self.code,
            "name": station_info.title,
            "description": station_info.description,
            "sample_interval": station_info.sample_interval,
            "live_data_available": station_info.live_data_available,
            "hardware_type_code": station_info.station_type_code,
            "hardware_type_name": station_info.station_type_title,
            "config": station_info.station_config
        }

        self.writeLine(json.dumps(result))

    def _output_standard(self, station_info):
        self.writeLine("Code: " + self.code)
        self.writeLine("Name: " + station_info.title)
        self.writeLine("Description: " + station_info.description)
        self.writeLine("Sample Interval: {0}".format(
            station_info.sample_interval))
        self.writeLine("Live Data Available: {0}".format(
            station_info.live_data_available))
        self.writeLine("Hardware Type: {0} ({1}) ".format(
            station_info.station_type_code, station_info.station_type_title))

        if station_info.station_type_code == "DAVIS":
            self.writeLine("Is Wireless: {0}".format(
                station_info.station_config["is_wireless"]))
            if station_info.station_config["is_wireless"]:
                self.writeLine("Broadcast ID: {0}".format(
                    station_info.station_config["broadcast_id"]
                ))
            self.writeLine("Model: {0}".format(
                station_info.station_config["hardware_type"]))
            self.writeLine("Has Solar and UV: {0}".format(
                station_info.station_config["has_solar_and_uv"]
            ))

    def _output_station_info(self, station_info):
        if self.json_mode:
            self._output_json(station_info)
        else:
            self._output_standard(station_info)
        self.finished()

    def main(self):
        self.auto_exit = False
        self.code = self.parameters[1]
        if "json" in self.qualifiers and self.qualifiers["json"] == "true":
            self.json_mode = True
        elif "json" in self.qualifiers and self.qualifiers["json"] == "false":
            self.json_mode = False

        if not station_exists(self.code):
            self.writeLine("Invalid station code")
            self.finished()
            return

        get_station_info(self.code).addCallback(self._output_station_info)

class ListStationsCommand(Command):
    """
    Lists all stations in the database.
    """

    def _table_line(self, code_max_len, name_max_len, format_string,
                    join_char):

        if self.environment["term_mode"] == TERM_CRT:

            if join_char == '\x77':
                # top
                left_char = '\x6c'
                right_char = '\x6b'
            elif join_char == '\x6e':
                # middle
                left_char = '\x74'
                right_char = '\x75'
            else:
                # bottom
                left_char = '\x6d'
                right_char = '\x6a'

            self.write("\033(0")
            self.write(left_char + "\x71" * code_max_len + join_char)
            self.write("\x71" * name_max_len + right_char)
            self.writeLine("\033(B")
        else:
            self.writeLine(format_string.format(
                '-' * code_max_len,
                '-' * name_max_len))

    def _output_station_list_term(self, station_list):
        stations = []
        code_max_len = 4
        name_max_len = 4

        for station in station_list:
            code = station[0]
            name = station[1]

            if len(code) > code_max_len:
                code_max_len = len(code)
            if len(name) > name_max_len:
                name_max_len = len(name)

            stations.append((code,name))

        if self.environment["term_mode"] == TERM_CRT:
            format_string = "\033(0\x78\033(B{{0:<{0}}}" \
                            "\033(0\x78\033(B{{1:<{1}}}" \
                            "\033(0\x78\033(B" \
                .format(code_max_len, name_max_len)
        else:
            format_string = "{{0:<{0}}}  {{1:<{1}}}"\
                .format(code_max_len, name_max_len)

        if self.environment["term_mode"] == TERM_CRT:
            self._table_line(code_max_len, name_max_len, format_string, '\x77')

        self.writeLine(format_string.format("Code", "Name"))

        self._table_line(code_max_len, name_max_len, format_string, '\x6e')

        for entry in stations:
            self.writeLine(format_string.format(*entry))

        self._table_line(code_max_len, name_max_len, format_string, '\x76')

        self.finished()

    def _output_station_list_json(self, station_list):

        result = []
        for stn in station_list:
            station = {
                "code": stn[0],
                "name": stn[1]
            }
            result.append(station)

        self.writeLine(json.dumps(result))
        self.finished()

    def _output_station_list(self, station_list):
        if self._json_mode:
            self._output_station_list_json(station_list)
        else:
            self._output_station_list_term(station_list)

    def main(self):
        self.auto_exit = False

        if "json" in self.qualifiers:
            self._json_mode = True
        else:
            self._json_mode = self.environment["json_mode"]

        get_station_list().addCallback(self._output_station_list)