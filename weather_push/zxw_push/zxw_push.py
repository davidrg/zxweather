# coding=utf-8
"""
weather push: A program for pushing weather data to a remote zxweatherd server.
"""
from twisted.application import internet
from twisted.internet import reactor
from twisted.python import log
from twisted.python.logfile import DailyLogFile
from ssh_client import ShellClientFactory
from upload_client import UploadClient
from database import WeatherDatabase

__author__ = 'david'


def client_ready(dsn):
    """
    Called when the UploadClient is ready to receive data. This is where we
    connect to the database, etc.
    :param dsn: Data source name
    :type dsn: str
    """
    print('Client ready.')
    database.connect(dsn)

def client_finished():
    """
    Called when the UploadClient has cleared its buffers and disconnected from
    the remote system. This is where we close database connections, etc.
    """
    # TODO: Disconnect from database and stop reactor.
    reactor.stop()


def getPushService(hostname, port, username, password, host_key_fingerprint, dsn):
    """
    Connects to the remote server.
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
    """
    global database
    log.msg('Connecting...')

    #log.startLogging(DailyLogFile.fromFullPath("log-file"), setStdout=False)

    _upload_client = UploadClient(
        client_finished, lambda : client_ready(dsn), "weather-push")

    database = WeatherDatabase(_upload_client)

    factory = ShellClientFactory(username, password, host_key_fingerprint,
        _upload_client)

    return internet.TCPClient(hostname, port, factory)
