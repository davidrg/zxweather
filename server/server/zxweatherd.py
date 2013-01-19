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

INPUT_SHELL = 0
INPUT_COMMAND = 1

class ZxweatherShellProtocol(recvline.HistoricRecvLine):
    """
    The zxweather shell.
    """

    ps = ("$ ", "_ ")
    line_partial = ""

    def __init__(self,user):
        self.dispatcher = Dispatcher(
            lambda prompt: self.commandProcessorPrompter(prompt),
            lambda warning: self.commandProcessorWarning(warning),
            TABLE_SET_AUTHENTICATED
        )
        self.dispatcher.environment["username"] = user.username

        self.input_mode = INPUT_SHELL
        self.current_command = None

    def connectionMade(self):
        """
        Called when a new connection is made by a client.
        """
        recvline.HistoricRecvLine.connectionMade(self)

    def handle_RETURN(self):
        """
        Overrides the function in all parent classes to provide an
        implementation that allows commands to be split over multiple lines
        using the - character when in INPUT_SHELL mode.
        """
        # From HistoricRecvLine
        if self.lineBuffer:
            self.historyLines.append(''.join(self.lineBuffer))
            self.historyPosition = len(self.historyLines)

        # From RecvLine
        line = ''.join(self.lineBuffer)
        self.lineBuffer = []
        self.lineBufferIndex = 0
        self.terminal.nextLine()

        # Handle the continuation - character if we're in SHELL mode.
        if line.endswith("-") and self.input_mode == INPUT_SHELL:
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

    def executeCommand(self, command):
        """
        Executes the specified command.
        :param command: Command string to execute.
        """

        # Evaluate the command string
        partial_cmd = self.dispatcher.get_command(command)

        # If nothing to execute, do nothing.
        if partial_cmd is None:
            self.processFinished()
            return

        # Supply the command with functions to control its status, input, etc.
        self.current_command = partial_cmd(
            output_callback=lambda text: self.processOutput(text),
            finished_callback=lambda: self.processFinished(),
            halt_input_callback=lambda: self.haltInput(),
            resume_input_callback=lambda: self.resumeInput()
        )

        # Switch input over to the command
        self.input_mode = INPUT_COMMAND

        # and set it running.
        self.current_command.execute()

    def lineReceived(self, line):
        """
        Called when ever a line of data is received. This function then routes
        that line off to where ever it should go based on the input mode.
        :param line:
        :return:
        """
        if self.input_mode == INPUT_SHELL:
            self.executeCommand(line)
        elif self.input_mode == INPUT_COMMAND:
            if self.current_command is not None:
                self.current_command.lineReceived(line)

    def commandProcessorPrompter(self, prompt_string):
        """
        Prompts the user for further input when required parameters are missing.
        :param prompt_string: The string to display to the user for the prompt.
        :return: The users input.
        """
        # TODO: Create a new line
        # TODO: Write out prompt string
        # TODO: Get input somehow with a deferred.
        return None

    def commandProcessorWarning(self, warning):
        """
        Called by the command processor and dispatcher when an error or
        warning has occurred.
        :param warning: The warning or error text.
        """
        self.terminal.write(warning)
        self.terminal.nextLine()

    def processOutput(self, text):
        """
        Called when a process wants to output text.
        :param text: Text to output.
        """
        self.terminal.write(text)

    def processFinished(self):
        """
        Called when the process has finished executing.
        """
        self.input_mode = INPUT_SHELL
        self.showPrompt()

    def haltInput(self):
        """
        Called to halt all user input.
        """
        # TODO: switch off everything in keystrokeReceived except listening for ^Z
        pass

    def resumeInput(self):
        """
        Called to resume user input.
        """
        # TODO: undo whatever haltInput did.
        pass

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