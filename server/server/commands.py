# coding=utf-8
"""
Some basic commands
"""
import datetime
from server.command import Command, TYP_INFO, TYP_ERROR
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
            self.codedWriteLine(TYP_INFO, 1, sid)

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
        length = datetime.datetime.now() - connect_time

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

