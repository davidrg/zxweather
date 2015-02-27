# coding=utf-8
import ConfigParser

from zxw_push.zxw_push import getClientService

__author__ = 'david'

from zope.interface import implements

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker


class Options(usage.Options):
    optParameters = [
        ["config-file", "f", None, "Configuration file."],
    ]


class ZXWPushClientServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """
    implements(IServiceMaker, IPlugin)
    tapname = "weather_push"
    description = "Service for pushing data out to a remote weather server"
    options = Options

    def _readConfigFile(self, filename):
        S_DATABASE = 'database'
        S_RABBITMQ = 'rabbitmq'
        S_TRANSPORT = 'transport'
        S_SSH = 'ssh'

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
                mq_user = config.get(S_RABBITMQ, "user")
                mq_password = config.get(S_RABBITMQ, "password")
                mq_exchange = config.get(S_RABBITMQ, "exchange")

            if config.has_option(S_RABBITMQ, "virtual_host"):
                mq_vhost = config.get(S_RABBITMQ, "virtual_host")

        transport_type = config.get(S_TRANSPORT, "type")
        if transport_type not in ["ssh", "udp"]:
            raise Exception("Invalid transport type")

        encoding = config.get(S_TRANSPORT, "encoding")
        if encoding not in ["standard", "diff"]:
            raise Exception("Invalid encoding type")

        hostname = config.get(S_TRANSPORT, "hostname")
        port = config.getint(S_TRANSPORT, "port")

        ssh_user = None
        ssh_password = None
        ssh_host_key = None

        if transport_type == "ssh":
            ssh_user = config.get(S_SSH, "username")
            ssh_password = config.get(S_SSH, "password")

            if config.has_option(S_SSH, "host_key"):
                ssh_host_key = config.get(S_SSH, "host_key")

        return dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, transport_type, encoding, hostname, port, ssh_user, \
            ssh_password, ssh_host_key

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, transport_type, encoding, hostname, port, ssh_user, \
            ssh_password, ssh_host_key = self._readConfigFile(
                options['config-file'])

        # All OK. Go get the service.
        return getClientService(
            hostname,
            port,
            ssh_user,
            ssh_password,
            ssh_host_key,
            dsn,
            transport_type,
            encoding,
            mq_host,
            mq_port,
            mq_exchange,
            mq_user,
            mq_password,
            mq_vhost
        )


serviceMaker = ZXWPushClientServiceMaker()
