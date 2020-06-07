# coding=utf-8
"""
Client implementation of Weather Push TCP protocol. It works largely the same
way as the UDP implementation but with different packet types.
"""
import json
from datetime import datetime

from twisted.internet import reactor, defer
from twisted.internet import protocol
from twisted.internet.protocol import connectionDone
from twisted.python import log

from zxw_push.common.data_codecs import encode_live_record, \
    encode_sample_record
from zxw_push.common.statistics_collector import MultiPeriodStatisticsCollector
from zxw_push.common.util import Event, Sequencer
from zxw_push.common.packets import decode_packet, LiveDataRecord, \
    SampleDataRecord, AuthenticateTCPPacket, WeatherDataTCPPacket, \
    StationInfoTCPPacket, SampleAcknowledgementTCPPacket, \
    AuthenticateFailedTCPPacket, get_packet_size, \
    get_data_required_for_size_calculation, ImageTCPPacket, \
    ImageAcknowledgementTCPPacket

__author__ = 'david'

# Set this to the name of the csv file to log packet statistics to. Set it to
# None to disable statistics logging.
_PACKET_STATISTICS_FILE = None

def toHexString(string):
    """
    Converts the supplied string to hex.
    :param string: Input string
    :return:
    """
    result = ""
    for char in string:

        hex_encoded = hex(ord(char))[2:]
        if len(hex_encoded) == 1:
            hex_encoded = '0' + hex_encoded

        result += r'\x{0}'.format(hex_encoded)
    return result

