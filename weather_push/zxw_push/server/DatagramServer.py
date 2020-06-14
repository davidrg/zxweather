# coding=utf-8
"""
UDP implementation of the WeatherPush server
"""
from datetime import datetime, timedelta

from twisted.internet import defer
from twisted.internet.protocol import DatagramProtocol
from twisted.python import log
from zxw_push.common.packets import decode_packet, \
    StationInfoRequestUDPPacket, StationInfoResponseUDPPacket, \
    WeatherDataUDPPacket, SampleAcknowledgementUDPPacket
from zxw_push.common.util import Sequencer
from zxw_push.server.base_server import WeatherPushServerBase
from zxw_push.server.database import ServerDatabase

__author__ = 'david'

# TODO: check UDP client reconnect behaviour as we currently stop tracking clients
#  after 24h of no data.

class WeatherPushDatagramClientInstance(WeatherPushServerBase):
    """
    Handles communications with a specific client instance identified by its
    address
    """

    def __init__(self, database, address, transmit_function, authorisation_code):
        super(WeatherPushDatagramClientInstance, self).__init__(authorisation_code)

        self._protocol = self._PROTO_UDP

        self._db = database
        self._address = address  # Remote client address
        self._sequence_id = Sequencer()
        self._send_packet = transmit_function

    @defer.inlineCallbacks
    def start(self):
        yield self._get_stations(None)

    def packet_received(self, packet):
        if isinstance(packet, StationInfoRequestUDPPacket):
            self._send_station_info(packet.authorisation_code)
        elif isinstance(packet, WeatherDataUDPPacket):
            self._handle_weather_data(packet)

    @defer.inlineCallbacks
    def _send_station_info(self, authorisation_code):
        log.msg("Sending station info...")

        packet = StationInfoResponseUDPPacket(
            self._sequence_id(), authorisation_code)

        station_set = yield self._get_stations(authorisation_code)

        for station in station_set:
            packet.add_station(station[0], station[1], station[2])

        # Client has probably just reconnected so we'll clear some state so
        # that the client can start sequence IDs from 0, etc.
        self._reset_tracking_variables()

        self._encode_and_send_packet(packet)

    @defer.inlineCallbacks
    def _handle_weather_data(self, packet):

        if not self._ready:
            return  # We're not ready to process data yet. Ignore it.

        ack_packet = SampleAcknowledgementUDPPacket()

        sample_records = yield self._process_weather_records(packet, ack_packet)

        # Make sure the count is valid
        if self._lost_live_records > 255:
            self._lost_live_records = 255

        # Only send the acknowledgement packet if we've got samples to
        # acknowledge
        if sample_records:
            ack_packet.lost_live_records = self._lost_live_records
            self._encode_and_send_packet(ack_packet)

        # Done!

    def _encode_and_send_packet(self, packet):
        encoded = self._encode_packet_for_sending(packet)

        self._send_packet(encoded, self._address)


# Can't fix old-style-class-ness as its a twisted thing.
# noinspection PyClassicStyleClass
class WeatherPushDatagramServer(DatagramProtocol):
    """
    Implements the server-side of the WeatherPush protocol.
    """

    def __init__(self, dsn, authorisation_code):
        self._authorisation_code = authorisation_code

        self._protocol = WeatherPushServerBase._PROTO_UDP

        self._dsn = dsn
        self._db = None

        self._clients = dict()
        self._last_contact = dict()

    def startProtocol(self):
        """
        Called when the protocol has started. Connects to the database, etc.
        """
        log.msg("Datagram Server started")
        self._db = ServerDatabase(self._dsn)

        self._ready = True

    def datagramReceived(self, datagram, address):
        """
        Processes a received datagram
        :param datagram: Received datagram data
        :param address: Sending address
        """

        packet = decode_packet(datagram)

        if packet is None:
            # invalid packet.
            return

        if packet.authorisation_code != self._authorisation_code:
            log.msg("Ignoring packet from unauthorised client with "
                    "auth code {0}".format(packet.authorisation_code))
            return  # Unauthorised client. Ignore it.

        if address not in self._clients:
            server_instance = WeatherPushDatagramClientInstance(
                self._db, address, self._send_packet, self._authorisation_code)
            server_instance.start()
            self._clients[address] = server_instance

        self._clients[address].packet_received(packet)
        self._last_contact[address] = datetime.now()

        # Clear out any dead connections.
        for client_address in self._last_contact.keys():
            if self._last_contact[client_address] < datetime.now() - timedelta(days=1):
                # Client hasn't been seen in over 24h. Assume its gone.
                self._clients.pop(client_address, None)
                self._last_contact.pop(client_address, None)

    def _send_packet(self, packet, address):
        self.transport.write(packet, address)

