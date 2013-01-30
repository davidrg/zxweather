# coding=utf-8
__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker
from twisted.application import internet

from zxw_push.zxw_push import getPushService


class Options(usage.Options):
    optParameters = [
        ["host", 'h', None, "The remote hostname or IP address"],
        ["port", "p", 22, "The port the remote SSH service is listening on.", int],
        ["username", "u", None, "The username to authenticate with"],
        ["password", "a", "", "The password to authenticate with"],
        ["key_fingerprint", "k", None, "Remote host SSH key fingerprint"]
    ]


class ZXWPushServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """
    implements(IServiceMaker, IPlugin)
    tapname = "weather_push"
    description = "Service for pushing data out to a remote weather server"
    options = Options

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        # Check for required parameters.
        if options["host"] is None:
            raise Exception('Hostname is required')
        elif options["username"] is None:
            raise Exception('Username required')

        # All OK. Go get the service.
        return getPushService(
            options["hostname"],
            int(options["port"]),
            options["username"],
            options["password"],
            options["key_fingerprint"]
        )

serviceMaker = ZXWPushServiceMaker()