# No control over it being an old-style class
# noinspection PyClassicStyleClass
class WeatherPushProtocol(protocol.Protocol):
    """
    Handles sending weather data to a remote WeatherPush server via TCP
    """
    _STATION_LIST_TIMEOUT = 60  # seconds to wait for a response

    def __init__(self, authorisation_code,
                 confirmed_sample_func, new_image_size, resize_sources):

        self._authorisation_code = authorisation_code
        self._confirmed_sample_func = confirmed_sample_func

        self._resize_images = True
        self._new_image_size = new_image_size
        self._resize_sources = resize_sources

        if new_image_size is None:
            self._resize_images = False

        self.Ready = Event()
        self.ReceiptConfirmation = Event()
        self.ImageReceiptConfirmation = Event()
        self.ConnectionLost = Event()

        # These are all keyed by station id
        self._live_sequence_id = {}  # Sequencers for each station
        self._outgoing_samples = {}  # List of waiting samples for each station
        self._previous_live_record = {}  # Last live record for each station

        # This is keyed by station and live sequence id
        self._outgoing_weather_packets = dict()

        self._stations = None
        self._station_ids = None
        self._image_type_ids = None
        self._image_type_codes = None
        self._image_source_ids = None
        self._image_source_codes = None
        self._station_codes = None

        self._authentication_retries = 0

        self._ready_event_fired = False

        self._receive_buffer = ''

        # How many compressed live records we should send before forcing a full
        # (uncompressed) record
        self._max_compressed_live_records = 30

        self._compressed_live_records_remaining = \
            self._max_compressed_live_records

        self._authentication_failed = False

        self._statistics_collector = MultiPeriodStatisticsCollector(datetime.now, log.msg)

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
        encoded = packet.encode()

        if isinstance(encoded, bytearray):
            encoded = bytes(encoded)

        self._statistics_collector.log_packet_transmission(packet.__class__.__name__, len(encoded))

        self.transport.write(encoded)

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
        self._stations = dict()
        self._station_ids = dict()
        self._station_codes = dict()
        self._image_type_ids = dict()
        self._image_type_codes = dict()
        self._image_source_ids = dict()
        self._image_source_codes = dict()

        station_codes = []

        log.msg("Received station list:- ")
        for station in station_list_packet.stations:
            station_code = station.station_code
            station_id = station.station_id

            # stationCode: hardwareType
            self._stations[station_code] = station.hardware_type
            self._station_ids[station_code] = station_id
            self._station_codes[station_id] = station_code
            station_codes.append(station_code)

            # Have to strip as its a fixed-width field padded with spaces

            log.msg("\t- station {0} hardware {1} station id {2}".format(
                station_code, station.hardware_type, station_id))

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

        if not self._ready_event_fired:
            self.Ready.fire(station_codes)
            self._ready_event_fired = True

        log.msg("Client Ready.")

    def _handle_sample_acknowledgements(self, packet):
        if self._stations is None:
            return

        for acknowledgement in packet.sample_acknowledgements():
            station_id = acknowledgement[0]
            time_stamp = acknowledgement[1]

            station_code = self._station_codes[station_id]

            self.ReceiptConfirmation.fire(station_code, time_stamp)

    def send_live(self, live, hardware_type):
        """
        Sends a live data record. This will also cause any waiting samples to
        be sent in the same packet.

        :param live: Live data to send
        :type live: dict
        :param hardware_type: Type of hardware that generated the record
        :type hardware_type: str
        """
        if self._stations is None:
            return

        live = self._to_real_dict(live)

        station_code = live['station_code']

        if station_code not in self._stations.keys():
            return  # server isn't accepting data from us for this station

        station_id = self._station_ids[station_code]

        if station_id not in self._live_sequence_id.keys():
            # I guess we've not seen this station before
            self._live_sequence_id[station_id] = Sequencer()
            self._previous_live_record[station_id] = None

        self._flush_data_buffer(station_id, live, hardware_type)

    @staticmethod
    def _to_real_dict(value):
        result = {}

        for key in value.keys():
            result[key] = value[key]

        return result

    def send_sample(self, sample, hardware_type, hold=False):
        """
        Queues a sample for transmission to the remote server. The sample will
        be sent when when the next live data record is ready to be sent or when
        the number of waiting samples reaches a threshold

        :param sample: Sample to send
        :param hardware_type: Type of hardware that generated the sample
        :param hold: Ignored
        """

        # To suppress warning about unused parameter
        if hold:
            pass

        sample = self._to_real_dict(sample)

        station_code = sample['station_code']

        if station_code not in self._stations.keys():
            return  # server isn't accepting data from us for this station

        station_id = self._station_ids[station_code]

        if station_id not in self._outgoing_samples.keys():
            # I guess we've not seen data from this station before.
            self._outgoing_samples[station_id] = []

        self._outgoing_samples[station_id].append((sample, hardware_type))

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

    # Cant be static as we need to stay compatible with other client classes
    # noinspection PyMethodMayBeStatic
    def flush_samples(self):
        """
        Instructs the client to flush its sample buffer to the network. This is
        primarily used for batching up a large number of samples by calling
        sendSample with the hold parameter set.

        The TCP Client always sends data in batches to improve efficiency -
        either with a live record or when the number of samples waiting gets
        large enough. As a result this function (and the hold parameter on
        sendSample) does nothing.
        """
        pass

    def _make_live_weather_record(self, data, previous_live, previous_sample,
                                  hardware_type, station_id, sequence_id):

        # We send a full uncompressed record every so often just in case a
        # packet has gone missing or arrived out of order at some point.
        # When that happens the server looses the ability to decode live records
        # until we 'reset' the compression by sending an uncompressed record.
        compress = True
        if self._compressed_live_records_remaining <= 0:
            compress = False
            self._compressed_live_records_remaining = \
                self._max_compressed_live_records

        encoded, field_ids, compression = encode_live_record(data,
                                                             previous_live,
                                                             previous_sample,
                                                             hardware_type,
                                                             compress)
        record = None

        if encoded is not None:
            record = LiveDataRecord()
            record.station_id = station_id
            record.sequence_id = sequence_id
            record.field_list = field_ids
            record.field_data = encoded

        uncompressed_size = compression[0]
        algorithm = compression[2]

        self._statistics_collector.log_live_record(len(encoded) if encoded is not None else 0,
                                                   uncompressed_size, algorithm)

        # Track how many compressed records we've sent.
        if algorithm != "none":
            self._compressed_live_records_remaining -= 1

        return record

    def _make_sample_weather_record(self, data, previous_sample, hardware_type,
                                    station_id):

        # Samples are always compressed when it makes sense (which is
        # practically always). There is no forced uncompressed record as we
        # always know exactly what the remote server has so we're never
        # compressing against data the server might not have which would prevent
        # it from being able to decompress

        encoded, field_ids, compression = encode_sample_record(
            data, previous_sample, hardware_type)

        uncompressed_size = compression[0]
        algorithm = compression[2]

        self._statistics_collector.log_sample_record(len(encoded) if encoded is not None else 0,
                                                     uncompressed_size, algorithm)

        if encoded is None:
            return None

        record = SampleDataRecord()
        record.station_id = station_id
        record.field_list = field_ids
        record.field_data = encoded
        record.timestamp = data["time_stamp"]
        record.download_timestamp = data["download_timestamp"]

        return record

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

        live_sequence_id = self._live_sequence_id[station_id]()

        if station_id not in self._outgoing_weather_packets.keys():
            self._outgoing_weather_packets[station_id] = dict()

        # Reserve a transmission slot for the packet we're about to assemble.
        self._outgoing_weather_packets[station_id][live_sequence_id] = None

        previous_sample = yield self._confirmed_sample_func(
            self._station_codes[station_id])

        weather_records = []

        # Max is really 65535 but we'll leave some space for the live record and
        # packet headers, etc.
        max_sample_payload = 60000

        data_size = 0

        # Grab any samples waiting and do those first
        if station_id in self._outgoing_samples.keys():
            while len(self._outgoing_samples[station_id]) > 0:
                sample = self._outgoing_samples[station_id].pop(0)[0]

                if previous_sample is not None:
                    sample["sample_diff_timestamp"] = \
                        previous_sample["time_stamp"]

                record = self._make_sample_weather_record(sample,
                                                          previous_sample,
                                                          hardware_type,
                                                          station_id)
                if record is None:
                    continue  # Bad record.

                weather_records.append(record)

                # While we haven't confirmed the server has received this sample
                # the next sample in the packet can be diffed from it anyway as
                # if it gets the next sample its guaranteed to get this one too
                # as they're in the same packet.
                previous_sample = sample

                # +1 for the record terminator the packet includes for each record
                data_size += record.encoded_size() + 1

                if data_size > max_sample_payload:
                    # Enough samples! We need to leave some space in this packet
                    # for the live data update
                    break

        if previous_sample is not None:
            live_data["sample_diff_timestamp"] = previous_sample["time_stamp"]

        previous_live_record = self._previous_live_record[station_id]
        previous_live = None

        if previous_live_record is not None:
            previous_live = previous_live_record[0]
            previous_live_seq = previous_live_record[1]

            live_data["live_diff_sequence"] = previous_live_seq

        live_record = self._make_live_weather_record(
            live_data, previous_live, previous_sample,
            hardware_type, station_id,
            live_sequence_id
        )

        # live_record is None when compression throws away *all* data (meaning
        # there is no change from the last live record so no differences to
        # send)
        if live_record is not None:
            weather_records.append(live_record)

            self._previous_live_record[station_id] = (live_data,
                                                      live_record.sequence_id)
        else:
            # Live record doesn't need transmitting. Clear its reservation.
            self._outgoing_weather_packets[station_id].pop(live_sequence_id, None)
            live_sequence_id = None

        if len(weather_records) == 0:
            # Nothing to send.
            return

        packet = WeatherDataTCPPacket()

        for record in weather_records:
            packet.add_record(record)

        self._send_weather_data_packet(packet, station_id, live_sequence_id)

    def _send_weather_data_packet(self, packet, station_id, live_sequence_id):
        """
        This function will ensure packets containing live weather data are sent strictly in the order they are
        prepared.

        Most of the time this should happen naturally but under certain circumstances (such is when
        there are a large number of sample records waiting to be sent) the event driven nature of packet
        assembly can result in multiple packets being assembled at one time and completed out of order.

        :param packet: Packet to transmit
        :param station_id: Station the packet is for
        :param live_sequence_id: ID of the live record in the packet
        """

        if live_sequence_id is None:
            self._send_packet(packet)  # No live record in the packet - transmission order doesn't matter.
            return

        if station_id not in self._outgoing_weather_packets:
            raise Exception("No outgoing packets reserved for station {0}".format(station_id))

        if live_sequence_id not in self._outgoing_weather_packets[station_id]:
            raise Exception("Packet {0} for station {1} has no reserved transmission slot".format(
                live_sequence_id, station_id))

        if self._outgoing_weather_packets[station_id][live_sequence_id] is not None:
            raise Exception("Packet {0} for station {1} is already awaiting transmission!")

        log.msg("Queue data for station {0} packet {1}".format(station_id, live_sequence_id))

        self._outgoing_weather_packets[station_id][live_sequence_id] = packet

        waiting_packets = sorted(self._outgoing_weather_packets[station_id].keys())

        other_queued  = False
        if len(waiting_packets) > 2:
            log.msg("Weather data packets with the following live sequence IDs are awaiting transmission "
                    "for station {0} while transmitting live {2}: {1}".format(
                station_id, repr(waiting_packets), live_sequence_id))
            other_queued = True

        while len(waiting_packets) > 0:
            packet_id = waiting_packets.pop(0)

            this_packet = self._outgoing_weather_packets[station_id][packet_id]

            if this_packet is None:
                log.msg("No data for packet {0} yet. Stopping transmission loop. The following additional "
                        "packets remain to be transmitted: {1}".format(packet_id, repr(waiting_packets)))
                # Stop transmission until we've got data for this packet to ensure packets aren't transmitted
                # out of order.
                break

            log.msg("Transmit packet {0} for station {1}. Queue is now: {2}".format(
                packet_id, station_id, repr(waiting_packets)))
            self._send_packet(this_packet)

            # Packet is sent, remove it from the queue
            self._outgoing_weather_packets[station_id].pop(packet_id, None)

        if other_queued and len(self._outgoing_weather_packets[station_id].keys()) == 0:
            log.msg("Live queue cleared.")