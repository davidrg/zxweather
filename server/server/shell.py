# coding=utf-8
"""
Executes commands sent by remote systems.
"""
from functools import partial
from twisted.internet import defer
from server.command_tables import authenticated_verb_table, \
    authenticated_syntax_table, authenticated_keyword_table, \
    authenticated_dispatch_table, shell_init_verb_table, shell_init_syntax_table, shell_init_keyword_table, shell_init_dispatch_table
from server.zxcl.processor import CommandProcessor

__author__ = 'david'

TABLE_SET_SHELL_INIT = 0
TABLE_SET_AUTHENTICATED = 2

class Command(object):
    """
    Base class for all commands executed by the ZXCL Shell.
    :param output_callback: A function to call to write out output
    :type output_callback: callable
    :param finished_callback: A function to call when the command has finished
    executing
    :type finished_callback: callable
    :param halt_input_callback: A function to call when command isn't
    interested in receiving any further user input.
    :type halt_input_callback: callable
    :param resume_input_callback: A function to call when command wishes to
    resume receiving user input.
    :type resume_input_callback: callable
    :param environment: A dict containing various details about the
    environment. This includes some settings.
    :type environment: dict
    :param parameters: A list of all parameters that were supplied
    :type parameters: dict
    :param qualifiers: A list of all qualifiers that were supplied
    :type qualifiers: dict
    """

    def __init__(self, output_callback, finished_callback,
                 halt_input_callback, resume_input_callback,
                 environment,
                 parameters, qualifiers):
        self.write = output_callback
        self.finished = finished_callback
        self.halt_input = halt_input_callback
        self.resume_input = resume_input_callback
        self.environment = environment
        self.parameters = parameters
        self.qualifiers = qualifiers
        self.lineBuffer = []
        self.readLineDeferred = None
        self.terminated = False

    def execute(self):
        """
        Executes the command. The finished callback will be called when it has
        completed.
        """
        self.finished()

    def lineReceived(self, line):
        """
        Called when ever a line of input is ready for the command to process.
        This is added to the commands line input buffer and will be removed
        from there when the command consumes a line of input. If the command
        does not take input then the input will just be discarded when the
        command finishes.
        :param line:
        :return:
        """
        self.lineBuffer += line

        if self.readLineDeferred is not None:
            # The app is waiting for a line of input. Send it off.
            deferred = self.readLineDeferred
            self.readLineDeferred = None
            deferred.callback(self.lineBuffer.pop(0))

    def readLine(self):
        """
        Gets a line of input from the input buffer and returns it as a Deferred.
        If the input buffer is empty then the deferred will not execute until
        the lineReceived method is called with a new line of input.
        :return: One line of input
        :rtype: Deferred
        """
        # We should call resume_input() just in case input was previously
        # halted. If we don't then we will just sit here waiting for something
        # that will never happen.
        self.resume_input()

        if len(self.lineBuffer) > 0:
            return defer.succeed(self.lineBuffer.pop(0))
        else:
            self.readLineDeferred = defer.Deferred()
            return self.readLineDeferred

    def terminate(self):
        """
        Called to try and terminate the the command. This may or may not have
        any effect as the command must check to see if it has been terminated
        and decide what to do.
        :return:
        """
        self.terminated = True


class Dispatcher(object):
    """
    Executes commands sent by remote systems
    """

    environment = {}

    def __init__(self, prompter, warning_handler, initial_table_set, writer,
                 reader):
        """
        Constructs a new dispatcher.
        :param prompter: Function to prompt for more information with. Takes
        a single argument (the prompt string)
        :type prompter: callable
        :param warning_handler: A function to output warning messages from
        the command processor with. Takes a single parameter: the warning
        message.
        :type warning_handler: callable
        :param initial_table_set: Initial set of command tables to use. This
         can be changed later using switch_table_set().
        """
        self.prompter = prompter
        self.warning_handler = warning_handler
        self.writer = writer
        self.reader = reader
        self.switch_table_set(initial_table_set)

    def switch_table_set(self, table_set):
        """
        Switches to an alternate set of command tables.
        :param table_set: New command set table.
        """
        if table_set == TABLE_SET_AUTHENTICATED:
            self.processor = CommandProcessor(
                authenticated_verb_table,
                authenticated_syntax_table,
                authenticated_keyword_table,
                self.prompter,
                self.warning_handler)
            self.dispatch_table = authenticated_dispatch_table.copy()
        elif table_set == TABLE_SET_SHELL_INIT:
            self.processor = CommandProcessor(
                shell_init_verb_table,
                shell_init_syntax_table,
                shell_init_keyword_table,
                self.prompter,
                self.warning_handler)
            self.dispatch_table = shell_init_dispatch_table.copy()

    def get_command(self, command):
        """
        Gets a command which can be executed at a later time. It is returned
        as a partial function which requires the following callbacks to be
        supplied:
         - output_callback
         - finished_callback,
         - halt_input_callback
         - resume_input_callback
        :param command: The command string to process.
        :return: A new partial function which can be called to finish building
        an executable command
        :rtype: callable
        """

        try:
            handler, params, qualifiers = self.processor.process_command(command)
        except Exception as e:
            self.warning_handler("Error: " + e.message)
            return None

        if handler is None:
            # nothing to do.
            return None

        if handler not in self.dispatch_table:
            self.warning_handler("Error: invalid command (dispatch error)")
            return None
        else:
            cmd = self.dispatch_table[handler]

        func = partial(cmd, parameters=params, qualifiers=qualifiers,
            environment=self.environment)

        return func

    def execute_command(self, command):
        """
        Executes the supplied command.
        :param command: Command to execute
        """
        try:
            handler, params, qualifiers = self.processor.process_command(command)
        except Exception as e:
            self.warning_handler("Error: " + e.message)
            return

        if handler is None:
            # nothing to do.
            return

        if handler not in self.dispatch_table:
            self.warning_handler("Error: invalid command (dispatch error)")
        else:
            self.dispatch_table[handler](self.writer, self.reader,
                self.environment, params, qualifiers)

        # TODO: Make this return a callable object that we can forward all
        # user input to in the shell.

