# coding=utf-8
"""
Some basic commands
"""
from server.command import Command

__author__ = 'david'

class ShowUserCommand(Command):
    """
    Shows the currently logged in user. Operated through the SHOW USER
    verb+keyword combination.
    """
    def main(self):
        """ Executes the command """
        self.write(self.environment["username"] + "\r\n")


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

        self.environment["client"] = version_info


class ShowClientCommand(Command):
    """
    Shows information previously set using SET CLIENT. Operated through the
    SHOW CLIENT verb+keyword combination.
    """
    def main(self):
        """ Executes the command """
        if "client" not in self.environment:
            self.write("Unknown client.\r\n")
            return

        client_info = self.environment["client"]

        info_str = "Client: {0}\r\nVersion: {1}\r\n".format(
            client_info["name"], client_info["version"])
        self.write(info_str)

class LogoutCommand(Command):
    """
    A command that attempts to log the user out using a function stored in
    the environment.
    """
    def execute(self):
        """ Overridden as we don't want to ever return control to the shell. """
        self.main()

    def main(self):
        """ Logs the user out """
        if "f_logout" not in self.environment:
            self.write("Error: unable to logout")
            return
        self.environment["f_logout"]()