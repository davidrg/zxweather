# coding=utf-8
"""
Some basic commands
"""
import datetime
from server.command import Command
from server.session import get_session_value, update_session, get_session_counts, get_session_id_list, session_exists

__author__ = 'david'

class ShowUserCommand(Command):
    """
    Shows the currently logged in user. Operated through the SHOW USER
    verb+keyword combination.
    """
    def main(self):
        """ Executes the command """

        username = get_session_value(self.environment["sessionid"], "username")

        self.write(username + "\r\n")


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
            self.write("Unknown client.\r\n")
            return

        info_str = "Client: {0}\r\nVersion: {1}\r\n".format(
            client_info["name"], client_info["version"])
        self.write(info_str)

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

class ShowSessionCommand(Command):
    """
    Shows various information about sessions
    """

    def show_session_list(self):
        """
        Shows a list of all active sessions
        """
        sessions = get_session_id_list()

        for sid in sessions:
            self.write("{0}\r\n".format(sid))

    def show_session_statistics(self):
        """
        Shows various statistics about all sessions.
        """
        current,total = get_session_counts()
        self.write("Current sessions: {0}\r\n".format(current))
        self.write("Total sessions: {0}\r\n".format(total))


    def show_session(self, sid):
        """
        shows information about a specific session.
        :param sid: The session ID.
        """

        if not session_exists(sid):
            self.write("Invalid session id\r\n")
            return

        username = get_session_value(sid, "username")
        client_info = get_session_value(sid, "client")
        command = get_session_value(sid, "command")
        connect_time = get_session_value(sid, "connected")
        length = datetime.datetime.now() - connect_time

        self.write("Username: {0}\r\n".format(username))
        self.write("Connected: {0} ({1} ago)\r\n".format(connect_time,length))

        if client_info is not None:
            name = client_info["name"]
            version = client_info["version"]
            self.write("Client: {0}\r\n".format(name))
            self.write("Version: {0}\r\n".format(version))
        else:
            self.write("Unknown client (interactive?)\r\n")

        self.write("Current Command:\r\n{0}\r\n".format(command))

    def main(self):
        if "list" in self.qualifiers:
            self.show_session_list()
        elif "id" in self.qualifiers:
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
            self.readLine().addCallback(self.add_line)
        else:
            self.print_lines()

    def main(self):
        self.lines = []
        self.auto_exit = False
        self.write("Enter 5 lines of text:\r\n> ")

        self.readLine().addCallback(self.add_line)

