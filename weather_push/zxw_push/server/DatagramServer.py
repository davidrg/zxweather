from twisted.internet.protocol import DatagramProtocol

__author__ = 'david'

class WeatherPushDatagramServer(DatagramProtocol):

    def startProtocol(self):
        # TODO: Connect to database
        pass


    def datagramReceived(self, datagram, address):
        print("Received {0} from {1}".format(datagram, address))