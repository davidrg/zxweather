from twisted.internet import defer
from twisted.internet.protocol import DatagramProtocol
from twisted.python import log
from zxw_push.common.packets import decode_packet, StationInfoRequestPacket, \
    StationInfoResponsePacket
from zxw_push.common.util import Sequencer
from zxw_push.server.database import ServerDatabase

__author__ = 'david'


class WeatherPushDatagramServer(DatagramProtocol):

    def __init__(self, dsn):
        self._dsn = dsn
        self._sequence_id = Sequencer()
        self._db = None
        self._next_station_id = 0
        self._station_code_id = {}
        self._station_id_code = {}

    def startProtocol(self):
        log.msg("Datagram Server started")
        self._db = ServerDatabase(self._dsn)

    def datagramReceived(self, datagram, address):
        log.msg("Received {0} from {1}".format(datagram, address))

        packet = decode_packet(datagram)

        if isinstance(packet, StationInfoRequestPacket):
            self._send_station_info(address, packet.authorisation_code)

    def _send_packet(self, packet, address):
        encoded = packet.encode()

        payload_size = len(encoded)
        udp_header_size = 8
        ip4_header_size = 20

        log.msg("Sending {0} packet. Payload {1} bytes. UDP size {2} bytes. "
                "IPv4 size {3} bytes.".format(
            packet.__class__.__name__,
            payload_size,
            payload_size + udp_header_size,
            payload_size + udp_header_size + ip4_header_size
        ))

        self.transport.write(encoded, address)

    @defer.inlineCallbacks
    def _send_station_info(self, address, authorisation_code):
        log.msg("Sending station info...")

        station_info = yield self._db.get_station_info(authorisation_code)

        packet = StationInfoResponsePacket(
            self._sequence_id(),
            authorisation_code)

        for station in station_info:
            station_code = station[0]
            if station_code not in self._station_code_id.keys():

                if self._next_station_id >= 255:
                    log.msg("*** WARNING: out of station IDs")
                    continue

                self._station_code_id[station_code] = self._next_station_id
                self._station_id_code[self._next_station_id] = station_code
                self._next_station_id += 1

            packet.add_station(station_code,
                               station[1],  # Hardware type code
                               self._station_code_id[station_code])

        self._send_packet(packet, address)