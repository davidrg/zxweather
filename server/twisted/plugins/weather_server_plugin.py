# coding=utf-8
import ConfigParser
from server.zxweatherd import getServerService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["config-file", "f", None, "Configuration file."],
    ]


class ZXWServerServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """
    implements(IServiceMaker, IPlugin)
    tapname = "weather_server"
    description = "Service for providing access to weather data"
    options = Options

    def _readConfigFile(self, filename):
        S_SSH = 'ssh'
        S_TELNET = 'telnet'
        S_RAW = 'raw'
        S_WS = 'websocket'
        S_WSS = 'websocket-ssl'
        S_DATABASE = 'database'

        config = ConfigParser.ConfigParser()
        config.read([filename])

        ssh_config = None
        telnet_config = None
        raw_config = None
        ws_config = None
        wss_config = None
        dsn = config.get(S_DATABASE, 'dsn')

        if config.has_section(S_SSH) and config.has_option(S_SSH, 'enable') \
            and config.getboolean(S_SSH, 'enable'):
            ssh_config = {
                'port': config.getint(S_SSH, 'port'),
                'private_key_file': config.get(S_SSH, 'private_key_file'),
                'public_key_file': config.get(S_SSH, 'public_key_file'),
                'passwords_file': config.get(S_SSH, 'passwords_file')
            }

        if config.has_section(S_TELNET) \
                and config.has_option(S_TELNET, 'enable') \
                and config.getboolean(S_TELNET, 'enable'):
            telnet_config = {
                'port': config.getint(S_TELNET, 'port')
            }

        if config.has_section(S_RAW) and config.has_option(S_RAW, 'enable') \
                and config.getboolean(S_RAW, 'enable'):
            raw_config = {
                'port': config.getint(S_RAW, 'port')
            }

        if config.has_section(S_WS) and config.has_option(S_WS, 'enable') \
                and config.getboolean(S_WS, 'enable'):
            ws_config = {
                'port': config.getint(S_WS, 'port')
            }

        if config.has_section(S_WSS) and config.has_option(S_WSS, 'enable') \
                and config.getboolean(S_WSS, 'enable'):

            chain = None
            if config.has_option(S_WSS, 'chain_file'):
                chain = config.get(S_WSS, 'chain_file')
            if config.has_option(S_WSS, 'certificate_file'):
                cert = config.get(S_WSS, 'certificate_file')

            wss_config = {
                'port': config.getint(S_WSS, 'port'),
                'key': config.get(S_WSS, 'private_key_file'),
                'certificate': cert,
                'chain': chain,
            }

        return ssh_config, telnet_config, raw_config, ws_config, wss_config, dsn

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        # Check for required parameters.
        if options["config-file"] is None:
            raise Exception('Configuration file required')

        ssh_config, telnet_config, raw_config, websocket_config, wss_config, \
            dsn = self._readConfigFile(options['config-file'])


        # All OK. Go get the service.
        return getServerService(
            dsn, ssh_config, telnet_config, raw_config,
            websocket_config, wss_config)


serviceMaker = ZXWServerServiceMaker()
