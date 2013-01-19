# coding=utf-8
"""
Base classes for commands.
"""
from twisted.internet import defer

__author__ = 'david'


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

    def execute(self):
        """
        Executes the command. The finished callback will be called when it has
        completed.
        """
        self.haltInput()
        self.main()
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
        self.resumeInput()

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

    def main(self):
        """
        Override this to do what ever the command should do
        """