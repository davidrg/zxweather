# coding=utf-8
"""
Executes commands sent by remote systems.
"""
from functools import partial
from server.command_tables import authenticated_verb_table, \
    authenticated_syntax_table, authenticated_keyword_table, \
    authenticated_dispatch_table
from server.zxcl.processor import CommandProcessor

__author__ = 'david'

TABLE_SET_SHELL_INIT = 0
TABLE_SET_AUTHENTICATED = 2


class Dispatcher(object):
    """
    Processes command strings, finds and sets up the command to be executed
    and returns it ready for execution by the proper shell.
    """

    environment = {}

    def __init__(self, prompter, warning_handler, initial_table_set):
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
        :rtype: partial
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
