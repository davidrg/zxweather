# coding=utf-8
"""
weather push: A program for pushing weather data to a remote zxweatherd server.
"""
import socket
from twisted.application import internet, service
from twisted.application.service import MultiService
from twisted.internet import reactor
from twisted.internet.protocol import ServerFactory, ReconnectingClientFactory
from twisted.python import log

from client.zxweather_server import ShellClientFactory, ZXDUploadClient
from client.weather_push_server import WeatherPushDatagramClient
from client.database import WeatherDatabase
from client.mq_receiver import RabbitMqReceiver
from server.DatagramServer import WeatherPushDatagramServer
from client.weather_push_server.tcp_client import WeatherPushProtocol
from server.tcp_server import WeatherPushTcpServer
from common.util import Event

__author__ = 'david'


def client_finished():
    """
    Called when the UploadClient has cleared its buffers and disconnected from
    the remote system. This is where we close database connections, etc.
    """
    # TODO: Disconnect from database and stop reactor.
    reactor.stop()


class DatagramClientService(service.Service):
    def __init__(self, protocol):
        self._protocol = protocol

    def startService(self):
        log.msg("Starting UDP Client service...")
        # Port 0 for any port (don't care which - we're only getting replies)
        self._port = reactor.listenUDP(0, self._protocol)

    def stopService(self):
        return self._port.stopListening()


class TcpClientFactory(ReconnectingClientFactory):
    def __init__(self, authorisation_code, last_confirmed_sample_func,
                 new_image_size, protocol_setup_func):
        self._authorisation_code = authorisation_code
        self._last_confirmed_sample_func = last_confirmed_sample_func
        self._new_image_size = new_image_size
        self._setup_protocol = protocol_setup_func

        self.NotReady = Event()

    def buildProtocol(self, addr):
        self.resetDelay()
        p = WeatherPushProtocol(self._authorisation_code,
                                self._last_confirmed_sample_func,
                                self._new_image_size)
        self._setup_protocol(p)
        return p

    def clientConnectionLost(self, connector, unused_reason):
        log.msg("Connection lost")
        self.NotReady.fire()
        ReconnectingClientFactory.clientConnectionLost(self, connector, unused_reason)

    def clientConnectionFailed(self, connector, reason):
        log.msg("Connection failed")
        self.NotReady.fire()
        ReconnectingClientFactory.clientCOnnectionFailed(self, connector, reason)


# This wraps up the eventual TCP Client as a service while its still connecting.
class TcpClientService(service.Service):
    def __init__(self, hostname, port, authorisation_code,
                 last_confirmed_sample_func, new_image_size):
        self._hostname = hostname
        self._port = port
        self._protocol = None
        self._samples = []
        self._images = []
        self._authorisation_code = authorisation_code
        self._last_confirmed_sample_func = last_confirmed_sample_func
        self._new_image_size = new_image_size

        # Event subscriptions
        self.Ready = Event()
        self.ReceiptConfirmation = Event()
        self.ImageReceiptConfirmation = Event()

        self._factory = None

        self._is_ready = False

    def startService(self):
        self._factory = TcpClientFactory(self._authorisation_code,
                                         self._last_confirmed_sample_func,
                                         self._new_image_size,
                                         self._setup_protocol)
        self._factory.NotReady += self._not_ready

        reactor.connectTCP(self._hostname, self._port, self._factory)

    def _setup_protocol(self, client):
        self._protocol = client
        for x in self.Ready.handlers:
            self._protocol.Ready += x

        for x in self.ReceiptConfirmation.handlers:
            self._protocol.ReceiptConfirmation += x

        for x in self.ImageReceiptConfirmation.handlers:
            self._protocol.ImageReceiptConfirmation += x

        client.Ready += self._ready

    # noinspection PyUnusedLocal
    def _ready(self, not_used):
        self._is_ready = True
        self._flush_buffers()

    def _not_ready(self):
        self._is_ready = False

    def _flush_buffers(self):
        if self._protocol is None or not self._is_ready:
            return

        while len(self._samples) > 0:
            s = self._samples.pop(0)
            self._protocol.send_sample(s[0], s[1], s[2])

        while len(self._images) > 0:
            i = self._images.pop(0)
            self._protocol.send_image(i)

    def send_live(self, live, hardware_type):
        if self._protocol is not None:
            self._protocol.send_live(live, hardware_type)

    def send_sample(self, sample, hardware_type, hold=False):
        self._samples.append((sample, hardware_type, hold))

        self._flush_buffers()

    def send_image(self, image):
        self._images.append(image)

        self._flush_buffers()

    def flush_samples(self):
        # The TCP ignores this.
        pass

    def stopService(self):
        return self._protocol.transport.looseConnection()


