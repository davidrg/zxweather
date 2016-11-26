# coding=utf-8
"""
Executes commands sent by remote systems.
"""
from functools import partial
import uuid
import datetime
from twisted.python import log
from server.command_tables import  std_verb_table, std_syntax_table, \
    std_keyword_table, std_dispatch_table
from server.session import register_session, end_session, update_session
from server.zxcl.processor import CommandProcessor
from twisted.conch import   recvline

__author__ = 'david'

TABLE_SET_UNAUTHENTICATED = 1
TABLE_SET_AUTHENTICATED = 2

TERM_CRT = 0
TERM_BASIC = 1


class Dispatcher(object):
    """
    Processes command strings, finds and sets up the command to be executed
    and returns it ready for execution by the proper shell.
    """

    def __init__(self, prompter, warning_handler):
        """
        Constructs a new dispatcher.
        :param prompter: Function to prompt for more information with. Takes
        a single argument (the prompt string)
        :type prompter: callable
        :param warning_handler: A function to output warning messages from
        the command processor with. Takes a single parameter: the warning
        message.
        :type warning_handler: callable
        """
        self.environment = {}
        self.prompter = prompter
        self.warning_handler = warning_handler

        self.environment["term_type"] = TERM_BASIC
        self.environment["ui_json"] = False
        self.environment["json_mode"] = False

        self.processor = CommandProcessor(
            std_verb_table,
            std_syntax_table,
            std_keyword_table,
            self.prompter,
            self.warning_handler)
        self.dispatch_table = std_dispatch_table.copy()

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
        log.msg('cmd-exec: {0}'.format(command),
                system="Dispatcher/{0}".format(self.environment["sessionid"]))
        try:
            handler, params, qualifiers = self.processor.process_command(command)
        except Exception as e:
            if e.message is not None:
                self.warning_handler("Error: " + e.message)
                return None
            else:
                raise e

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


class BaseShell(object):
    def __init__(self, user, protocol):
        self.dispatcher = Dispatcher(
            lambda prompt: self.commandProcessorPrompter(prompt),
            lambda warning: self.commandProcessorWarning(warning)
        )
        self.prompt = ["$ ", "_ "]
        self.prompt_number = 0
        self.input_mode = INPUT_SHELL
        self.current_command = None
        self.line_partial = ""

        self.dispatcher.environment["f_logout"] = lambda: self.logout()
        self.dispatcher.environment["prompt"] = self.prompt

        self.sid = str(uuid.uuid1())
        self.dispatcher.environment["sessionid"] = self.sid

        # Base shell has no terminal.
        self.dispatcher.environment["terminal"] = None

        # Configure the environment for interactive or non-interactive use
        # depending on the protocol name
        if protocol in ["ssh", "telnet"]:
            self.dispatcher.environment["term_mode"] = TERM_CRT
            self.dispatcher.environment["term_echo"] = True
        else:
            # If we're not connecting via telnet or ssh the thing at the
            # other end probably isn't a terminal emulator. Turn everything
            # off.
            self.dispatcher.environment["term_mode"] = TERM_BASIC
            self.dispatcher.environment["term_echo"] = False
            self.dispatcher.environment["prompt"][0] = "_ok\n"

        self.dispatcher.environment["protocol"] = protocol

        if user is None:
            username = "anonymous"
            self.dispatcher.environment["authenticated"] = False
        else:
            username = user.username
            self.dispatcher.environment["authenticated"] = True

        register_session(
            self.dispatcher.environment["sessionid"],
            {
                "username": username,
                "command": "[shell]",
                "connected": datetime.datetime.now(),
                "protocol": protocol,
                "environment": self.dispatcher.environment
            }
        )

    def commandProcessorPrompter(self, prompt):
        return None

    def commandProcessorWarning(self, warning):
        pass

    def processFinished(self):
        """
        Called when the process has finished executing.
        """
        if self.current_command is not None:
            self.current_command.cleanUp()
            self.current_command = None

        self.input_mode = INPUT_SHELL
        update_session(self.sid, "command", "[shell]")
        self.showPrompt()

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
        update_session(self.sid, "command", command)
        try:
            self.current_command.execute()
        except Exception as e:
            print(e.message)
            self.processOutput("Command failed\n")
            self.processFinished()

    def logout(self):
        pass

    def terminateProcess(self):
        """
        Attempts to terminate the process.
        """
        if self.current_command is not None:
            self.current_command.terminate()

    def processLine(self, line):
        if self.input_mode == INPUT_SHELL:
            self.executeCommand(line)
        elif self.input_mode == INPUT_COMMAND:
            if self.current_command is not None:
                self.current_command.lineReceived(line)

    def processOutput(self, value):
        pass

    def haltInput(self):
        """
        Called to halt all user input.
        """
        # TODO: switch off everything in keystrokeReceived except listening for ^C
        pass

    def resumeInput(self):
        """
        Called to resume user input.
        """
        # TODO: undo whatever haltInput did.
        pass

    def showPrompt(self):
        pass

class ZxweatherShellProtocol(BaseShell, recvline.HistoricRecvLine):
    """
    The zxweather shell.
    """

    ps = ("$ ", "_ ")

    def __init__(self, user, protocol):
        super(ZxweatherShellProtocol, self).__init__(user, protocol)
        self.ps = (self.prompt[0], self.prompt[1])

    def connectionMade(self):
        """
        Called when a new connection is made by a client.
        """
        recvline.HistoricRecvLine.connectionMade(self)
        self.dispatcher.environment["terminal"] = self.terminal

        # Install key handler to take care of ^C
        self.keyHandlers['\x03'] = self.handle_CTRL_C

    def connectionLost(self, reason):
        """
        The session has ended. Clean everything up.
        :param reason:
        """
        self.terminateProcess()
        end_session(self.sid)

    def characterReceived(self, ch, moreCharactersComing):
        """
        Overridden to allow echo to be turned off.
        :param ch: Character received
        :param moreCharactersComing: ?
        """
        if self.mode == 'insert':
            self.lineBuffer.insert(self.lineBufferIndex, ch)
        else:
            self.lineBuffer[self.lineBufferIndex:self.lineBufferIndex+1] = [ch]
        self.lineBufferIndex += 1

        if self.dispatcher.environment["term_echo"]:
            self.terminal.write(ch)

    def handle_CTRL_C(self):
        """
        Tries to kill any running process.
        """
        if self.input_mode != INPUT_SHELL:
            if self.dispatcher.environment["term_mode"] == TERM_CRT:
                self.terminal.write("\033[7m EXIT \033[m\r\n")
            self.terminateProcess()

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
            self.prompt_number = 1
            self.showPrompt()
        else:
            line = self.line_partial + line
            self.line_partial = ""
            self.prompt_number = 0
            self.lineReceived(line)

    def showPrompt(self):
        """
        Shows the command prompt.
        """
        self.terminal.write(self.prompt[self.prompt_number])

    def lineReceived(self, line):
        """
        Called when ever a line of data is received. This function then routes
        that line off to where ever it should go based on the input mode.
        :param line:
        :return:
        """
        self.processLine(line)

    def commandProcessorPrompter(self, prompt_string):
        """
        Prompts the user for further input when required parameters are missing.
        :param prompt_string: The string to display to the user for the prompt.
        :return: The users input.
        """
        # TODO: Create a new line
        # TODO: Write out prompt string
        # TODO: Get input somehow with a deferred.
        # TODO: Remember to handle ^C which should cause this to return None.
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

    def logout(self):
        """
        Logs the user out.
        """
        self.terminal.write("bye.")
        self.terminal.nextLine()
        self.terminal.loseConnection()