# coding=utf-8
"""
Client implementation of Weather Push UDP protocol
"""
import datetime
from twisted.internet import reactor, defer
from twisted.internet.protocol import DatagramProtocol
from twisted.python import log

from zxw_push.common.data_codecs import encode_live_record, \
    encode_sample_record
from zxw_push.common.util import Event, Sequencer
from zxw_push.common.packets import StationInfoRequestUDPPacket, \
    decode_packet, StationInfoResponseUDPPacket, LiveDataRecord, \
    SampleDataRecord, WeatherDataUDPPacket, SampleAcknowledgementUDPPacket
import csv


__author__ = 'david'

# Set this to the name of the csv file to log packet statistics to. Set it to
# None to disable statistics logging.
_PACKET_STATISTICS_FILE = None


# No control over it being an old-style class
# noinspection PyClassicStyleClass
class WeatherPushDatagramClient(DatagramProtocol):
    """
    Handles sending weather data to a remote WeatherPush server via UDP
    """
    _STATION_LIST_TIMEOUT = 60  # seconds to wait for a response

    def __init__(self, ip_address, port, authorisation_code,
                 confirmed_sample_func, tcp_client_factory):
        self._ip_address = ip_address
        self._port = port
        self._authorisation_code = authorisation_code
        self._confirmed_sample_func = confirmed_sample_func
        self._tcp_client_factory = tcp_client_factory
        self._tcp_client = None

        self.Ready = Event()
        self.ReceiptConfirmation = Event()
        self.ImageReceiptConfirmation = Event()

        # 16bit integer sequences for packet and live records
        self._sequence_id = Sequencer()

        # These are all keyed by station id
        self._live_sequence_id = {}  # Sequencers for each station
        self._outgoing_samples = {}  # List of waiting samples for each station
        self._previous_live_record = {}  # Last live record for each station

        self._stations = None
        self._station_ids = None
        self._station_codes = None

        self._station_info_request_retries = 0

        self._ready_event_fired = False

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

        # How many compressed live records we should send before forcing a full
        # (uncompressed) record
        self._max_compressed_live_records = 30

        self._compressed_live_records_remaining = \
            self._max_compressed_live_records

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
        encoded = packet.encode()

        # payload_size = len(encoded)
        # udp_header_size = 8
        # ip4_header_size = 20
        # log.msg("Sending {0} packet. Payload {1} bytes. UDP size {2} bytes. "
        #         "IPv4 size {3} bytes.".format(
        #             packet.__class__.__name__,
        #             payload_size,
        #             payload_size + udp_header_size,
        #             payload_size + udp_header_size + ip4_header_size)
        #         )

        self._log_record_statistics(packet.sequence, packet.__class__.__name__,
                                    len(encoded), 0, "raw")

        self.transport.write(encoded, (self._ip_address, self._port))

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

    def _handle_station_list(self, station_list_packet):
        """
        Handles any received station list packets.
        :param station_list_packet: Packet containing a list of stations we can
                                    submit data for
        :type station_list_packet: StationInfoResponseUDPPacket
        """

        self._stations = {}
        self._station_ids = {}
        self._station_codes = {}

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

        if not self._ready_event_fired:
            self.Ready.fire(station_codes)
            self._ready_event_fired = True

    def _handle_sample_acknowledgements(self, packet):
        if self._stations is None:
            return

        self._lost_live_updates = packet.lost_live_records

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

    def send_image(self, image):
        # The UDP transport doesn't support sending images. So we rely on a
        # private instance of the TCP client to do that. This client instance
        # will only handle images - the rest of the weather data will go over
        # UDP.

        if self._tcp_client is None:
            log.msg("Starting TCP client to transmit image.")
            self._tcp_client = self._tcp_client_factory()

        self._tcp_client.send_image(image)

    # Cant be static as we need to stay compatible with other client classes
    # noinspection PyMethodMayBeStatic
    def flush_samples(self):
        """
        Instructs the client to flush its sample buffer to the network. This is
        primarily used for batching up a large number of samples by calling
        sendSample with the hold parameter set.

        The UDP Client always sends data in batches to improve efficiency -
        either with a live record or when the number of samples waiting gets
        large enough. As a result this function (and the hold parameter on
        sendSample) does nothing.
        """
        pass

    @staticmethod
    def _log_record_statistics(packet_id, record_type, original_size,
                               reduction_size, algorithm, live_id=None):
        if _PACKET_STATISTICS_FILE is not None:
            with open(_PACKET_STATISTICS_FILE, "ab") as csv_file:
                writer = csv.writer(csv_file)
                writer.writerow([
                    datetime.datetime.now(),
                    packet_id, record_type, original_size, reduction_size,
                    algorithm, live_id
                ])

    def _make_live_weather_record(self, data, previous_live, previous_sample,
                                  hardware_type, station_id, packet_id):

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
        seq_id = None

        if encoded is not None:
            record = LiveDataRecord()
            record.station_id = station_id
            record.sequence_id = self._live_sequence_id[station_id]()
            record.field_list = field_ids
            record.field_data = encoded

            seq_id = record.sequence_id

        # Log some compression statistics
        original_size = compression[0]
        reduction_size = compression[1]
        algorithm = compression[2]
        # new_size_percentage = ((original_size - reduction_size) /
        #                        (original_size * 1.0)) * 100.0

        self._log_record_statistics(packet_id, "LIVE", original_size,
                                    reduction_size, algorithm, seq_id)

        # log.msg("Reduced LIVE   by {0} bytes (new size is {1}%) using "
        #         "algorithm {2}. Live ID: {3}".format(reduction_size,
        #                                              new_size_percentage,
        #                                              algorithm,
        #                                              seq_id))

        # Track how many compressed records we've sent.
        if algorithm != "none":
            self._compressed_live_records_remaining -= 1

        return record

    def _make_sample_weather_record(self, data, previous_sample, hardware_type,
                                    station_id, packet_id):

        # Samples are always compressed when it makes sense (which is
        # practically always). There is no forced uncompressed record as we
        # always know exactly what the remote server has so we're never
        # compressing against data the server might not have which would prevent
        # it from being able to decompress

        encoded, field_ids, compression = encode_sample_record(
            data, previous_sample, hardware_type)

        record = SampleDataRecord()
        record.station_id = station_id
        record.field_list = field_ids
        record.field_data = encoded
        record.timestamp = data["time_stamp"]
        record.download_timestamp = data["download_timestamp"]

        # Log some compression statistics
        original_size = compression[0]
        reduction_size = compression[1]
        algorithm = compression[2]
        # new_size_percentage = ((original_size - reduction_size) /
        #                        (original_size * 1.0)) * 100.0

        self._log_record_statistics(packet_id, "SAMPLE", original_size,
                                    reduction_size, algorithm)

        # log.msg("Reduced SAMPLE by {0} bytes (new size is {1}%) using "
        #         "algorithm {2}".format(reduction_size, new_size_percentage,
        #                                algorithm))

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

        previous_live_record = self._previous_live_record[station_id]
        previous_sample = yield self._confirmed_sample_func(
            self._station_codes[station_id])

        weather_records = []

        # UDP max is really 65507 but we want to avoid fragmenting the packet
        # as it will reduce the chance of it arriving at its destination intact.
        # 512 bytes should be a safe number but we'll leave a bit of room for
        # a live record.
        max_sample_payload = 450

        data_size = 0

        packet_id = self._sequence_id()

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
                                                          station_id,
                                                          packet_id)
                weather_records.append(record)

                # While we haven't confirmed the server has received this sample
                # the next sample in the packet can be diffed from it anyway as
                # if it gets the next sample its guaranteed to get this one too
                # as they're in the same packet.
                previous_sample = sample

                data_size += record.encoded_size()

                if data_size > max_sample_payload:
                    # Enough samples! We need to leave some space in this packet
                    # for the live data update
                    break

        if previous_sample is not None:
            live_data["sample_diff_timestamp"] = previous_sample["time_stamp"]

        if previous_live_record is not None:
            live_data["live_diff_sequence"] = previous_live_record[1]

        previous_live = None
        if previous_live_record is not None:
            previous_live = previous_live_record[0]

        live_record = self._make_live_weather_record(
            live_data, previous_live, previous_sample,
            hardware_type, station_id, packet_id
        )

        # live_record is None when compression throws away *all* data (meaning
        # there is no change from the last live record so no differences to
        # send)
        if live_record is None and len(weather_records) == 0:
            # Nothing to send. Roll-back the packet ID so it doesn't jump
            # forward when we do actually send something.
            self._sequence_id.rollback()
            return

        if live_record is not None:
            weather_records.append(live_record)

            self._previous_live_record[station_id] = (live_data,
                                                      live_record.sequence_id)

        packet = WeatherDataUDPPacket(packet_id,
                                      self._authorisation_code)

        for record in weather_records:
            packet.add_record(record)

        self._send_packet(packet)
