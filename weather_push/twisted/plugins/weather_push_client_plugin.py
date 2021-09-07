# coding=utf-8
try:
    from ConfigParser import ConfigParser
except ImportError:
    from configparser import ConfigParser

from zxw_push.zxw_push import getClientService

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
class ZXWPushClientServiceMaker(object):
    """
    Creates the zxweather WeatherPush service. Or, rather, prepares it from the
    supplied command-line arguments.
    """

    tapname = "weather_push_client"
    description = "Service for pushing data out the weather push server or a " \
                  "remote zxweather server"
    options = Options

    def _readConfigFile(self, filename):
        S_DATABASE = 'database'
        S_RABBITMQ = 'rabbitmq'
        S_TRANSPORT = 'transport'
        S_SSH = 'ssh'
        S_IMAGES = 'images'

        config = ConfigParser()
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
        if transport_type not in ["ssh", "udp", "tcp"]:
            raise Exception("Invalid transport type")

        hostname = config.get(S_TRANSPORT, "hostname")
        port = config.getint(S_TRANSPORT, "port")

        ssh_user = None
        ssh_password = None
        ssh_host_key = None

        authorisation_code = None

        if transport_type == "ssh":
            ssh_user = config.get(S_SSH, "username")
            ssh_password = config.get(S_SSH, "password")

            if config.has_option(S_SSH, "host_key"):
                ssh_host_key = config.get(S_SSH, "host_key")
        elif transport_type == "udp" or transport_type == "tcp":
            authorisation_code = config.getint(S_TRANSPORT, "authorisation_code")

        tcp_port = config.getint(S_IMAGES, "tcp_port")
        resize_images = config.getboolean(S_IMAGES, "resize_images")
        new_width = config.getint(S_IMAGES, "new_width")
        new_height = config.getint(S_IMAGES, "new_height")
        new_size = (new_width, new_height)
        resize_source_str = config.get(S_IMAGES, "resize_image_sources")
        resize_sources = []
        if len(resize_source_str) > 0:
            if "," in resize_source_str:
                resize_sources = resize_source_str.split(",")
                resize_sources = [x.strip().upper() for x in resize_sources if len(x.strip()) > 0]
            else:
                resize_sources = [resize_source_str.strip().upper(),]

        return dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, transport_type, hostname, port, ssh_user, \
            ssh_password, ssh_host_key, authorisation_code, resize_images, \
            new_size, tcp_port, resize_sources

    def makeService(self, options):
        """
        Construct the TCPService.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, transport_type, hostname, port, ssh_user, \
            ssh_password, ssh_host_key, authorisation_code, resize_images,\
            new_size, tcp_port, resize_sources = self._readConfigFile(
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
            mq_host,
            mq_port,
            mq_exchange,
            mq_user,
            mq_password,
            mq_vhost,
            authorisation_code,
            resize_images,
            new_size,
            tcp_port,
            resize_sources
        )


serviceMaker = ZXWPushClientServiceMaker()
