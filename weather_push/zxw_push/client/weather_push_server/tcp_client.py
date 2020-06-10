# coding=utf-8
"""
Client implementation of Weather Push TCP protocol. It works largely the same
way as the UDP implementation but with different packet types.
"""
from twisted.internet import reactor, defer
from twisted.internet import protocol
from twisted.internet.protocol import connectionDone
from twisted.python import log

from zxw_push.client.weather_push_server.base_client import WeatherPushClientBase
from zxw_push.common.util import Event
from zxw_push.common.packets import decode_packet, \
    AuthenticateTCPPacket, WeatherDataTCPPacket, \
    StationInfoTCPPacket, SampleAcknowledgementTCPPacket, \
    AuthenticateFailedTCPPacket, get_packet_size, \
    get_data_required_for_size_calculation, ImageTCPPacket, \
    ImageAcknowledgementTCPPacket

__author__ = 'david'

# No control over it being an old-style class
# noinspection PyClassicStyleClass
class WeatherPushProtocol(protocol.Protocol, WeatherPushClientBase):
    """
    Handles sending weather data to a remote WeatherPush server via TCP
    """
    _STATION_LIST_TIMEOUT = 60  # seconds to wait for a response

    def __init__(self, authorisation_code,
                 confirmed_sample_func, new_image_size, resize_sources):

        super(WeatherPushProtocol, self).__init__(authorisation_code, confirmed_sample_func)

        # Max is really 65535 but we'll leave some space for the live record and
        # packet headers, etc.
        self._max_sample_payload = 60000

        # We can optionally resize images before transmission to save data costs.
        # These control that image resizing.
        self._resize_images = True
        self._new_image_size = new_image_size
        self._resize_sources = resize_sources

        if new_image_size is None:
            self._resize_images = False

        # Fired when we loose our connection to the server.
        self.ConnectionLost = Event()

        # Image source details
        self._image_type_ids = None
        self._image_type_codes = None
        self._image_source_ids = None
        self._image_source_codes = None

        # Number of failed attempts to authenticate with the server
        self._authentication_retries = 0

        # Data receive buffer
        self._receive_buffer = ''

        # If we've failed to authenticate with the server.
        self._authentication_failed = False

    def connectionMade(self):
        """
        Fires off an initial station list request
        """

        log.msg("Authenticating...")
        self._authenticate()

    def connectionLost(self, reason=connectionDone):
        # We need to break all references to the outside world here so we don't
        # end up being kept alive accidentally.

        self._confirmed_sample_func = None
        self.Ready.clearHandlers()
        self.ReceiptConfirmation.clearHandlers()
        self.ImageReceiptConfirmation.clearHandlers()

        self.ConnectionLost.fire()
        self.ConnectionLost.clearHandlers()

    def dataReceived(self, data):
        """
        Called whenever a packet arrives.
        :param data: Packet data
        """
        self._receive_buffer += data

        while len(self._receive_buffer) > 0:
            if len(self._receive_buffer) < 2 or len(self._receive_buffer) < \
                    get_data_required_for_size_calculation(
                        self._receive_buffer):
                # Insufficient data in the buffer to calculate the packet length
                return

            packet_size = get_packet_size(self._receive_buffer)

            if len(self._receive_buffer) >= packet_size:
                packet_data = self._receive_buffer[:packet_size]
                self._receive_buffer = self._receive_buffer[packet_size:]

                packet = decode_packet(packet_data)

                if isinstance(packet, StationInfoTCPPacket):
                    self._handle_station_list(packet)
                elif isinstance(packet, SampleAcknowledgementTCPPacket):
                    self._handle_sample_acknowledgements(packet)
                elif isinstance(packet, AuthenticateFailedTCPPacket):
                    self._handle_failed_authentication()
                elif isinstance(packet, ImageAcknowledgementTCPPacket):
                    self._handle_image_acknowledgement(packet)
            else:
                # Insufficient data in the buffer to decode the packet
                return

    def _send_packet(self, packet):
        """

        :param packet: Packet to send
        :type packet: Packet
        :return:
        """

        self.transport.write(self._encode_packet_for_sending(packet))

    def _authenticate(self):
        packet = AuthenticateTCPPacket(self._authorisation_code)

        self._send_packet(packet)

        reactor.callLater(self._STATION_LIST_TIMEOUT,
                          self._authenticate_timeout)

    def _authenticate_timeout(self):
        if self._stations is None and not self._authentication_failed:
            # No station info response packet received. Try asking again
            self._authentication_retries += 1
            log.msg("No response from server for authentication request after "
                    "{1} seconds. Resending request. This is retry {0}.".format(
                        self._authentication_retries,
                        self._STATION_LIST_TIMEOUT))
            self._authenticate()

    def _handle_failed_authentication(self):
        log.msg("Authentication failed.")
        self._authentication_failed = True
        self.transport.loseConnection()

    def _handle_station_list(self, station_list_packet):
        """
        Handles any received station list packets.
        :param station_list_packet: Packet containing a list of stations we can
                                    submit data for
        :type station_list_packet: StationInfoTCPPacket
        """

        self._image_type_ids = dict()
        self._image_type_codes = dict()
        self._image_source_ids = dict()
        self._image_source_codes = dict()

        for image_type in station_list_packet.image_types:
            type_code = image_type[0]
            type_id = image_type[1]
            self._image_type_ids[type_code] = type_id
            self._image_type_codes[type_id] = type_code
            log.msg("\t- image type {0} id {1}".format(type_code, type_id))

        for image_source in station_list_packet.image_sources:
            source_code = image_source[0]
            source_id = image_source[1]
            self._image_source_ids[source_code] = source_id
            self._image_source_codes[source_id] = source_code
            log.msg("\t- image source {0} id {1}".format(source_code,
                                                         source_id))

        super(WeatherPushProtocol, self)._handle_station_list(station_list_packet)

    def _handle_image_acknowledgement(self, packet):
        for image in packet.images:
            source_id = image[0]
            type_id = image[1]
            timestamp = image[2]

            source_code = self._image_source_codes[source_id]
            type_code = self._image_type_codes[type_id]

            resized = self._resize_images
            if source_code.upper() not in self._resize_sources:
                resized = False

            self.ImageReceiptConfirmation.fire(source_code, type_code,
                                               timestamp, resized)

    def send_image(self, image):
        type_code = image['image_type_code']
        source_code = image['image_source_code']
        time_stamp = image['time_stamp']
        title = image['title']
        description = image['description']
        mime_type = image['mime_type']
        metadata = image['metadata']
        data = image['image_data']

        image_type_id = self._image_type_ids[type_code]
        image_source_id = self._image_source_ids[source_code]

        # Only resize the image if:
        #   -> Image resizing is returned on
        #   -> The image source is in the list of image sources to resize for
        #   -> The image is actually an image (rather than a video, etc)
        if self._resize_images and source_code.upper() in self._resize_sources \
                and mime_type.startswith("image/"):
            log.msg("Resizing image...")
            from io import BytesIO
            from PIL import Image
            import mimetypes

            original = BytesIO(bytes(data))

            img = Image.open(original)

            img.thumbnail(self._new_image_size, Image.ANTIALIAS)  # AKA LANCZOS

            ext = mimetypes.guess_extension(mime_type)
            if ext == ".jpe":
                ext = ".jpeg"
            ext = ext[1:]

            out = BytesIO()
            img.save(out, format=ext)
            data = out.getvalue()
            out.close()
            original.close()

        packet = ImageTCPPacket(image_type_id, image_source_id, time_stamp,
                                title, description, mime_type, metadata,
                                str(data))

        self._send_packet(packet)

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

        packet = WeatherDataTCPPacket()

        transmit_packet, live_sequence_id = yield self._prepare_data_for_transmission(
            packet, station_id, live_data, hardware_type)

        if not transmit_packet:
            # Nothing to send.
            return

        self._send_weather_data_packet(packet, station_id, live_sequence_id)