def getClientService(hostname, port, username, password, host_key_fingerprint,
                     dsn, transport_type, mq_host, mq_port, mq_exchange,
                     mq_user, mq_password, mq_vhost, authorisation_code,
                     resize_images, new_image_size, tcp_port):
    """
    Connects to a remote WeatherPush server or zxweather daemon
    :param hostname: Remote host to connect to
    :type hostname: str
    :param port: port number
    :type port: int
    :param username: Username to authenticate with.
    :type username: str
    :param password: Password to authenticate with.
    :type password: str
    :param host_key_fingerprint: The expected host key for the server. If this
    is None then the servers key will not be verified.
    :type host_key_fingerprint: str or None
    :param dsn: Database connection string
    :type dsn: str
    :param transport_type: Transport type to use (ssh, udp or tcp)
    :type transport_type: str
    :param mq_host: RabbitMQ hostname
    :type mq_host: str
    :param mq_port: RabbitMQ port
    :type mq_port: int
    :param mq_exchange: RabbitMQ Exchange
    :type mq_exchange: str
    :param mq_user: RabbitMQ Username
    :type mq_user: str
    :param mq_password: RabbitMQ Password
    :type mq_password: str
    :param mq_vhost: RabbitMQ virtual host
    :type mq_vhost: str
    :param authorisation_code: Authorisation code to use for server
    :type authorisation_code: int
    :param resize_images: If images should be resized before transmission
    :type resize_images: bool
    :param new_image_size: New size for images
    :type new_image_size: (int, int)
    :param tcp_port: TCP Port to send images over
    :type tcp_port: int
    """
    global database, mq_client
    log.msg('Connecting...')

    database = WeatherDatabase(hostname, dsn)

    if not resize_images:
        new_image_size = None

    if transport_type == "ssh":
        # Connecting to a remote zxweather server via SSH
        _upload_client = ZXDUploadClient(
            client_finished, "weather-push")
    elif transport_type == "udp":
        ip_address = socket.gethostbyname(hostname)

        # This function creates and starts a TCP Client instance for handling
        # image traffic only. It doesn't hook up any events related to live
        # or sample data. This function will be called the first time the UDP
        # client is asked to transmit an image. If this never happens the client
        # is never created.
        def _make_tcp_service():
            tcp_svc = TcpClientService(hostname, tcp_port, authorisation_code,
                                       database.get_last_confirmed_sample,
                                       new_image_size)
            tcp_svc.ImageReceiptConfirmation += \
                database.confirm_image_receipt

            tcp_svc.startService()

            return tcp_svc

        # Connecting to a remote weather push server via UDP
        _upload_client = WeatherPushDatagramClient(
            ip_address, port, authorisation_code,
            database.get_last_confirmed_sample, _make_tcp_service)
    else:
        _upload_client = TcpClientService(
                hostname, port, authorisation_code,
                database.get_last_confirmed_sample, new_image_size)

    database.LiveUpdate += _upload_client.send_live
    database.NewSample += _upload_client.send_sample
    database.EndOfSamples += _upload_client.flush_samples

    if transport_type != "ssh":
        # The SSH transport doesn't support sending images at all.
        database.NewImage += _upload_client.send_image

    _upload_client.Ready += database.transmitter_ready
    _upload_client.ReceiptConfirmation += database.confirm_receipt
    _upload_client.ImageReceiptConfirmation += database.confirm_image_receipt

    if mq_host is not None:
        mq_client = RabbitMqReceiver(mq_user, mq_password, mq_vhost,
                                     mq_host, mq_port, mq_exchange)
        mq_client.LiveUpdate += _upload_client.send_live
        _upload_client.Ready += mq_client.connect

    if transport_type == "ssh":
        factory = ShellClientFactory(username, password, host_key_fingerprint,
                                     _upload_client)

        return internet.TCPClient(hostname, port, factory)
    elif transport_type == "udp":
        return DatagramClientService(_upload_client)
    else:
        return _upload_client


class TcpServerFactory(ServerFactory):

    protocol = WeatherPushTcpServer

    def __init__(self, dsn, authorisation_code):
        self._dsn = dsn
        self._authorisation_code = authorisation_code

    def buildProtocol(self, addr):
        p = WeatherPushTcpServer(self._authorisation_code)
        p.factory = self
        p.start_protocol(self._dsn)
        return p


def getServerService(dsn, interface, port, tcp_port, authorisation_code):
    """
    Starts a WeatherPush server
    :param port: UDP port to listen on
    """
    datagram_server = WeatherPushDatagramServer(dsn, authorisation_code)

    tcp_factory = TcpServerFactory(dsn, authorisation_code)

    udp_server = internet.UDPServer(port, datagram_server, interface=interface)

    tcp_server = internet.TCPServer(tcp_port, tcp_factory, interface=interface)

    svc = MultiService()
    svc.addService(udp_server)
    svc.addService(tcp_server)

    return svc