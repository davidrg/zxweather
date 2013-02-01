# coding=utf-8
"""
zxweather shell daemon
"""
from twisted.application.service import MultiService
from server.database import database_connect
from server.dbupdates import listener_connect
from server.ssh import getSSHService
from server.telnet import getTelnetService

def setupDatabase(dsn):
    """
    Sets up database connections
    :param dsn: Database connection string
    :type dsn: str
    """

    database_connect(dsn)
    listener_connect(dsn)


def getServerService(dsn, ssh_config, telnet_config):
    """
    Gets the zxweatherd server service.
    :param dsn: Database connection string
    :type dsn: str
    :param ssh_config: SSH protocol configuration
    :type ssh_config: dict
    :param telnet_config: Telnet protocol configuration
    :type telnet_config: dict
    :return: Server service.
    """

    if ssh_config is None and telnet_config is None:
        raise Exception('No protocols enabled')

    setupDatabase(dsn)

    service = MultiService()

    if ssh_config is not None:
        sshService = getSSHService(**ssh_config)
        sshService.setServiceParent(service)

    if telnet_config is not None:
        telnetService = getTelnetService(**telnet_config)
        telnetService.setServiceParent(service)

    return service