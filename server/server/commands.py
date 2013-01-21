# coding=utf-8
"""
Some basic commands
"""
import pytz
from datetime import datetime, timedelta
from server import subscriptions
from server.command import Command, TYP_INFO, TYP_ERROR
from server.session import get_session_value, update_session, get_session_counts, get_session_id_list, session_exists
import dateutil.parser


__author__ = 'david'

class ShowUserCommand(Command):
    """
    Shows the currently logged in user. Operated through the SHOW USER
    verb+keyword combination.
    """
    def main(self):
        """ Executes the command """

        username = get_session_value(self.environment["sessionid"], "username")

        self.codedWriteLine(TYP_INFO, 01, username)


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
            self.environment["term_mode"] = 0 # TERM_CRT
        elif "basic" in self.qualifiers:
            self.environment["term_mode"] = 1 # TERM_BASIC

class SetInterfaceCommand(Command):
    """
    Allows the type of the terminal to be switched between CRT or BASIC.
    """

    def main(self):
        """ Executes the command """
        if "coded" in self.qualifiers:
            if self.qualifiers["coded"] == "true":
                self.environment["ui_coded"] = True
            else:
                self.environment["ui_coded"] = False


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
            self.codedWriteLine(TYP_INFO, 1, "Unknown client")
            return

        self.codedWriteLine(TYP_INFO, 2, "Client: {0}".format(client_info["name"]))
        self.codedWriteLine(TYP_INFO, 3, "Version: {0}".format(client_info["version"]))


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
    Lists all active sessions on the system
    """

    def main(self):
        sessions = get_session_id_list()

        for sid in sessions:
            self.codedWriteLine(TYP_INFO, 1, sid)

class ShowSessionCommand(Command):
    """
    Shows various information about sessions
    """

    def show_session_statistics(self):
        """
        Shows various statistics about all sessions.
        """
        current,total = get_session_counts()
        self.codedWriteLine(TYP_INFO, 2, "Current sessions: {0}".format(current))
        self.codedWriteLine(TYP_INFO, 3, "Total sessions: {0}".format(total))


    def show_session(self, sid):
        """
        shows information about a specific session.
        :param sid: The session ID.
        """

        if not session_exists(sid):
            self.codedWriteLine(TYP_ERROR, 1,"Invalid session id")
            return

        username = get_session_value(sid, "username")
        client_info = get_session_value(sid, "client")
        command = get_session_value(sid, "command")
        connect_time = get_session_value(sid, "connected")
        length = datetime.now() - connect_time

        self.codedWriteLine(TYP_INFO, 5, "Username: {0}".format(username))
        self.codedWriteLine(TYP_INFO, 6, "Connected: {0} ({1} ago)".format(connect_time,length))

        if client_info is not None:
            name = client_info["name"]
            version = client_info["version"]
            self.codedWriteLine(TYP_INFO, 7,"Client: {0}".format(name))
            self.codedWriteLine(TYP_INFO, 8,"Version: {0}".format(version))
        else:
            self.codedWriteLine(TYP_INFO, 9,"Unknown client (interactive?)")

        self.codedWriteLine(TYP_INFO, 10,"Current Command:")
        self.codedWriteLine(TYP_INFO, 11,command)

    def main(self):
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
        self.lines = []
        self.auto_exit = False
        self.write("Enter 5 lines of text:\r\n> ")

        self.get_line()

class StreamCommand(Command):
    """
    This command streams live data and new samples back to the client as they
    arrive either in the database or from other clients.
    """

    def perform_catchup(self, station, start_timestamp, end_timestamp):
        """
        Fetches all data from the specified timestamp and returns it.
        :param start_timestamp:
        :return:
        """
        pass

    def subscribe(self, station, live, samples):
        """
        Subscribes to a data feed for the specified station
        :param station:
        :param live:
        :param samples:
        :return:
        """
        self.subscribed_station = station
        self.subscribe_live = live
        self.subscribe_samples = samples

        subscriptions.subscribe(self, station, live, samples)

        return datetime.utcnow().replace(tzinfo = pytz.utc)

    def unsubscribe(self):
        """
        Unsubscribes from any current subscriptions.
        """
        subscriptions.unsubscribe(self, self.subscribed_station)


    def live_data(self, data):
        """
        Called by the subscription stuff when ever new live data is available
        :param data: The new data
        :type data: str
        """
        if self.buffer_data:
            self.current_live = data
        else:
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



    def main(self):
        """
        Sets up subscriptions.
        """
        self.current_live = None
        self.sample_buffer = []
        self.haltInput() # This doesn't take any user input.

        station_code = self.parameters[0]
        stream_live = "live" in self.qualifiers
        stream_samples = "samples" in self.qualifiers
        from_timestamp = None
        if stream_live is False and stream_samples is False:
            self.writeLine("Nothing to stream.")
            return

        if "from_timestamp" in self.qualifiers:

            try:
                from_timestamp = dateutil.parser.parse(self.qualifiers["from_timestamp"])
            except Exception as e:
                self.writeLine("Error: {0}".format(e.message))
                return

            now = datetime.utcnow().replace(tzinfo = pytz.utc)
            if from_timestamp < now - timedelta(days=1):
                self.writeLine("Error: Catchup only allows a maximum of "
                               "24 hours of data.")
                return

        subset = "live and sample"
        catchup = ""
        if stream_live is True and stream_samples is False:
            subset = "live"
        elif stream_live is False and stream_samples is True:
            subset = "sample"
        if from_timestamp is not None:
            catchup = " catching up from {0}".format(from_timestamp)

        self.writeLine("# Streaming {0} data for station '{1}'{2}. Send ^C to stop.".format(subset,
            station_code, catchup))


        # Turn on data buffering (in case we need to catchup first) and
        # subscribe to the data feed. This will return the time when our
        # subscription started.
        self.buffer_data = True
        subscription_start = self.subscribe(station_code, stream_live,
            stream_samples)

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
        # we don't want to autoterminate.
        self.auto_exit = False

    def cleanUp(self):
        """
        Clean up
        """
        self.unsubscribe()
        self.writeLine("# Finished")

class ListStationsCommand(Command):
    """
    Lists all stations in the database.
    """

    def main(self):
        pass