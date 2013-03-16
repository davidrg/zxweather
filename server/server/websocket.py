# coding=utf-8
"""
WebSocket support for zxweatherd.
"""
from autobahn.websocket import WebSocketServerProtocol, WebSocketServerFactory
from twisted.application import internet
from twisted.internet import ssl
from server.session import end_session
from server.shell import BaseShell

__author__ = 'david'


def getWebSocketService(port):
    """
    Gets a WebSocket service listening on the specified port
    :param port: Port number
    :type port: int
    """
    factory = WebSocketServerFactory("ws://localhost:{0}".format(port))
    factory.protocol = WebSocketShellProtocol

    return internet.TCPServer(port, factory)

def getWebSocketSecureService(port, key, certificate):
    """
    Gets a SSL WebSocket service listening on the specified port
    :param port: Port to listen on
    :type port: int
    :param key: Server private key filename
    :type key: str
    :param certificate: Server certificate filename
    :type certificate: str
    """

    # To make self-signed keys:
    # openssl genrsa -out server.key 2048
    # openssl req -new -key server.key -out server.csr
    # openssl x509 -req -days 3650 -in server.csr -signkey server.key -out server.crt
    # openssl x509 -in server.crt -out server.pem

    contextFactory = ssl.DefaultOpenSSLContextFactory(key, certificate)
    factory = WebSocketServerFactory("ws://localhost:{0}".format(port))
    factory.protocol = WebSocketShellProtocol

    return internet.SSLServer(port, factory, contextFactory)


class WebSocketShellProtocol(WebSocketServerProtocol, BaseShell):
    """
    A super basic shell specially for WebSockets due to its incompatible
    protocol design.
    """

    def __init__(self):
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
