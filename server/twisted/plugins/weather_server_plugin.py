# coding=utf-8
from server.zxweatherd import getServerSSHService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["ssh-port", "s", 4222, "The port the remote SSH service is listening on.", int],
        ["private-key-file", "p", None, "The username to authenticate with"],
        ["public-key-file", "u" , None, "The password to authenticate with"],
        ["passwords-file", "f", None, "Remote host SSH key fingerprint"],
        ["dsn", "d", None, "Database connection string"]
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

        # All OK. Go get the service.
        return getServerSSHService(
            int(options['ssh-port']),
            options['private-key-file'],
            options['public-key-file'],
            options['passwords-file'],
            options['dsn']
        )


serviceMaker = ZXWServerServiceMaker()
