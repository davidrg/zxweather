# coding=utf-8
"""
zxweather shell daemon
"""
from twisted.conch.ssh.keys import Key
from twisted.cred import portal as cred_portal
from twisted.conch import  avatar, recvline, interfaces as conch_interfaces
from twisted.conch.ssh import factory, session
from twisted.conch.insults import insults
from twisted.cred.checkers import FilePasswordDB
from zope.interface import implements
from twisted.internet import reactor
from server.shell import Dispatcher, TABLE_SET_AUTHENTICATED, TABLE_SET_SHELL_INIT

class ZxweatherShellProtocol(recvline.HistoricRecvLine):
    """
    The zxweather shell.
    """

    ps = ("$ ", "_ ")
    line_partial = ""

    def __init__(self,user):
        self.dispatcher = Dispatcher(
            lambda prompt: self.prompter(prompt),
            lambda warning: self.cmd_proc_warning(warning),
            TABLE_SET_SHELL_INIT,
            lambda text: self.process_output(text),
            lambda : self.process_input()
        )
        self.dispatcher.execute_command('set_env "username" "{0}"'.format(
            user.username.replace('"',r'\"')))

        self.dispatcher.switch_table_set(TABLE_SET_AUTHENTICATED)

    def connectionMade(self):
        recvline.HistoricRecvLine.connectionMade(self)

    def handle_RETURN(self):
        # From HistoricRecvLine
        if self.lineBuffer:
            self.historyLines.append(''.join(self.lineBuffer))
            self.historyPosition = len(self.historyLines)

        # From RecvLine
        line = ''.join(self.lineBuffer)
        self.lineBuffer = []
        self.lineBufferIndex = 0
        self.terminal.nextLine()

        # Handle the continuation \ character
        if line.endswith("\\"):
            self.line_partial += line[:-1]
            if not self.line_partial.endswith(" "):
                self.line_partial += " "
            self.pn = 1
            self.showPrompt()
        else:
            line = self.line_partial + line
            self.line_partial = ""
            self.pn = 0
            self.lineReceived(line)

    def showPrompt(self):
        """
        Shows the command prompt.
        """
        self.terminal.write(self.ps[self.pn])

    def lineReceived(self, line):
        self.dispatcher.execute_command(line)
        #self.terminal.write("\r\nCommand: " + line + "\r\n")
        self.showPrompt()

    def prompter(self, prompt_string):
        #TODO: Implement me
        return "foo"

    def cmd_proc_warning(self, warning):
        self.terminal.write(warning)
        self.terminal.nextLine()

    def process_input(self):
        """
        Called when a process wants input.
        """
        return ""

    def process_output(self, text):
        """
        Called when a process wants to output text.
        :param text: Text to output.
        """
        self.terminal.write(text)

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


reactor.listenTCP(22, sshFactory)
reactor.run( )