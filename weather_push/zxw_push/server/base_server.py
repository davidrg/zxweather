import time
from datetime import datetime

from twisted.internet import defer
from twisted.python import log

from zxw_push.common.data_codecs import patch_live_from_sample, patch_live_from_live, decode_live_data, \
    decode_sample_data, patch_sample
from zxw_push.common.packets import LiveDataRecord, SampleDataRecord
from zxw_push.common.statistics_collector import MultiPeriodServerStatisticsCollector


class WeatherPushServerBase(object):
    _MAX_LIVE_RECORD_CACHE = 5
    _MAX_SAMPLE_RECORD_CACHE = 1

    _PROTO_UDP = 1
    _PROTO_TCP = 2

    def __init__(self, authorisation_code):
        self._authorisation_code = authorisation_code

        self._dsn = None
        self._db = None
        self._ready = False

        self._sample_record_cache = {}
        self._sample_record_cache_ids = []

        self._live_record_cache = {}
        self._live_record_cache_ids = {}
        self._previous_live_record_id = dict()
        self._lost_live_records = 0
        self._undecoded_live_records = dict()

        self._station_code_id = {}
        self._station_id_code = {}
        self._station_id_hardware_type = {}

        self._protocol = None

        self._statistics_collector = MultiPeriodServerStatisticsCollector(datetime.now, log.msg)

    def _encode_packet_for_sending(self, packet):
        encoded = packet.encode()

        self._statistics_collector.log_packet_transmission(packet.__class__.__name__,
                                                           len(encoded))

        return encoded

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

    def _reset_tracking_variables(self):
        self._live_record_cache = {}
        self._live_record_cache_ids = {}
        self._lost_live_records = 0
        self._previous_live_record_id = dict()

    @defer.inlineCallbacks
    def _get_live_record(self, record_id, station_id):

        if station_id not in self._live_record_cache.keys():
            log.msg("No live data cached for station {0}".format(station_id))
            # We've never seen live data for this station before.
            defer.returnValue(None)
            return

        if record_id in self._live_record_cache[station_id].keys():
            defer.returnValue(self._live_record_cache[station_id][record_id])
            return

        log.msg("Live record {0} for station {1} not found. Cached records are: {2}".format(
            record_id, station_id, repr(self._live_record_cache[station_id].keys())
        ))

        if station_id in self._undecoded_live_records:
            if record_id in self._undecoded_live_records[station_id]:
                log.msg("Found record {0} in undecoded records store. Attempting to decode...".format(
                    record_id))
                record = self._undecoded_live_records[station_id][record_id]
                new_record = yield self._decode_live_record(record, False, False)
                if new_record is not None:
                    log.msg("Successfully decoded record {0} from undecoded records store!".format(
                        record_id
                    ))
                    self._undecoded_live_records[station_id].pop(record_id, None)
                    self._statistics_collector.log_undecodable_sample_record()
                    defer.returnValue(new_record)
                    return

        defer.returnValue(None)

    def _cache_live_record(self, new_live, record_id, station_id):
        if station_id not in self._live_record_cache_ids.keys():
            self._live_record_cache_ids[station_id] = []
            self._live_record_cache[station_id] = dict()

        self._live_record_cache_ids[station_id].append(record_id)
        self._live_record_cache[station_id][record_id] = new_live

        while len(self._live_record_cache_ids[station_id]) > self._MAX_LIVE_RECORD_CACHE:
            rid = self._live_record_cache_ids[station_id].pop(0)
            del self._live_record_cache[station_id][rid]

        log.msg("live cache for station {0} is now: {1}".format(
            station_id, repr(sorted(self._live_record_cache[station_id]))
        ))

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
        rid = "{0}:{1}".format(station_id, time.mktime(time_stamp.timetuple()))

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

    def _cache_sample_record(self, station_id, time_stamp, sample):
        rid = "{0}:{1}".format(station_id, time.mktime(time_stamp.timetuple()))

        self._sample_record_cache_ids.append(rid)
        self._sample_record_cache[rid] = sample

        while len(self._sample_record_cache_ids) > \
                self._MAX_SAMPLE_RECORD_CACHE:
            rid = self._sample_record_cache_ids.pop(0)
            if rid in self._sample_record_cache.keys():
                try:
                    del self._sample_record_cache[rid]
                except KeyError:
                    # Don't care. We wanted it gone and apparently someone beat
                    # us to it.
                    pass

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

            if self._protocol == self._PROTO_TCP:
                log.msg("** NOTICE: Live record decoding failed - other sample "
                        "missing. This is probably a client bug.")
            else:
                log.msg("** NOTICE: Live record decoding failed - other sample "
                        "missing")
            self._lost_live_records += 1
            self._statistics_collector.log_undecodable_live_record()
            defer.returnValue(None)

        new_live = patch_live_from_sample(data, other_sample,
                                          fields, hw_type)

        defer.returnValue(new_live)

    @defer.inlineCallbacks
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
        other_live = yield self._get_live_record(other_live_id, station_id)

        log.msg("Decode live {0} based on {1}".format(sequence_id, other_live_id))

        if other_live is None:
            # base record could not be found. This means that
            # either:
            #  1. It hasn't arrived yet (its coming out of order) OR
            #  2. It went missing
            # Either we can't decode this record. Count it as an
            # error and move on.
            self._lost_live_records += 1
            log.msg("** NOTICE: Live record decoding failed for station {2} - other live not "
                    "in cache. Likely cause is network error or out-of-order processing. "
                    "This live is {0}, other is {1}".format(sequence_id, other_live_id, station_id))
            self._statistics_collector.log_undecodable_live_record()
            defer.returnValue(None)
            return

        defer.returnValue(patch_live_from_live(data, other_live, fields, hw_type))

    def _check_for_missing_live_records(self, station_id, sequence_id):
        """
        Checks that the live record has been received in order. Live records
        received out of order (specifically, old records that got delayed but
        still arrived) are of no use to anyone so we'll just throw them away.

        :param station_id:
        :param sequence_id:
        :return: True if sequence_id was received in order, False if it was
            received out of order.
        """
        if station_id not in self._previous_live_record_id:
            self._previous_live_record_id[station_id] = 0

        if sequence_id < self._previous_live_record_id[station_id]:
            # The record ID has jumped back! This means either the
            # packet has arrived out of order or the counter has
            # overflowed.
            diff = self._previous_live_record_id[station_id] - sequence_id
            if diff <= 60000:
                # Probably a lost live record. If it had gone back more
                # than 60k then we'd just assume a counter overflow.
                self._lost_live_records += 1
                log.msg("** NOTICE: Ignoring live record {0} - out of order"
                        .format(sequence_id))
                return False

            # The counter has wrapped around. This record is OK but
            # we need to check to see if any records in between have
            # gone missing. The last received record was before the
            # overflow (which is at 65535) and the current one is after
            # it so:
            missing_records = 65535 - self._previous_live_record_id[station_id]
            missing_records += sequence_id
            if missing_records > 1:
                self._lost_live_record(missing_records)
        else:
            # Take note of any missing records since the last received
            # one
            diff = sequence_id - self._previous_live_record_id[station_id]
            if diff > 1:
                self._lost_live_record(diff)

                self._previous_live_record_id[station_id] = sequence_id

        return True

    def _received_live_record(self):
        self._lost_live_records -= 1
        if self._lost_live_records < 0:
            self._lost_live_records = 0

    def _lost_live_record(self, count=1):
        self._lost_live_records += count
        if self._lost_live_records > 255:
            self._lost_live_records = 255

    @defer.inlineCallbacks
    def _decode_live_record(self, record, cache=True, put_aside=True):
        """
        Handles decoding a live record.

        On success the live record will be stored in the recent live records cache and returned.
        On failure None is returned.

        :param record: Received weather record
        :type record: LiveDataRecord
        :return: decoded live record
        """

        fields = record.field_list
        data = record.field_data
        hw_type = self._station_id_hardware_type[record.station_id]

        rec_data = decode_live_data(data, hw_type, fields)

        if "live_diff_sequence" in rec_data:

            new_live = yield self._build_live_from_live_diff(
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

            # We can now clear old un-decoded records from the un-decoded
            # records store.
            if record.station_id in self._undecoded_live_records:
                for key in self._undecoded_live_records[record.station_id].keys():
                    # Throw away all records older than this one or all records that
                    # are a long way in the future (to account for the sequence ID
                    # wrapping around)
                    if key < record.sequence_id or key > record.sequence_id + 100:
                        log.msg("Discarding live {0} for station {1} - "
                                "made obsolete by receipt of full live record".format(
                            key, record.station_id))
                        self._undecoded_live_records[record.station_id].pop(key, None)

        if new_live is None:
            # Couldn't decompress - base record doesn't exist

            if put_aside:
                # We'll just put it aside for now in case the base record turns up later.
                if record.station_id not in self._undecoded_live_records:
                    self._undecoded_live_records[record.station_id] = dict()
                self._undecoded_live_records[record.station_id][record.sequence_id] = record

                log.msg("Undecoded messages for station {0}: {1}".format(
                    record.station_id,
                    repr(sorted(self._undecoded_live_records[record.station_id].keys()))
                ))

            # Decode failed.
            defer.returnValue(None)
            return

        if cache:
            self._cache_live_record(new_live, record.sequence_id,
                                    record.station_id)

        defer.returnValue(new_live)

    @defer.inlineCallbacks
    def _handle_live_record(self, record):
        """
        Handles decoding and broadcasting a live record

        :param record: The received weather record
        :type record: LiveDataRecord
        :return: True on success, False on Failure
        :rtype: bool
        """
        new_live = yield self._decode_live_record(record)

        if new_live is None:
            defer.returnValue(False)
            return

        in_order = self._check_for_missing_live_records(record.station_id,
                                                        record.sequence_id)

        if not in_order:
            defer.returnValue(False)  # Record arrived too late to be useful.

        station_code = self._station_id_code[record.station_id]

        # Insert decoded into the database as live data
        yield self._db.store_live_data(station_code, new_live)

        # TODO: Broadcast to message bus if configured

        defer.returnValue(True)

    @defer.inlineCallbacks
    def _process_weather_records(self, weather_data_packet, samples_ack_packet):
        # The packet won't have fully decoded itself as it needs access to
        # hardware type information for each station id in the packet. So we
        # have to do that here.
        weather_data_packet.decode_records(self._station_id_hardware_type)

        records = weather_data_packet.records

        # Now run through the records processing each one. Processing in this
        # case means:
        #   - Patching in any missing data from other referenced packets
        #   - Inserting into the database
        #   - preparing an acknowledgement record

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
                        self._statistics_collector.log_undecodable_sample_record()
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

                samples_ack_packet.add_sample_acknowledgement(record.station_id,
                                                      record.timestamp)
                sample_records = True

        defer.returnValue(sample_records)
