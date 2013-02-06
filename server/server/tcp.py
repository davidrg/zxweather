# coding=utf-8
"""
Raw TCP support for zxweatherd
"""
from twisted.application import internet
from twisted.conch.insults import insults
from twisted.internet.protocol import  Factory
from server.shell import ZxweatherShellProtocol


__author__ = 'david'

class ShellProtocolFactory(Factory):
    def __init__(self, protocol_name):
        self._protocol_name = protocol_name
    def buildProtocol(self, addr):
        return insults.ServerProtocol(ZxweatherShellProtocol, None, self._protocol_name)

def getTCPService(port):
    """
    Listens for commands on a TCP port. The session is setup for non-interactive
    use and does not support line editing, etc.
    :param port: Port to listen on.
    :return:
    """

    return internet.TCPServer(port, ShellProtocolFactory("raw"))
