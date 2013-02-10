# coding=utf-8
"""
Data logger for Davis Vantage Vue weather stations.
"""
import sys
from twisted.internet import reactor
from twisted.internet.protocol import Protocol
from twisted.internet.serialport import SerialPort
from twisted.python import log
from davis_logger.davis import DavisWeatherStation
from davis_logger.util import toHexString

__author__ = 'david'

# 8 pm 10-feb-2012: \x4a\x1a\xd0\x07

class DavisLoggerProtocol(Protocol):
    """
    Bridges communications with the weather station and handles logging data.
    """

    def __init__(self):
        self.station = DavisWeatherStation()

        # Subscribe to outgoing data event
        self.station.sendData += self._sendData

        # This is fired when ever a loop packet arrives
        self.station.loopDataReceived += self._loopPacketReceived

        # Called when ever new samples are ready (the DMPAFT / getSamples()
        # command has finished).
        self.station.dumpFinished += self._samplesArrived

        # Fired when ever the loop packet subscription expires.
        self.station.loopFinished += self._loopFinished

    def dataReceived(self, data):
        """
        Passes all received data on to the weather station.
        :param data: Data to pass on
        """
        #log.msg('>> {0}'.format(toHexString(data)))
        self.station.dataReceived(data)

    def _sendData(self, data):
        #log.msg('<< {0}'.format(toHexString(data)))
        self.transport.write(data)

    def connectionMade(self):
        """ Called to start logging data. """
        self.station.getLoopPackets(50)

    def _loopPacketReceived(self, loop):
        log.msg('LOOP: ' + repr(loop))

    def _loopFinished(self):
        log.msg('LOOP finished')

    def _samplesArrived(self, sampleList):
        log.msg('DMP finished: ' + repr(sampleList))

log.startLogging(sys.stdout)

SerialPort(DavisLoggerProtocol(), 'COM1', reactor, baudrate=19200)

reactor.run()