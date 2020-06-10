# coding=utf-8
"""
Client implementation of Weather Push UDP protocol
"""
from twisted.internet import reactor, defer
from twisted.internet.protocol import DatagramProtocol
from twisted.python import log

from zxw_push.client.weather_push_server.base_client import WeatherPushClientBase
from zxw_push.common.util import Sequencer
from zxw_push.common.packets import StationInfoRequestUDPPacket, \
    decode_packet, StationInfoResponseUDPPacket, \
    WeatherDataUDPPacket, SampleAcknowledgementUDPPacket

__author__ = 'david'

# No control over it being an old-style class
# noinspection PyClassicStyleClass
class WeatherPushDatagramClient(DatagramProtocol, WeatherPushClientBase):
    """
    Handles sending weather data to a remote WeatherPush server via UDP
    """
    _STATION_LIST_TIMEOUT = 60  # seconds to wait for a response

    def __init__(self, ip_address, port, authorisation_code,
                 confirmed_sample_func, tcp_client_factory):

        super(WeatherPushDatagramClient, self).__init__(authorisation_code, confirmed_sample_func)

        # UDP max is really 65507 but we want to avoid fragmenting the packet
        # as it will reduce the chance of it arriving at its destination intact.
        # 512 bytes should be a safe number but we'll leave a bit of room for
        # a live record.
        self._max_sample_payload = 450

        # Server we're communicating with
        self._ip_address = ip_address
        self._port = port

        # The UDP client doesn't support sending images. For that we'll fire up
        # an instance of the TCP client and use that.
        self._tcp_client_factory = tcp_client_factory
        self._tcp_client = None

        # 16bit integer sequence for packet IDs
        self._sequence_id = Sequencer()

        self._station_info_request_retries = 0

        # This variable tracks how many of the last 256 live data records
        # went missing because either:
        # - The packet containing them went missing
        #    - The packet containing them arrived in the wrong order
        #    - The live data was diff-compressed against a live record that
        #      went missing for one of these three reasons
        # If this value is 128 that means that fully 50% of the live records
        # we sent either never reached the server or could not be used or
        # decoded when they did arrive. If this value gets high we probably
        # want to abandon diff-compression as it will only increase the error
        # rate (due to reason #3 above)
        # This variable is updated via sample acknowledgement packets.
        self._lost_live_updates = 0

    def startProtocol(self):
        """
        Sets up the transport and fires off an initial station list request
        """
        # Connecting the transport like this causes reliability problems. Every
        # so often we just start getting exceptions every time we try to write
        # to the transport - possibly caused by the network interface (ppp0)
        # disappearing briefly .
        # self.transport.connect(self._ip_address, self._port)

        log.msg("Requesting station list from server...")
        self._request_station_list()

    def stopProtocol(self):
        # Shut down the TCP client if its running.
        if self._tcp_client is not None:
            self._tcp_client.stopService()

    def datagramReceived(self, datagram, address):
        """
        Called whenever a packet arrives.
        :param datagram: Packet data
        :param address: Address and port from source application
        """
        packet = decode_packet(datagram)

        if isinstance(packet, StationInfoResponseUDPPacket):
            self._handle_station_list(packet)
        elif isinstance(packet, SampleAcknowledgementUDPPacket):
            self._handle_sample_acknowledgements(packet)

    def _send_packet(self, packet):
        """

        :param packet: Packet to send
        :type packet: Packet
        :return:
        """

        self.transport.write(self._encode_packet_for_sending(packet),
                             (self._ip_address, self._port))

    def _request_station_list(self):
        packet = StationInfoRequestUDPPacket(self._sequence_id(),
                                             self._authorisation_code)

        self._send_packet(packet)

        reactor.callLater(self._STATION_LIST_TIMEOUT,
                          self._request_station_list_timeout)

    def _request_station_list_timeout(self):
        if self._stations is None:
            # No station info response packet received. Try asking again
            self._station_info_request_retries += 1
            log.msg("No response from server for station list request after "
                    "{1} seconds. Resending request. This is retry {0}.".format(
                        self._station_info_request_retries,
                        self._STATION_LIST_TIMEOUT))
            self._request_station_list()

    def _handle_sample_acknowledgements(self, packet):
        super(WeatherPushDatagramClient, self)._handle_sample_acknowledgements(packet)

        self._lost_live_updates = packet.lost_live_records

    def send_image(self, image):
        # The UDP transport doesn't support sending images. So we rely on a
        # private instance of the TCP client to do that. This client instance
        # will only handle images - the rest of the weather data will go over
        # UDP.

        if self._tcp_client is None:
            log.msg("Starting TCP client to transmit image.")
            self._tcp_client = self._tcp_client_factory()

        self._tcp_client.send_image(image)

    @defer.inlineCallbacks
    def _flush_data_buffer(self, station_id, live_data, hardware_type):
        """
        Writes the supplied live data and any waiting samples for the station id
        to the network.

        :param station_id: Station ID the live data is for
        :type station_id: int
        :param live_data: Live data
        :type live_data: dict
        :param hardware_type: Hardware type used by the live data
        :type hardware_type: str
        """

        packet = WeatherDataUDPPacket(self._sequence_id(), self._authorisation_code)

        transmit_packet, live_sequence_id = yield self._prepare_data_for_transmission(
            packet, station_id, live_data, hardware_type)

        if not transmit_packet:
            # Nothing to send. Roll-back the packet ID so it doesn't jump
            # forward when we do actually send something.
            self._sequence_id.rollback()
            return

        self._send_weather_data_packet(packet, station_id, live_sequence_id)
