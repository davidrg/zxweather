# coding=utf-8
import ConfigParser
from davis_logger.logger import DavisService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["config-file", "f", None, "Configuration file."],
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

    def _readConfigFile(self, filename):
        S_DATABASE = 'database'
        S_RABBITMQ = 'rabbitmq'
        S_STATION = 'station'
        S_LOGGER = 'logger'
        S_DST = 'dst'

        config = ConfigParser.ConfigParser()
        config.read([filename])

        dsn = config.get(S_DATABASE, 'dsn')

        mq_host = None
        mq_port = None
        mq_exchange = None
        mq_user = None
        mq_password = None
        mq_vhost = "/"

        if config.has_section(S_RABBITMQ):
            if config.has_option(S_RABBITMQ, "host") \
                    and config.has_option(S_RABBITMQ, "port"):
                mq_host = config.get(S_RABBITMQ, "host")
                mq_port = config.getint(S_RABBITMQ, "port")
                mq_user = config.get(S_RABBITMQ, "username")
                mq_password = config.get(S_RABBITMQ, "password")
                mq_exchange = config.get(S_RABBITMQ, "exchange")

            if config.has_option(S_RABBITMQ, "virtual_host"):
                mq_vhost = config.get(S_RABBITMQ, "virtual_host")

        station_code = config.get(S_STATION, "station_code")
        serial_port = config.get(S_LOGGER, "serial_port")
        baud_rate = config.getint(S_LOGGER, "baud_rate")
        sample_error_file = config.get(S_LOGGER, "sample_error_file")

        auto_dst = False
        time_zone = ""
        if config.has_section(S_DST) and config.has_option(S_DST, "auto_dst"):
            auto_dst = config.getboolean(S_DST, "auto_dst")

        if auto_dst:
            time_zone = config.get(S_DST, "time_zone")

        return dsn, station_code, serial_port, baud_rate, sample_error_file, \
               auto_dst, time_zone, mq_host, mq_port, mq_exchange, mq_user, \
               mq_password, mq_vhost

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        # Check for required parameters.
        if options["config-file"] is None:
            raise Exception('Configuration file required')

        dsn, station_code, serial_port, baud_rate, sample_error_file, auto_dst, \
            time_zone, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost = self._readConfigFile(options['config-file'])

        return DavisService(dsn, station_code, serial_port, baud_rate,
                            sample_error_file, auto_dst, time_zone, mq_host,
                       mq_port, mq_exchange, mq_user, mq_password,
                       mq_vhost)


serviceMaker = DavisLoggerServiceMaker()
