# coding=utf-8
try:
    from ConfigParser import ConfigParser
except ImportError:
    from configparser import ConfigParser

from zxw_push.zxw_push import getServerService

__author__ = 'david'

from zope.interface import implementer

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["config-file", "f", None, "Configuration file."],
    ]


@implementer(IServiceMaker, IPlugin)
class ZXWPushServerServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """

    tapname = "weather_push_server"
    description = "Service for receiving data from a remote weather station"
    options = Options

    def _readConfigFile(self, filename):
        S_DATABASE = 'database'
        S_TRANSPORT = 'transport'
        S_SECURITY = 'security'

        config = ConfigParser()
        config.read([filename])

        dsn = config.get(S_DATABASE, 'dsn')

        interface = config.get(S_TRANSPORT, "interface")
        port = config.getint(S_TRANSPORT, "port")

        tcp_port = config.getint(S_TRANSPORT, "tcp_port")

        auth_code = config.getint(S_SECURITY, "authorisation_code")

        return dsn, interface, port, tcp_port, auth_code

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        dsn, interface, port, tcp_port, auth_code = \
            self._readConfigFile(options['config-file'])

        # All OK. Go get the service.
        return getServerService(
            dsn, interface, port, tcp_port, auth_code
        )


serviceMaker = ZXWPushServerServiceMaker()
