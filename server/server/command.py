# coding=utf-8
"""
Base classes for commands.
"""
from twisted.internet import defer

__author__ = 'david'

TYP_INFO = 1
TYP_WARN = 2
TYP_ERROR = 3

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
        self.haltInput = halt_input_callback
        self.resumeInput = resume_input_callback
        self.environment = environment
        self.parameters = parameters
        self.qualifiers = qualifiers
        self.lineBuffer = []
        self.readLineDeferred = None
        self.terminated = False
        self.auto_exit = True

        self.force_close = True

    def execute(self):
        """
        Executes the command. The finished callback will be called when it has
        completed.
        """
        self.haltInput()
        self.main()
        if self.auto_exit:
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
        self.lineBuffer.append(line)

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
        self.resumeInput()

        if len(self.lineBuffer) > 0:
            return defer.succeed(self.lineBuffer.pop(0))
        else:
            self.readLineDeferred = defer.Deferred(
                canceller=lambda inst: self.inputCanceled(inst))
            return self.readLineDeferred

    def terminate(self):
        """
        Called to try and terminate the the command. This may or may not have
        any effect as the command must check to see if it has been terminated
        and decide what to do.
        :return:
        """
        self.terminated = True

        # If the command is waiting on input then cancel it (the input will
        # never arrive).
        if self.readLineDeferred is not None:
            self.readLineDeferred.cancel()

        if self.force_close:
            self.finished()

    def inputCanceled(self, deferred_instance):
        """
        Called when user input is canceled. When this happens it most likely
        means the remote system has requested the command be terminated.
        :param deferred_instance: The deferred instance that was canceled.
        :type deferred_instance: Deferred
        """
        self.finished()

    def writeLine(self, text):
        """
        Writes a line of text to the console
        :param text: Text to write out.
        """
        self.write("{0}\r\n".format(text))

    def codedWriteLine(self, type, code, message):
        """
        Writes out a coded message.
        :param type: Message type (info, warning, error)
        :type type: int
        :param code: Message code. This is command-specific
        :type code: int < 99
        :param message: The message to output
        :type message: str
        """

        if not self.environment["ui_coded"]:
            self.write(message + "\r\n")
        else:
            msg = "{0}{1:02d} {2}\r\n".format(type,code,message)
            self.write(msg)

    def cleanUp(self):
        """
        Called to clean up any resources allocated once the command has
        finished executing.
        """
        pass

    def main(self):
        """
        Override this to do what ever the command should do
        """