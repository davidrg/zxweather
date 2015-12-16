# coding=utf-8
"""
WebSocket support for zxweatherd.
"""
import txaio

# Autobahn moved stuff around at some point.
try:
    from autobahn.twisted.websocket import WebSocketServerProtocol, WebSocketServerFactory
except:
    from autobahn.websocket import WebSocketServerProtocol, WebSocketServerFactory

from twisted.application import internet
from twisted.internet import ssl

from OpenSSL import SSL

from server.session import end_session
from server.shell import BaseShell

__author__ = 'david'


class ChainedOpenSSLContextFactory(ssl.DefaultOpenSSLContextFactory):
    def __init__(self, privateKeyFileName, certificateFileName,
                 certificateChainFileName):
        """
        :param privateKeyFileName: Name of a file containing a private key
        :param certificateFileName: Name of a file containing a certificate
        :param certificateChainFileName: Name of a file containing a
                                        certificate chain
        :param sslmethod: The SSL method to use
        """
        self.privateKeyFileName = privateKeyFileName
        self.certificateFileName = certificateFileName
        self.certificateChainFileName = certificateChainFileName
        self.cacheContext()

    def cacheContext(self):
        ctx = SSL.Context(SSL.SSLv23_METHOD)
        if self.certificateFileName is not None:
            ctx.use_certificate_file(self.certificateFileName)
        if self.certificateChainFileName is not None:
            ctx.use_certificate_chain_file(self.certificateChainFileName)
        ctx.use_privatekey_file(self.privateKeyFileName)
        self._context = ctx



def getWebSocketService(host, port):
    """
    Gets a WebSocket service listening on the specified port
    :param host: The hostname the server is available under
    :type host: str
    :param port: Port number
    :type port: int
    """

    txaio.use_twisted()

    factory = WebSocketServerFactory("ws://{1}:{0}".format(port, host))
    factory.protocol = WebSocketShellProtocol

    return internet.TCPServer(port, factory)

def getWebSocketSecureService(host, port, key, certificate, chain):
    """
    Gets a SSL WebSocket service listening on the specified port
    :param host: The hostname the server is available under
    :type host: str
    :param port: Port to listen on
    :type port: int
    :param key: Server private key filename
    :type key: str
    :param certificate: Server certificate filename
    :type certificate: str
    :param chain: Certificate chain filename (PEM encoded)
    :type chain: str
    """

    # To make self-signed keys:
    # openssl genrsa -out server.key 2048
    # openssl req -new -key server.key -out server.csr
    # openssl x509 -req -days 3650 -in server.csr -signkey server.key -out server.crt
    # openssl x509 -in server.crt -out server.pem

    contextFactory = ChainedOpenSSLContextFactory(key, certificate, chain)
    factory = WebSocketServerFactory("wss://{1}:{0}".format(port, host))
    factory.protocol = WebSocketShellProtocol

    return internet.SSLServer(port, factory, contextFactory)


class WebSocketShellProtocol(WebSocketServerProtocol, BaseShell):
    """
    A super basic shell specially for WebSockets due to its incompatible
    protocol design.
    """

    def __init__(self):
        # noinspection PyBroadException
        try:
            # Later versions of autobahn require this. Earlier versions don't
            # support it at all.
            WebSocketServerProtocol.__init__(self)
        except:
            pass

        BaseShell.__init__(self, None, "websocket")
        self._buffer = ""

    def connectionLost(self, reason):
        super(WebSocketShellProtocol, self).connectionLost(reason)

        self.terminateProcess()
        end_session(self.sid)

    def onMessage(self, msg, binary):

        if binary:
            return # We don't do binary.

        for char in msg:
            if char == '\x03':

                # ^C was sent. Terminate the process.
                self.terminateProcess()
            else:
                self._buffer += char

            if len(self._buffer) > 0 and self._buffer[-1:] == "\n":
                self.processLine(self._buffer)
                self._buffer = ""

        if len(self._buffer) > 0:
            self.processLine(self._buffer)
            self._buffer = ""

    def commandProcessorWarning(self, warning):
        """
        Called when the command processor gets annoyed.
        :param warning: Warning text
        :type warning: str
        """
        self.sendMessage(warning + "\n", False)

    def logout(self):
        """
        Called when a disconnect has been requested.
        """
        self.sendClose()

    def processOutput(self, value):
        """
        Called when the command wants to send output.
        :param value: Output data
        """
        self.sendMessage(value, False)

    def showPrompt(self):
        """
        Outputs the prompt string.
        :return:
        """
        self.sendMessage(self.prompt[self.prompt_number])

    def sendServerStatus(self, redirectUrl = None, redirectAfter = 0):
        """
        Used to send out server status/version upon receiving a HTTP/GET
        without upgrade to WebSocket header (and option serverStatus is True).
        """
        html = """
    <!DOCTYPE html>
    <html>
       <head>
          <title>zxweather server WebSocket endpoint</title>
       </head>
       <body>
          <h1>zxweather WebSocket Endpoint</h1>
          <hr>
          <p>
             This is not a web server. You have reached the WebSocket endpoint
             on the zxweather server. Its purpose is to supply instant data
             update notifications and other station data to the web interface.
          </p>
          <p>If you actually were expecting to do WebSockety stuff here then
             your WebSocket client failed to send the appropriate upgrade
             header (or something else stripped it off).
          </p>
          <p>See Also: <a href="http://tools.ietf.org/html/rfc6455">RFC-6455 -
          The WebSocket Protocol</a>
          </p>
          <hr>
          <div style="text-align: right">zxweather server</div>
       </body>
    </html>
    """
        self.sendHtml(html)
