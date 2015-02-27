# coding=utf-8
from twisted.internet.protocol import DatagramProtocol

from ...common.util import Event

__author__ = 'david'

class WeatherPushDatagramClient(DatagramProtocol):
    def __init__(self, hostname, port):
        self._host = hostname
        self._port = port

        self.Ready = Event()
        self.ReceiptConfirmation = Event()

    def startProtocol(self):
        self.transport.connect(self._host, self._port)