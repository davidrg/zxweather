# coding=utf-8
"""
weather push: A program for pushing weather data to a remote zxweatherd server.
"""
from twisted.internet import reactor
from twisted.python import log
from twisted.python.logfile import DailyLogFile
from ssh_client import connect
from upload_client import UploadClient
from database import WeatherDatabase

__author__ = 'david'


def client_ready():
    """
    Called when the UploadClient is ready to receive data. This is where we
    connect to the database, etc.
    """
    print('Client ready.')
    database.connect(
        "host=titan.rua.zx.net.nz port=5432 user=zxweather password=password dbname=weather")

def client_finished():
    """
    Called when the UploadClient has cleared its buffers and disconnected from
    the remote system. This is where we close database connections, etc.
    """
    # TODO: Disconnect from database and stop reactor.
    reactor.stop()



if __name__ == '__main__':

    log.startLogging(DailyLogFile.fromFullPath("log-file"), setStdout=False)

    _upload_client = UploadClient(
        client_finished, client_ready, "weather-push")

    database = WeatherDatabase(_upload_client)

    # If anything goes wrong in the client connection the reactor will be
    # stopped automatically. Or at least that's how its supposed to work.
    connect("localhost", 22, "admin","",
        '03:ab:56:b4:92:66:01:3d:0d:53:3f:fe:0a:80:5e:35', _upload_client)

    reactor.run()