# coding=utf-8
"""
WebSocket support for zxweatherd.
"""
from autobahn.websocket import WebSocketServerProtocol, WebSocketServerFactory
from twisted.application import internet
from server.session import end_session
from server.shell import BaseShell

__author__ = 'david'


def getWebSocketService(port):
    """
    Gets a WebSocket service listening on the specified port
    :param port: Port number
    :type port: int
    """
    factory = WebSocketServerFactory("ws://localhost:{0}".format(port))
    factory.protocol = WebSocketShellProtocol

    return internet.TCPServer(port, factory)

class WebSocketShellProtocol(WebSocketServerProtocol, BaseShell):
    """
    A super basic shell specially for WebSockets due to its incompatible
    protocol design.
    """

    def __init__(self):
        BaseShell.__init__(self, None, "websocket")
        self._buffer = ""

    def connectionLost(self, reason):
        super(WebSocketShellProtocol, self).connectionLost(reason)

        self.terminateProcess()
        end_session(self.sid)

    def onMessage(self, msg, binary):

        if binary:
            return # We don't do binary.

        for char in msg:
            if char == '\x03':

                # ^C was sent. Terminate the process.
                self.terminateProcess()
            else:
                self._buffer += char

            if len(self._buffer) > 0 and self._buffer[-1:] == "\n":
                self.processLine(self._buffer)
                self._buffer = ""

        if len(self._buffer) > 0:
            self.processLine(self._buffer)
            self._buffer = ""

    def commandProcessorWarning(self, warning):
        """
        Called when the command processor gets annoyed.
        :param warning: Warning text
        :type warning: str
        """
        self.sendMessage(warning + "\n", False)

    def logout(self):
        """
        Called when a disconnect has been requested.
        """
        self.sendClose()

    def processOutput(self, value):
        """
        Called when the command wants to send output.
        :param value: Output data
        """
        self.sendMessage(value, False)

    def showPrompt(self):
        """
        Outputs the prompt string.
        :return:
        """
        self.sendMessage(self.prompt[self.prompt_number])

    # TODO: consider overriding sendServerStatus()
