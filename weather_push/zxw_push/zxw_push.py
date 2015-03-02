# coding=utf-8
"""
weather push: A program for pushing weather data to a remote zxweatherd server.
"""
import socket
from twisted.application import internet, service
from twisted.internet import reactor
from twisted.python import log
#from twisted.python.logfile import DailyLogFile
from client.zxweather_server import ShellClientFactory, ZXDUploadClient
from client.weather_push_server import WeatherPushDatagramClient
from client.database import WeatherDatabase
from client.mq_receiver import RabbitMqReceiver
from server.DatagramServer import WeatherPushDatagramServer

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


def getClientService(hostname, port, username, password, host_key_fingerprint,
                   dsn, transport_type, mq_host, mq_port, mq_exchange,
                   mq_user, mq_password, mq_vhost, authorisation_code):
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
    :param transport_type: Transport type to use (ssh or udp)
    :type transport_type: str
    :param encoding: Data encoding to use (standard or diff)
    :type encoding: str
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
    """
    global database, mq_client
    log.msg('Connecting...')

    # log.startLogging(DailyLogFile.fromFullPath("log-file"), setStdout=False)

    database = WeatherDatabase(hostname, dsn)

    if transport_type == "ssh":
        # Connecting to a remote zxweather server via SSH
        _upload_client = ZXDUploadClient(
            client_finished, "weather-push")
    else:
        ip_address = socket.gethostbyname(hostname)
        # Connecting to a remote weather push server via UDP
        _upload_client = WeatherPushDatagramClient(
            ip_address, port, authorisation_code,
            database.get_last_confirmed_sample)

    database.LiveUpdate += _upload_client.send_live
    database.NewSample += _upload_client.send_sample
    database.EndOfSamples += _upload_client.flush_samples

    _upload_client.Ready += database.transmitter_ready
    _upload_client.ReceiptConfirmation += database.confirm_receipt

    if mq_host is not None:
        mq_client = RabbitMqReceiver(mq_user, mq_password, mq_vhost,
                                     mq_host, mq_port, mq_exchange)
        mq_client.LiveUpdate += _upload_client.send_live
        _upload_client.Ready += mq_client.connect

    if transport_type == "ssh":

        factory = ShellClientFactory(username, password, host_key_fingerprint,
                                     _upload_client)

        return internet.TCPClient(hostname, port, factory)
    else:
        return DatagramClientService(_upload_client)

def getServerService(dsn, interface, port):
    """
    Starts a WeatherPush server
    :param port: UDP port to listen on
    """
    datagram_server = WeatherPushDatagramServer(dsn)

    return internet.UDPServer(port, datagram_server, interface=interface)