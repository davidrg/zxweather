# coding=utf-8
"""
SSH protocol support for zxweatherd.
"""
__author__ = 'david'

from twisted.application import internet
from twisted.conch.ssh.keys import Key
from twisted.cred import portal as cred_portal
from twisted.conch import  avatar, interfaces as conch_interfaces
from twisted.conch.ssh import factory, session
from twisted.conch.insults import insults
from twisted.cred.checkers import FilePasswordDB
from zope.interface import implementer

from server.shell import ZxweatherShellProtocol


@implementer(conch_interfaces.ISession)
class ZxwAvatar(avatar.ConchUser):

    def __init__(self, username):
        avatar.ConchUser.__init__(self)
        self.username = username
        self.channelLookup.update({b'session':session.SSHSession})
        self._server_protocol = None

    def openShell(self, protocol):
        """
        Starts the zxweather shell.
        :param protocol: The transport to use.
        """
        serverProtocol = insults.ServerProtocol(ZxweatherShellProtocol, self, "ssh")
        serverProtocol.makeConnection(protocol)
        protocol.makeConnection(session.wrapProtocol(serverProtocol))
        self._server_protocol = serverProtocol

    def getPty(self, terminal, windowSize, attrs):
        """
        Not used but we have to stub it out otherwise the client will likely
        throw errors.
        :param terminal:
        :param windowSize:
        :param attrs:
        """
        return None

    def execCommand(self, protocol, cmd):
        """ Not implemented.
        :param protocol:
        :param cmd:
        :return:
        """
        raise NotImplementedError

    def closed(self):
        """ Lets the shell know the session is dead. """
        self._server_protocol.connectionLost(None)

    def eofReceived(self):
        # This is here otherwise we get an error on disconnect.
        pass


@implementer(cred_portal.IRealm)
class ZxwRealm(object):

    def requestAvatar(self, avatarId, mind, *interfaces):
        if conch_interfaces.IConchUser in interfaces:
            return interfaces[0], ZxwAvatar(avatarId), lambda: None
        else:
            raise Exception("Invalid interface requested in ZxwRealm")


def getSSHService(port, private_key_file, public_key_file, passwords_file):
    """
    Gets the zxweatherd SSH Service.
    :param port: Port to listen on
    :type port: int
    :param private_key_file: Private key filename
    :type private_key_file: str
    :param public_key_file: Public key filename
    :type public_key_file: str
    :param passwords_file: File to read usernames and passwords from
    :type passwords_file: str
    """
    with open(private_key_file) as privateBlobFile:
        privateBlob = privateBlobFile.read()
        privateKey = Key.fromString(data=str(privateBlob))

    with open(public_key_file) as publicBlobFile:
        publicBlob = publicBlobFile.read()
        publicKey = Key.fromString(data=str(publicBlob))

    sshFactory = factory.SSHFactory()
    sshFactory.privateKeys = {b'ssh-rsa': privateKey}
    sshFactory.publicKeys = {b'ssh-rsa': publicKey}
    sshFactory.portal = cred_portal.Portal(ZxwRealm())
    sshFactory.portal.registerChecker(FilePasswordDB(passwords_file))

    return internet.TCPServer(port, sshFactory)