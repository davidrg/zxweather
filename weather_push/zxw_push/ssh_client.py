# coding=utf-8
"""
Connects to the server via SSH and then forwards all data off to the
supplied protocol.

As a side note: There is an awful lot of boiler-plate code required to get this
thing going. Just running the operating systems ssh client (or plink.exe for
windows) would have probably been easier.
"""
__author__ = 'david'

from twisted.internet.error import ConnectionLost, ConnectionDone
from twisted.python import log
from twisted.conch.ssh import transport, connection, userauth, channel
from twisted.internet import defer, protocol, reactor







#STG_INIT = -1
#STG_SET_PROMPT = 0
#STG_SET_TERM = 1
#STG_SET_CLIENT = 2
#STG_GET_RECENT = 3
#STG_UPLOAD = 4
#STG_QUIT = 5

class ShellChannel(channel.SSHChannel):
    name = 'session'

    def __init__(self, client, *args, **kwargs):
        channel.SSHChannel.__init__(self, *args, **kwargs)
        self._client = client

    def channelOpen(self, data):
        self._client.makeConnection(self)
        self.conn.sendRequest(
            self, 'shell', "")


    def dataReceived(self, data):
        self._client.dataReceived(data)


class PasswordAuth(userauth.SSHUserAuthClient):
    """
    Authenticates using the supplied username and password.
    """
    def __init__(self, user, password, connection):
        userauth.SSHUserAuthClient.__init__(self, user, connection)
        self._password = password

    def getPassword(self, prompt=None):
        return defer.succeed(self._password)

class ClientConnection(connection.SSHConnection):
    def __init__(self, client, *args, **kwargs):
        connection.SSHConnection.__init__(self)
        self._client = client

    def serviceStarted(self):
        self.openChannel(ShellChannel(conn=self, client=self._client))


class KeyValidationError(Exception):
    pass

class ShellClientTransport(transport.SSHClientTransport):
    """
    An ssh client transport that authenticates using a supplied username
    and password. Additionally it will validate a key fingerprint if it is
    supplied with one.
    """
    def __init__(self, username, password, key_fingerprint, client):
        self._username = username
        self._password = password
        self._key_fingerprint = key_fingerprint
        self._client = client

    def verifyHostKey(self, pubKey, fingerprint):
        log.msg('Host key fingerprint: {0}'.format(fingerprint))
        if self._key_fingerprint is None:
            log.msg('Warning: host key not verified. Add trusted host key '
                    'fingerprint to configuration file')
            return defer.succeed(True)

        if self._key_fingerprint == fingerprint:
            log.msg('Host key fingerprint matches')
            return defer.succeed(True)
        else:
            raise KeyValidationError('Host key fingerprint mismatch')

    def connectionSecure(self):
        self.requestService(
            PasswordAuth(self._username, self._password, ClientConnection(self._client))
        )

class ShellClientFactory(protocol.ClientFactory):
    """
    A factory to create new SSH transport clients which will request a shell
    service on the remote host.
    """
    def __init__(self, username, password, host_key_fingerprint, client):
        self._username = username
        self._password = password
        self._host_key_fingerprint = host_key_fingerprint
        self._client = client

    def buildProtocol(self, addr):
        """
        Constructs the a new SSHClientTransport that authenticates using the
        supplied username and password.
        """
        return ShellClientTransport(
            self._username, self._password, self._host_key_fingerprint,
            self._client)

    def clientConnectionLost(self, connector, reason):
        """
        Called when ever the client connection is lost. Here we can reconnect
        or kill the connection completely if the failure reason is quite
        serious.
        """

        # Let the client know the connection has gone away.
        self._client.reset()

        def _auto_reconnect():
            log.msg('Attempting reconnect...')
            connector.connect()

        if reason.check(KeyValidationError):
            # The server responded with a key we were not expecting. This is
            # quite serious (or would be if we were doing anything sensitive)
            # so we'll kill the connection entirely. The user needs to check
            # their configuration.
            log.err(reason)
            log.msg('Connection aborted due to key host key validation error. '
                    'Check your configuration.')
            reactor.stop()
        elif reason.check(ConnectionLost):
            # We lost the connection for some reason. Reconnect.

            if not reactor.running:
                log.msg('Connection lost due to server shutdown.')
                return

            log.msg('Reconnect...')
            connector.connect()
        elif reason.check(ConnectionDone):
            log.msg('Server has closed the connection (server shut down?).')
            log.msg('Reconnect attempt in 1 minute...')
            reactor.callLater(60, _auto_reconnect)
        else:
            log.msg('An unexpected error caused client disconnect.')
            reactor.stop()

    def clientConnectionFailed(self, connector, reason):
        log.msg('Connect failed. Reason: {0}'.format(reason.getErrorMessage()))
        log.msg('Reconnect attempt in 1 minute...')

        def _auto_reconnect():
            log.msg('Attempting reconnect...')
            connector.connect()

        reactor.callLater(60, _auto_reconnect)