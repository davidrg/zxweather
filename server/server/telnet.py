# coding=utf-8
"""
Telnet protocol support for zxweatherd.
"""
__author__ = 'david'

from twisted.application import internet
from twisted.conch.insults import insults
from twisted.conch.telnet import TelnetTransport, TelnetBootstrapProtocol
from server.shell import ZxweatherShellProtocol
from twisted.internet import  protocol as tiProtocol

def getTelnetService(port):
    """
    Creates the zxweatherd Telnet service. This allows unauthenticated
    access to read-only zxweather server functions.

    :param port: Port to listen on.
    """
    proto = tiProtocol.ServerFactory()
    proto.protocol = lambda: TelnetTransport(
        TelnetBootstrapProtocol,
        insults.ServerProtocol,
        lambda : ZxweatherShellProtocol(None, "telnet")) # Pass 'None' as user (unauthenticated)

    return internet.TCPServer(port, proto)
