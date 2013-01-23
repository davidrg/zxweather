# coding=utf-8
"""
zxweather shell daemon
"""
from twisted.conch.ssh.keys import Key
from twisted.cred import portal as cred_portal
from twisted.conch import  avatar, interfaces as conch_interfaces
from twisted.conch.ssh import factory, session
from twisted.conch.insults import insults
from twisted.cred.checkers import FilePasswordDB
from zope.interface import implements
from twisted.internet import reactor
from server.database import database_connect
from server.dbupdates import listener_connect
from server.shell import ZxweatherShellProtocol


class ZxwAvatar(avatar.ConchUser):
    implements(conch_interfaces.ISession)

    def __init__(self, username):
        avatar.ConchUser.__init__(self)
        self.username = username
        self.channelLookup.update({'session':session.SSHSession})

    def openShell(self, protocol):
        """
        Starts the zxweather shell.
        :param protocol: The transport to use.
        """
        serverProtocol = insults.ServerProtocol(ZxweatherShellProtocol, self)
        serverProtocol.makeConnection(protocol)
        protocol.makeConnection(session.wrapProtocol(serverProtocol))

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
        """ Not implemented. """
        pass


class ZxwRealm(object):
    implements(cred_portal.IRealm)

    def requestAvatar(self, avatarId, mind, *interfaces):
        if conch_interfaces.IConchUser in interfaces:
            return interfaces[0], ZxwAvatar(avatarId), lambda: None
        else:
            raise Exception("Invalid interface requested in ZxwRealm")

with open('../id_rsa') as privateBlobFile:
    privateBlob = privateBlobFile.read()
    privateKey = Key.fromString(data=str(privateBlob))

with open('../id_rsa.pub') as publicBlobFile:
    publicBlob = publicBlobFile.read()
    publicKey = Key.fromString(data=str(publicBlob))

sshFactory = factory.SSHFactory()
sshFactory.privateKeys = {'ssh-rsa': privateKey}
sshFactory.publicKeys = {'ssh-rsa': publicKey}
sshFactory.portal = cred_portal.Portal(ZxwRealm())
sshFactory.portal.registerChecker(FilePasswordDB("ssh-passwords"))

conn_str = "host=localhost port=5432 user=zxweather password=password dbname=weather"
database_connect(conn_str)
listener_connect(conn_str)

reactor.listenTCP(22, sshFactory)
reactor.run( )