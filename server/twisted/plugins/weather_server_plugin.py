# coding=utf-8
from server.zxweatherd import getServerService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["ssh-port", "s", 4222, "The port the SSH service should listening on.", int],
        ["private-key-file", "p", None, "SSH Private key file"],
        ["public-key-file", "u" , None, "SSH Public key file"],
        ["passwords-file", "f", None, "Name of the file containing usernames and passwords"],
        ["dsn", "d", None, "Database connection string"],
        ["telnet-port", "t", 4223, "The port the Telnet service should listen on", int],
        ["standard-port", "r", 4224, "The port the standard weather service should listen on", int]
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

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        # Check for required parameters.
        if options["private-key-file"] is None:
            raise Exception('SSH Private key filename required')
        elif options["public-key-file"] is None:
            raise Exception('SSH Public key filename required')
        elif options["passwords-file"] is None:
            raise Exception('SSH passwords required')
        elif options["dsn"] is None:
            raise Exception('Database connection string required')


        ssh_config = {
            'port': int(options['ssh-port']),
            'private_key_file': options['private-key-file'],
            'public_key_file': options['public-key-file'],
            'passwords_file': options['passwords-file']
        }

        telnet_config = {'port': options['telnet-port']}

        raw_config = {'port': options['standard-port']}

        # All OK. Go get the service.
        return getServerService(
            options['dsn'], ssh_config, telnet_config, raw_config)


serviceMaker = ZXWServerServiceMaker()
