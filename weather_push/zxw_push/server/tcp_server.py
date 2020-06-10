# coding=utf-8
"""
TCP implementation of the WeatherPush server. Much the same as the UDP version
but slightly different packets.
"""
from twisted.internet import defer, reactor, protocol
from twisted.python import log
from zxw_push.common.packets import decode_packet, \
    get_data_required_for_size_calculation, get_packet_size, \
    AuthenticateTCPPacket, WeatherDataTCPPacket, StationInfoTCPPacket, \
    SampleAcknowledgementTCPPacket, AuthenticateFailedTCPPacket, ImageTCPPacket, \
    ImageAcknowledgementTCPPacket
from zxw_push.common.util import Sequencer
from zxw_push.server.base_server import WeatherPushServerBase

__author__ = 'david'

# Can't fix old-style-class-ness as its a twisted thing.
# noinspection PyClassicStyleClass
class WeatherPushTcpServer(protocol.Protocol, WeatherPushServerBase):
    """
    Implements the server-side of the WeatherPush TCP protocol.
    """

    def __init__(self, authorisation_code):
        super(WeatherPushTcpServer, self).__init__(authorisation_code)
        self._protocol = self._PROTO_TCP

        self._image_type_id_seq = Sequencer()
        self._image_type_code_id = dict()
        self._image_type_id_code = dict()

        self._image_source_id_seq = Sequencer()
        self._image_source_code_id = dict()
        self._image_source_id_code = dict()

        self._receive_buffer = ''

        self._authenticated = False

    def start_protocol(self, db):
        """
        Called when the protocol has started. Connects to the database, etc.
        """

        self._db = db

        log.msg("TCP Server started")

        self._get_stations(None)

        self._ready = True

    def dataReceived(self, data):
        """
        Processes a received datagram
        :param data: Received data
        """

        self._receive_buffer += data

        if not self._ready:
            reactor.callLater(1, self.dataReceived, "")

        while len(self._receive_buffer) > 0:

            if len(self._receive_buffer) < 2 or len(self._receive_buffer) < \
                    get_data_required_for_size_calculation(
                        self._receive_buffer):
                # insufficient data to determine size of packet
                return

            packet_size = get_packet_size(self._receive_buffer)

            if len(self._receive_buffer) >= packet_size:
                packet_data = self._receive_buffer[:packet_size]
                self._receive_buffer = self._receive_buffer[packet_size:]

                packet = decode_packet(packet_data)

                if isinstance(packet, AuthenticateTCPPacket):
                    self._send_station_info(packet.authorisation_code)
                elif self._authenticated:
                    # These packets require authentication before they will be
                    # processed. Until the client has authenticated we'll just
                    # ignore them.
                    if isinstance(packet, WeatherDataTCPPacket):
                        self._handle_weather_data(packet)
                    elif isinstance(packet, ImageTCPPacket):
                        self._handle_image_data(packet)
                    else:
                        log.msg("Unsupported packet type {0}".format(
                            packet.packet_type))
                else:
                    log.msg("Ignoring packet of type {0} from unauthenticated "
                            "client.".format(type(packet)))
            else:
                # Insufficient data to decode packet
                return

    def _send_packet(self, packet):
        encoded = self._encode_packet_for_sending(packet)

        self.transport.write(encoded)

    @defer.inlineCallbacks
    def _get_image_types(self):
        image_types = yield self._db.get_image_type_codes()

        result = []

        for type_row in image_types:
            type_code = type_row[0]

            if type_code in self._image_type_code_id.keys():
                type_id = self._image_type_code_id[type_code]
            else:
                type_id = self._image_type_id_seq()
                self._image_type_code_id[type_code] = type_id
                self._image_type_id_code[type_id] = type_code

            result.append((type_code, type_id))

        defer.returnValue(result)

    @defer.inlineCallbacks
    def _get_image_sources(self, stations):
        """
        Gets all image sources for a list of stations.

        :param stations: A list of station codes to get image sources for
        """
        image_sources = yield self._db.get_image_sources()

        result = []

        for source in image_sources:
            source_code = source[0]
            station_code = source[1]

            if station_code in stations:
                if source_code in self._image_source_code_id.keys():
                    source_id = self._image_source_code_id[source_code]
                else:
                    source_id = self._image_source_id_seq()
                    self._image_source_code_id[source_code] = source_id
                    self._image_source_id_code[source_id] = source_code

                result.append((source_code, source_id))

        defer.returnValue(result)

    @defer.inlineCallbacks
    def _send_station_info(self, authorisation_code):

        if authorisation_code != self._authorisation_code:
            log.msg("Ignoring packet from unauthorised client with "
                    "auth code {0}".format(authorisation_code))
            packet = AuthenticateFailedTCPPacket()
            self._send_packet(packet)
            self._authenticated = False
            return

        self._authenticated = True

        log.msg("Sending station info...")

        packet = StationInfoTCPPacket()

        station_set = yield self._get_stations(authorisation_code)

        # image support was added in schema level 3 (zxweather 1.0.0)
        image_type_set = []
        image_source_set = []
        if self._db.schema_version >= 3:
            image_type_set = yield self._get_image_types()
            image_source_set = yield self._get_image_sources(
                [x[0] for x in station_set])  # x[0] is the station code

        for station in station_set:
            #                  code        hw_type     station_id
            packet.add_station(station[0], station[1], station[2])

        for image_type in image_type_set:
            #                     code           id
            packet.add_image_type(image_type[0], image_type[1])

        for image_source in image_source_set:
            #                       code             id
            packet.add_image_source(image_source[0], image_source[1])

        # Client has probably just reconnected so we'll clear some state so
        # that the client can start sequence IDs from 0, etc.
        self._reset_tracking_variables()

        self._send_packet(packet)

    @defer.inlineCallbacks
    def _handle_image_data(self, packet):

        if self._db.schema_version < 3:
            log.msg("*** ERROR: Received image when no image sources were "
                    "advertised due to incompatible database schema level. "
                    "Image will not be stored or acknowledged.")
            return

        source_code = self._image_source_id_code[packet.image_source_id]
        type_code = self._image_type_id_code[packet.image_type_id]

        result = yield self._db.store_image(source_code, type_code, packet.timestamp,
                                           packet.title, packet.description,
                                           packet.mime_type, packet.metadata,
                                           packet.image_data)
        if not result:
            self._statistics_collector.log_duplicate_image_receipt()

        ack = ImageAcknowledgementTCPPacket()
        ack.add_image(packet.image_source_id, packet.image_type_id,
                      packet.timestamp)

        self._send_packet(ack)

    @defer.inlineCallbacks
    def _handle_weather_data(self, packet):

        if not self._ready:
            return  # We're not ready to process data yet. Ignore it.

        ack_packet = SampleAcknowledgementTCPPacket()

        sample_records = yield self._process_weather_records(packet, ack_packet)

        # Only send the acknowledgement packet if we've got samples to
        # acknowledge
        if sample_records:
            self._send_packet(ack_packet)

        # Done!
