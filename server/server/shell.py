# coding=utf-8
"""
Executes commands sent by remote systems.
"""
from functools import partial
from server.command_tables import authenticated_verb_table, \
    authenticated_syntax_table, authenticated_keyword_table, \
    authenticated_dispatch_table
from server.zxcl.processor import CommandProcessor
from twisted.conch import   recvline

__author__ = 'david'

TABLE_SET_SHELL_INIT = 0
TABLE_SET_AUTHENTICATED = 2


class Dispatcher(object):
    """
    Processes command strings, finds and sets up the command to be executed
    and returns it ready for execution by the proper shell.
    """

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
        self.environment = {}
        print("Init dispatcher: " + str(self) + " (env = " + str(id(self.environment)) + ")")
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


INPUT_SHELL = 0
INPUT_COMMAND = 1

class ZxweatherShellProtocol(recvline.HistoricRecvLine):
    """
    The zxweather shell.
    """

    ps = ("$ ", "_ ")
    line_partial = ""

    def __init__(self,user):
        self.dispatcher = Dispatcher(
            lambda prompt: self.commandProcessorPrompter(prompt),
            lambda warning: self.commandProcessorWarning(warning),
            TABLE_SET_AUTHENTICATED
        )
        self.dispatcher.environment["username"] = user.username
        self.dispatcher.environment["f_logout"] = lambda: self.logout()

        self.input_mode = INPUT_SHELL
        self.current_command = None

    def connectionMade(self):
        """
        Called when a new connection is made by a client.
        """
        recvline.HistoricRecvLine.connectionMade(self)
        self.dispatcher.environment["terminal"] = self.terminal
        self.dispatcher.environment["term"] = "crt"

    def connectionLost(self, reason):
        pass

    def handle_RETURN(self):
        """
        Overrides the function in all parent classes to provide an
        implementation that allows commands to be split over multiple lines
        using the - character when in INPUT_SHELL mode.
        """
        # From HistoricRecvLine
        if self.lineBuffer:
            self.historyLines.append(''.join(self.lineBuffer))
            self.historyPosition = len(self.historyLines)

        # From RecvLine
        line = ''.join(self.lineBuffer)
        self.lineBuffer = []
        self.lineBufferIndex = 0
        self.terminal.nextLine()

        # Handle the continuation - character if we're in SHELL mode.
        if line.endswith("-") and self.input_mode == INPUT_SHELL:
            self.line_partial += line[:-1]
            if not self.line_partial.endswith(" "):
                self.line_partial += " "
            self.pn = 1
            self.showPrompt()
        else:
            line = self.line_partial + line
            self.line_partial = ""
            self.pn = 0
            self.lineReceived(line)

    def showPrompt(self):
        """
        Shows the command prompt.
        """
        self.terminal.write(self.ps[self.pn])

    def executeCommand(self, command):
        """
        Executes the specified command.
        :param command: Command string to execute.
        """

        # Evaluate the command string
        partial_cmd = self.dispatcher.get_command(command)

        # If nothing to execute, do nothing.
        if partial_cmd is None:
            self.processFinished()
            return

        # Supply the command with functions to control its status, input, etc.
        self.current_command = partial_cmd(
            output_callback=lambda text: self.processOutput(text),
            finished_callback=lambda: self.processFinished(),
            halt_input_callback=lambda: self.haltInput(),
            resume_input_callback=lambda: self.resumeInput()
        )

        # Switch input over to the command
        self.input_mode = INPUT_COMMAND

        # and set it running.
        self.current_command.execute()

    def lineReceived(self, line):
        """
        Called when ever a line of data is received. This function then routes
        that line off to where ever it should go based on the input mode.
        :param line:
        :return:
        """
        if self.input_mode == INPUT_SHELL:
            self.executeCommand(line)
        elif self.input_mode == INPUT_COMMAND:
            if self.current_command is not None:
                self.current_command.lineReceived(line)

    def commandProcessorPrompter(self, prompt_string):
        """
        Prompts the user for further input when required parameters are missing.
        :param prompt_string: The string to display to the user for the prompt.
        :return: The users input.
        """
        # TODO: Create a new line
        # TODO: Write out prompt string
        # TODO: Get input somehow with a deferred.
        return None

    def commandProcessorWarning(self, warning):
        """
        Called by the command processor and dispatcher when an error or
        warning has occurred.
        :param warning: The warning or error text.
        """
        self.terminal.write(warning)
        self.terminal.nextLine()

    def processOutput(self, text):
        """
        Called when a process wants to output text.
        :param text: Text to output.
        """
        self.terminal.write(text)

    def processFinished(self):
        """
        Called when the process has finished executing.
        """
        self.input_mode = INPUT_SHELL
        self.showPrompt()

    def haltInput(self):
        """
        Called to halt all user input.
        """
        # TODO: switch off everything in keystrokeReceived except listening for ^Z
        pass

    def resumeInput(self):
        """
        Called to resume user input.
        """
        # TODO: undo whatever haltInput did.
        pass

    def logout(self):
        """
        Logs the user out.
        """
        self.terminal.write("bye.")
        self.terminal.nextLine()
        self.terminal.loseConnection()