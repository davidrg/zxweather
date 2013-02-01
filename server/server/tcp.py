# coding=utf-8
"""
Raw TCP support for zxweatherd
"""
from twisted.application import internet
from twisted.conch.insults import insults
from twisted.internet.protocol import ServerFactory, Factory
from server.shell import ZxweatherShellProtocol


__author__ = 'david'

#class FakeTerminalTransport(object):
#    def __init__(self, transport):
#        self._transport = transport
#
#    def write(self, data):
#        self._transport.write(data)
#
#    def setModes(self, modes):
#        pass
#
#    def cursorPosition(self, x, y):
#        pass
#
#    def eraseDisplay(self):
#        pass
#
#    def resetModes(self, modes):
#        pass
#
#    def nextLine(self):
#        self.write("\n")
#
#class NonInteractiveShellProtocol(protocol.Protocol):
#    def dataReceived(self, data):
#        pass


class ShellProtocolFactory(Factory):
    def __init__(self, protocol_name):
        self._protocol_name = protocol_name
    def buildProtocol(self, addr):
        return insults.ServerProtocol(ZxweatherShellProtocol, None, self._protocol_name)

def getTCPService(port):
    """
    Listens for commands on a TCP port. The session is setup for non-interactive
    use and does not support line editing, etc.
    :param port:
    :return:
    """

    return internet.TCPServer(port, ShellProtocolFactory("raw"))
