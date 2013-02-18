# coding=utf-8
from davis_logger.logger import DavisService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["dsn", "d", None, "Database connection string"],
        ["station_code", "s", None,
         "The code for the station to log data against."],
        ["serial_port", "p", None,
         "The serial port device the weather station is attached to."],
        ["baud_rate", "b", 19200, "Serial baud rate", int],
    ]


class DavisLoggerServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """
    implements(IServiceMaker, IPlugin)
    tapname = "davis_logger"
    description = "Service logging data from a Davis Weather Station"
    options = Options

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        # Check for required parameters.
        if options["station_code"] is None:
            raise Exception('Station code required')
        elif options["serial_port"] is None:
            raise Exception('Serial port device required')
        elif options["dsn"] is None:
            raise Exception('Database connection string required')


        dsn = options["dsn"]
        station_code = options["station_code"]
        serial_port = options["serial_port"]
        baud_rate = options["baud_rate"]

        return DavisService(dsn, station_code, serial_port, baud_rate)

serviceMaker = DavisLoggerServiceMaker()
