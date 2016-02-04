# coding=utf-8
"""
TCP implementation of the WeatherPush server. Much the same as the UDP version
but slightly different packets.
"""
from twisted.internet import defer, reactor, protocol
from twisted.python import log
from zxw_push.common.data_codecs import decode_live_data, decode_sample_data, \
    patch_live_from_live, patch_live_from_sample, patch_sample
from zxw_push.common.packets import decode_packet, LiveDataRecord, \
    SampleDataRecord, get_data_required_for_size_calculation, get_packet_size, \
    AuthenticateTCPPacket, WeatherDataTCPPacket, StationInfoTCPPacket, \
    SampleAcknowledgementTCPPacket, AuthenticateFailedTCPPacket, ImageTCPPacket, \
    ImageAcknowledgementTCPPacket
from zxw_push.common.util import Sequencer
from zxw_push.server.database import ServerDatabase

__author__ = 'david'


# TODO: The TCP and UDP servers could share a bit more code around sample
# decoding and caching


# Can't fix old-style-class-ness as its a twisted thing.
# noinspection PyClassicStyleClass
class WeatherPushTcpServer(protocol.Protocol):
    """
    Implements the server-side of the WeatherPush TCP protocol.
    """

    _MAX_LIVE_RECORD_CACHE = 5
    _MAX_SAMPLE_RECORD_CACHE = 1

    def __init__(self, authorisation_code):
        self._dsn = None
        self._db = None
        self._station_code_id = {}
        self._station_id_code = {}
        self._station_id_hardware_type = {}

        self._image_type_id_seq = Sequencer()
        self._image_type_code_id = dict()
        self._image_type_id_code = dict()

        self._image_source_id_seq = Sequencer()
        self._image_source_code_id = dict()
        self._image_source_id_code = dict()

        self._live_record_cache = {}
        self._live_record_cache_ids = {}

        self._sample_record_cache = {}
        self._sample_record_cache_ids = []

        self._ready = False

        self._receive_buffer = ''

        self._authenticated = False

        self._authorisation_code = authorisation_code

    def start_protocol(self, dsn):
        """
        Called when the protocol has started. Connects to the database, etc.
        """

        self._dsn = dsn

        log.msg("TCP Server started")
        self._db = ServerDatabase(self._dsn)

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

            if len(self._receive_buffer) < \
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
        encoded = packet.encode()

        self.transport.write(encoded)

    def _add_station_to_caches(self, station_code, hardware_type, station_id):
        if station_code not in self._station_code_id.keys():
            self._station_code_id[station_code] = station_id
            self._station_id_hardware_type[station_id] = hardware_type
            self._station_id_code[station_id] = station_code

    @defer.inlineCallbacks
    def _get_stations(self, authorisation_code):
        station_info = yield self._db.get_station_info(authorisation_code)

        station_set = []

        for station in station_info:
            station_code = station[0]
            hardware_type = station[1]
            # real_station_id = station[2]
            station_id = station[3]
            self._add_station_to_caches(station_code, hardware_type, station_id)

            station_set.append((station_code, hardware_type,
                                self._station_code_id[station_code]))

        defer.returnValue(station_set)

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

    def _reset_tracking_variables(self):
        self._live_record_cache = {}
        self._live_record_cache_ids = {}

    def _get_live_record(self, record_id, station_id):

        if station_id not in self._live_record_cache.keys():
            return None  # We've never seen live data for this station before.

        if record_id in self._live_record_cache[station_id].keys():
            return self._live_record_cache[station_id][record_id]

        return None

    def _cache_live_record(self, new_live, record_id, station_id):

        if station_id not in self._live_record_cache_ids.keys():
            self._live_record_cache_ids[station_id] = []
            self._live_record_cache[station_id] = dict()

        self._live_record_cache_ids[station_id].append(record_id)
        self._live_record_cache[station_id][record_id] = new_live

        while len(self._live_record_cache_ids[station_id]) > self._MAX_LIVE_RECORD_CACHE:
            rid = self._live_record_cache_ids[station_id].pop(0)
            del self._live_record_cache[station_id][rid]

    @staticmethod
    def _to_real_dict(value):
        result = {}

        for key in value.keys():
            result[key] = value[key]

        return result

    @defer.inlineCallbacks
    def _get_sample_record(self, station_id, time_stamp):
        """
        :type station_id: int
        :type time_stamp: datetime.datetime
        """
        rid = (station_id, time_stamp)

        if rid in self._sample_record_cache.keys():
            defer.returnValue(self._sample_record_cache[rid])

        station_code = self._station_id_code[station_id]
        hw_type = self._station_id_hardware_type[station_id]

        sample = yield self._db.get_sample(station_code, time_stamp, hw_type)

        if sample is not None:

            # Convert it to a real dict so things don't get confusing later
            sample = self._to_real_dict(sample)

            self._cache_sample_record(station_id, time_stamp, sample)
            defer.returnValue(sample)

        defer.returnValue(None)

    def _cache_sample_record(self, station_id, sample_id, sample):
        rid = (station_id, sample_id)

        self._sample_record_cache_ids.append(rid)
        self._sample_record_cache[rid] = sample

        while len(self._sample_record_cache_ids) > \
                self._MAX_SAMPLE_RECORD_CACHE:
            rid = self._sample_record_cache_ids.pop(0)
            del self._sample_record_cache[rid]

    @defer.inlineCallbacks
    def _build_live_from_sample_diff(self, data, station_id, fields, hw_type):
        """
        Decompresses a live record compressed using sample-diff. If the required
        sample can't be found None is returned to indicate failure.

        :param data: Live record data
        :type data: dict
        :param station_id: Station ID that recorded the data
        :type station_id: int
        :param fields: List of fields contained in the data
        :type fields: list[int]
        :param hw_type: Type of hardware used by the station
        :type hw_type: str
        :return: Decompressed live data or None on failure
        """
        other_sample_id = data["sample_diff_timestamp"]
        other_sample = yield self._get_sample_record(
            station_id, other_sample_id)

        if other_sample is None:
            # base record could not be found. This means that
            # either:
            #  1. It hasn't arrived yet (its coming out of order) OR
            #  2. It went missing
            # Either we can't decode this record. Count it as an
            # error and move on.
            log.msg("** NOTICE: Live record decoding failed - other sample "
                    "missing. This is probably a client bug.")
            defer.returnValue(None)

        new_live = patch_live_from_sample(data, other_sample,
                                          fields, hw_type)

        defer.returnValue(new_live)

    def _build_live_from_live_diff(self, data, station_id, fields, hw_type, sequence_id):
        """
        Decompresses a live record compressed using live-diff. If the required
        live record can't be found None is returned to indicate failure.

        :param data: Live record data
        :type data: dict
        :param station_id: Station ID that recorded the data
        :type station_id: int
        :param fields: List of fields contained in the data
        :type fields: list[int]
        :param hw_type: Type of hardware used by the station
        :type hw_type: str
        :return: Decompressed live data or None on failure
        """
        other_live_id = data["live_diff_sequence"]
        other_live = self._get_live_record(other_live_id, station_id)

        if other_live is None:
            # base record could not be found. This means that
            # either:
            #  1. It hasn't arrived yet (its coming out of order) OR
            #  2. It went missing
            # Either we can't decode this record. Count it as an
            # error and move on.
            log.msg("** NOTICE: Live record decoding failed for station {2} - other live not "
                    "in cache. This is probably a client bug. This live is {0},"
                    " other is {1}".format(sequence_id, other_live_id, station_id))
            return None

        return patch_live_from_live(data, other_live, fields, hw_type)

    @defer.inlineCallbacks
    def _handle_live_record(self, record):
        """
        Handles decoding and broadcasting a live record

        :param record: The received weather record
        :type record: LiveDataRecord
        :return: True on success, False on Failure
        :rtype: bool
        """

        fields = record.field_list
        data = record.field_data
        hw_type = self._station_id_hardware_type[record.station_id]
        station_code = self._station_id_code[record.station_id]

        rec_data = decode_live_data(data, hw_type, fields)

        if "live_diff_sequence" in rec_data:

            new_live = self._build_live_from_live_diff(
                rec_data, record.station_id, fields, hw_type,
                record.sequence_id
            )

        elif "sample_diff_timestamp" in rec_data:
            new_live = yield self._build_live_from_sample_diff(
                rec_data, record.station_id, fields, hw_type
            )

        else:
            # No compression. All the data should be right there in
            # the record.
            new_live = rec_data

        if new_live is None:
            # Couldn't decompress - base record doesn't exist
            defer.returnValue(False)

        self._cache_live_record(new_live, record.sequence_id,
                                record.station_id)

        # Insert decoded into the database as live data
        yield self._db.store_live_data(station_code, new_live)

        # TODO: Broadcast to message bus if configured

        defer.returnValue(True)

    @defer.inlineCallbacks
    def _handle_image_data(self, packet):
        source_code = self._image_source_id_code[packet.image_source_id]
        type_code = self._image_type_id_code[packet.image_type_id]

        yield self._db.store_image(source_code, type_code, packet.timestamp,
                                   packet.title, packet.description,
                                   packet.mime_type, packet.metadata,
                                   packet.image_data)

        ack = ImageAcknowledgementTCPPacket()
        ack.add_image(packet.image_source_id, packet.image_type_id,
                      packet.timestamp)

        self._send_packet(ack)

    @defer.inlineCallbacks
    def _handle_weather_data(self, packet):

        if not self._ready:
            return  # We're not ready to process data yet. Ignore it.

        # The packet won't have fully decoded itself as it needs access to
        # hardware type information for each station id in the packet. So we
        # have to do that here.
        packet.decode_records(self._station_id_hardware_type)

        records = packet.records

        # Now run through the records processing each one. Processing in this
        # case means:
        #   - Patching in any missing data from other referenced packets
        #   - Inserting into the database
        #   - preparing an acknowledgement record

        ack_packet = SampleAcknowledgementTCPPacket()

        sample_records = False

        for record in records:
            if isinstance(record, LiveDataRecord):
                ok = yield self._handle_live_record(record)

                if not ok:
                    continue

            elif isinstance(record, SampleDataRecord):

                fields = record.field_list
                data = record.field_data
                hw_type = self._station_id_hardware_type[record.station_id]
                station_code = self._station_id_code[record.station_id]

                rec_data = decode_sample_data(data, hw_type, fields)

                if "sample_diff_timestamp" in rec_data:
                    other_sample_timestamp = rec_data["sample_diff_timestamp"]
                    other_sample = yield self._get_sample_record(
                        record.station_id, other_sample_timestamp)

                    if other_sample is None:
                        # base record could not be found. This means the client
                        # is making assumptions about what samples we've
                        # received rather than going by what we've told it.
                        # Naughty.
                        log.msg("*** NOTICE: Sample record {0} for station {1} "
                                "ignored - record depends on {2} which has not "
                                "been received. This is probably a client bug."
                                .format(record.timestamp, station_code,
                                        other_sample_timestamp))
                        continue

                    new_sample = patch_sample(rec_data, other_sample,
                                              fields, hw_type)
                else:
                    # No compression. Use the record as-is
                    new_sample = rec_data

                new_sample["time_stamp"] = record.timestamp
                new_sample["download_timestamp"] = record.download_timestamp

                # Add the sample to the cache to allow future samples to be
                # decompressed without hitting the database.
                self._cache_sample_record(record.station_id,
                                          record.timestamp,
                                          new_sample)

                # Insert decoded into the database as sample data and append to
                # acknowledgement packet
                yield self._db.store_sample(station_code, new_sample)

                ack_packet.add_sample_acknowledgement(record.station_id,
                                                      record.timestamp)
                sample_records = True

        # Only send the acknowledgement packet if we've got samples to
        # acknowledge
        if sample_records:
            self._send_packet(ack_packet)

        # Done!
