from twisted.internet import defer
from twisted.internet.protocol import DatagramProtocol
from twisted.python import log
from zxw_push.common.data_codecs import decode_live_data, decode_sample_data, \
    patch_live_from_live, patch_live_from_sample, patch_sample
from zxw_push.common.packets import decode_packet, StationInfoRequestPacket, \
    StationInfoResponsePacket, WeatherDataPacket, LiveDataRecord, \
    SampleDataRecord, SampleAcknowledgementPacket
from zxw_push.common.util import Sequencer
from zxw_push.server.database import ServerDatabase

__author__ = 'david'


class WeatherPushDatagramServer(DatagramProtocol):

    _MAX_LIVE_RECORD_CACHE = 5
    _MAX_SAMPLE_RECORD_CACHE = 1

    def __init__(self, dsn):
        self._dsn = dsn
        self._sequence_id = Sequencer()
        self._db = None
        self._next_station_id = 0
        self._station_code_id = {}
        self._station_id_code = {}
        self._station_id_hardware_type = {}

        self._lost_live_records = 0

        self._previous_live_record_id = 0
        self._live_record_cache = {}
        self._live_record_cache_ids = []

        self._sample_record_cache = {}
        self._sample_record_cache_ids = []

    def startProtocol(self):
        """
        Called when the protocol has started. Connects to the database, etc.
        """
        log.msg("Datagram Server started")
        self._db = ServerDatabase(self._dsn)

    def datagramReceived(self, datagram, address):
        """
        Processes a received datagram
        :param datagram: Received datagram data
        :param address: Sending address
        """
        log.msg("Received {0} from {1}".format(datagram, address))

        packet = decode_packet(datagram)

        if isinstance(packet, StationInfoRequestPacket):
            self._send_station_info(address, packet.authorisation_code)
        elif isinstance(packet, WeatherDataPacket):
            self._handle_weather_data(address, packet)

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
            hardware_type = station[1]
            if station_code not in self._station_code_id.keys():

                if self._next_station_id >= 255:
                    log.msg("*** WARNING: out of station IDs")
                    continue

                self._station_code_id[station_code] = self._next_station_id
                self._station_id_hardware_type[self._next_station_id] = \
                    hardware_type
                self._station_id_code[self._next_station_id] = station_code
                self._next_station_id += 1

            packet.add_station(station_code,
                               hardware_type,
                               self._station_code_id[station_code])

        # Client has probably just reconnected so we'll clear some state so
        # that the client can start sequence IDs from 0, etc.
        self._reset_tracking_variables()

        self._send_packet(packet, address)

    def _reset_tracking_variables(self):
        self._previous_live_record_id = 0
        self._lost_live_records = 0
        self._live_record_cache = {}
        self._live_record_cache_ids = []

    def _get_live_record(self, record_id, station_id):
        rid = (station_id, record_id)
        if rid in self._live_record_cache.keys():
            return self._live_record_cache[rid]

        return None

    def _cache_live_record(self, new_live, record_id, station_id):
        rid = (station_id, record_id)

        self._live_record_cache_ids.append(rid)
        self._live_record_cache[rid] = new_live

        while len(self._live_record_cache_ids) > self._MAX_LIVE_RECORD_CACHE:
            rid = self._live_record_cache_ids.pop(0)
            del self._live_record_cache[rid]

    @defer.inlineCallbacks
    def _get_sample_record(self, station_id, sample_id):
        rid = (station_id, sample_id)
        if rid in self._sample_record_cache.keys():
            defer.returnValue(self._sample_record_cache[rid])

        sample = yield self._db.get_sample(station_id, sample_id)

        if sample is not None:
            self._cache_sample_record(station_id, sample_id, sample)
            defer.returnValue(sample)

        defer.returnValue(None)

    def _cache_sample_record(self, station_id, sample_id, sample):
        rid = (station_id, sample_id)

        self._sample_record_cache_ids.append(rid)
        self._sample_record_cache[rid] = sample

        while len(self._sample_record_cache_ids) > self._MAX_SAMPLE_RECORD_CACHE:
            rid = self._sample_record_cache_ids.pop(0)
            del self._sample_record_cache[rid]

    @defer.inlineCallbacks
    def _handle_weather_data(self, address, packet):
        # The packet won't have fully decoded itself as it needs access to
        # hardware type information for each station id in the packet. So we
        # have to do that here.
        packet.decode_records(self._station_id_hardware_type)

        records = packet.records

        log.msg("Packet record count: {0}".format(len(records)))
        log.msg(packet._encoded_records)

        # Now run through the records processing each one. Processing in this
        # case means:
        #   - Patching in any missing data from other referenced packets
        #   - Inserting into the database
        #   - preparing an acknowledgement record

        ack_packet = SampleAcknowledgementPacket()

        for record in records:
            log.msg("Process record")

            fields = record.field_list
            data = record.field_data
            hw_type = self._station_id_hardware_type[record.station_id]
            station_code = self._station_id_code[record.station_id]

            if isinstance(record, LiveDataRecord):
                log.msg("Record is LIVE")
                if record.sequence_id < self._previous_live_record_id:
                    # The record ID has jumped back! This means either the
                    # packet has arrived out of order or the counter has
                    # overflowed.
                    diff = self._previous_live_record_id - record.sequence_id
                    if diff <= 60000:
                        # Probably a lost live record. If it had gone back more
                        # than 60k then we'd just assume a counter overflow.
                        self._lost_live_records += 1
                        log.msg("Ignoring live record {0} - out of order"
                                .format(record.sequence_id))
                        continue

                    # The counter has wrapped around. This record is OK but
                    # we need to check to see if any records in between have
                    # gone missing. The last received record was before the
                    # overflow (which is at 65535) and the current one is after
                    # it so:
                    missing_records = 65535 - self._previous_live_record_id
                    missing_records += record.sequence_id
                    self._lost_live_records += missing_records
                else:
                    # Take not of any missing records since the last received
                    # one
                    self._lost_live_records += (record.sequence_id -
                                                self._previous_live_record_id)

                rec_data = decode_live_data(data, hw_type, fields)

                if "live_diff_sequence" in rec_data:

                    other_live_id = rec_data["live_diff_sequence"]
                    other_live = yield self._get_live_record(other_live_id,
                                                             record.station_id)

                    if other_live is None:
                        # base record could not be found. This means that
                        # either:
                        #  1. It hasn't arrived yet (its coming out of order) OR
                        #  2. It went missing
                        # Either we can't decode this record. Count it as an
                        # error and move on.
                        self._lost_live_records += 1
                        continue

                    new_live = patch_live_from_live(rec_data, other_live,
                                                    fields, hw_type)

                elif "sample_diff_timestamp" in rec_data:
                    other_sample_id = rec_data["sample_diff_timestamp"]
                    other_sample = yield self._get_sample_record(
                        record.station_id, other_sample_id)

                    if other_sample is None:
                        # base record could not be found. This means that
                        # either:
                        #  1. It hasn't arrived yet (its coming out of order) OR
                        #  2. It went missing
                        # Either we can't decode this record. Count it as an
                        # error and move on.
                        self._lost_live_records += 1
                        continue

                    new_live = patch_live_from_sample(rec_data, other_sample,
                                                      fields, hw_type)
                else:
                    # No compression. All the data should be right there in
                    # the record.
                    new_live = rec_data

                # Update missing record statistics
                self._lost_live_records -= 1
                if self._lost_live_records < 0:
                    self._lost_live_records = 0

                # Insert decoded into the database as live data
                yield self._db.store_live_data(station_code, new_live, hw_type)

                # TODO: Broadcast to message bus if configured

                self._cache_live_record(new_live, record.sequence_id,
                                        record.station_id)
                self._previous_live_record_id = record.sequence_id

            elif isinstance(record, SampleDataRecord):
                log.msg("Record is SAMPLE")
                rec_data = decode_sample_data(data, hw_type, fields)

                if "sample_diff_timestamp" in rec_data:
                    other_sample_id = rec_data["sample_diff_timestamp"]
                    other_sample = self._get_sample_record(
                        record.station_id, other_sample_id)

                    if other_sample is None:
                        # base record could not be found. This means the client
                        # is making assumptions about what samples we've
                        # received rather than going by what we've told it.
                        # Naughty.
                        log.msg("*** NOTICE: Sample record {0} for station {1} "
                                "ignored - record depends on {2} which has not "
                                "been received. This is probably a client bug."
                                .format(record.timestamp, station_code,
                                        other_sample_id))
                        pass

                    new_sample = patch_sample(rec_data, other_sample,
                                              fields, hw_type)
                else:
                    # No compression. Use the record as-is
                    new_sample = rec_data

                # Insert decoded into the database as sample data and append to
                # acknowledgement packet
                self._db.store_sample(station_code, new_sample, hw_type)

                ack_packet.add_sample_acknowledgement(record.station_id,
                                                      record.timestamp)

        ack_packet.lost_live_records = self._lost_live_records
        #self._send_packet(ack_packet, address)
        # Done!
