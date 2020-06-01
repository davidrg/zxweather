# coding=utf-8
"""
weather-push binary streaming protocol packet definitions. Has separate packet
types for TCP and UDP transport protocols.
"""
from math import log
import struct
import datetime
from twisted.python import log as _log

from zxw_push.common.data_codecs import timestamp_encode, timestamp_decode, find_sample_subfield_ids, \
    find_live_subfield_ids
from zxw_push.common.packets.common import Packet, HARDWARE_TYPE_CODES, \
    HARDWARE_TYPE_IDS, LiveDataRecord, WeatherRecord, SampleDataRecord, \
    StationInfoRecord

__author__ = 'david'

# Max UDP payload is 65,507 bytes


def _size_of_int(n):
    if n == 0:
        return 1
    return int(log(n, 256)) + 1


class UDPPacket(Packet):
    """
    Common functionality for all UDP packets.

    This class is not intended to be sent over the wire on its own. As such
    it has no packet type ID.

    The basic header for all packets is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Sequence                        |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Authorisation code                                                |
    +------+                                                                   +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+

    Fields:
       Packet Type         - Identifies what sort of packet this is
       Reserved            - Reserved for future use
       Sequence            - 16bit int that must be incremented with each packet
                             sent.
       Authorisation Code  - Unique 64bit unsigned integer used to identify the
                             client. This is used to control access to the
                             server.
    """

    # unsigned 16bit int (short) for the sequence number
    _FMT_SEQUENCE = "H"

    # 64bit unsigned integer for authorisation code
    _FMT_AUTH_CODE = "Q"

    # This is the common header for all udp packets
    _HEADER_FORMAT = Packet._FMT_ENDIANNESS + Packet._FMT_PACKET_TYPE + \
        Packet._FMT_RESERVED + _FMT_SEQUENCE + _FMT_AUTH_CODE

    def __init__(self, packet_type, sequence=0, authorisation_code=0):
        super(UDPPacket, self).__init__(packet_type)

        self._authorisation_code = authorisation_code
        self._sequence = sequence

    @property
    def authorisation_code(self):
        """
        The 64bit authorisation code.
        :rtype: int
        """
        return self._authorisation_code

    @authorisation_code.setter
    def authorisation_code(self, value):
        """
        Sets the 64bit authorisation code. Value must be greater than zero.
        :param value: 64bit unsigned integer
        :type value: int
        """
        if not (0 <= value <= 0xFFFFFFFF):
            raise ValueError("Value must be between 0 and 0xFFFFFFFF")

        self._authorisation_code = value

    @property
    def sequence(self):
        """
        Returns the 16bit packet sequence number
        :rtype: int
        """
        return self._sequence

    @sequence.setter
    def sequence(self, value):
        """
        Sets the 16bit packet sequence number. Must be greater than zero
        :param value: Packet sequence number
        :type value: int
        """

        if not (0 <= value <= 65535):
            raise ValueError("Value must be between 0 and 65535")

        self._sequence = value

    def _get_packet_header(self):
        packed = struct.pack(self._HEADER_FORMAT,
                             self.packet_type,
                             0,  # Reserved field (one byte)
                             self.sequence,
                             self.authorisation_code)

        pk_type, res, seq, auth = struct.unpack_from(
            self._HEADER_FORMAT, packed)

        assert(pk_type == self.packet_type)
        assert(res == 0)
        assert(seq == self.sequence)
        assert(auth == self.authorisation_code)

        return packed

    def _load_packet_header(self, packet_data):

        self._packet_type, reserved, self.sequence, \
            self._authorisation_code = struct.unpack_from(
                self._HEADER_FORMAT, packet_data)

    @staticmethod
    def _get_header_size():
        """
        Returns the size of the header.
        :rtype: int
        """
        return struct.calcsize(UDPPacket._HEADER_FORMAT)

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        return self._get_packet_header()

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        self._load_packet_header(packet_data)


class StationInfoRequestUDPPacket(UDPPacket):
    """
    Requests information about the stations the server knows about and will
    receive data for given the supplied authorisation code.

    This packets type id is 0x1

    The packet contains no additional data beyond the standard packet header.
    """

    def __init__(self, sequence=0, authorisation_code=0):
        super(StationInfoRequestUDPPacket, self).__init__(0x01, sequence,
                                                          authorisation_code)

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        return super(StationInfoRequestUDPPacket, self).encode()

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        super(StationInfoRequestUDPPacket, self).decode(packet_data)


class StationInfoResponseUDPPacket(UDPPacket):
    """
    Response to a StationInfoRequestPacket containing information for each
    station that can be accessed with the authorisation code originally
    supplied.

    This packets type ID is 0x2

    The basic structure is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Sequence                        |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Authorisation code                                                |
    +------+                                                                   +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    |12  96| List of stations...                                               |
    +------+----------------+----------------+----------------+----------------+

    A single station information record is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Station Code                                                     |
    +------+                +----------------+----------------+----------------+
    | 4  32|                | Hardware TypeID| Station ID     |
    +------+----------------+----------------+----------------+

    The fields are:
      Station Code    - 5 character string
      Hardware TypeID - Hardware type ID (not the same as the database ID)

    """

    _STATION_FMT = UDPPacket._FMT_ENDIANNESS + "5sBB"

    def __init__(self, sequence=0, authorisation_code=0):
        super(StationInfoResponseUDPPacket, self).__init__(0x02, sequence,
                                                           authorisation_code)

        self._stations = []
        """ :type self._stations: [StationInfoRecord] """

    @property
    def stations(self):
        """
        Returns a list of all the stations in the packet.

        :return: List of stations
        :rtype: [StationInfoRecord]
        """
        return self._stations

    def add_station(self, station_code, hardware_type, station_id):
        """
        Adds a station to the packet.

        :param station_code: 5 character station code
        :type station_code: str
        :param hardware_type: hardware type code
        :type hardware_type: str
        :param station_id: A unique value between 0 and 254 that identifies this
                           station
        :type station_id: int
        """

        self._stations.append(StationInfoRecord(station_code, hardware_type,
                                                station_id))

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        packet_data = super(StationInfoResponseUDPPacket, self).encode()

        for station in self._stations:
            packet_data += station.encode()

        # Add on the end of transmission marker to signal there are no more
        # stations in the list
        packet_data += struct.pack(UDPPacket._FMT_ENDIANNESS + "c", "\x04")
        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        # Decode the packet header
        super(StationInfoResponseUDPPacket, self).decode(packet_data)

        # Throw away the header data and decode all of the packets
        header_size = self._get_header_size()

        # station_size = struct.calcsize(self._STATION_FMT)
        station_size = StationInfoRecord.size()

        station_data = packet_data[header_size:]

        while True:
            if len(station_data) < station_size:
                _log.msg("Station Info Response Packet is malformed - "
                         "insufficient data for station record")
                return

            station = station_data[:station_size]
            station_data = station_data[station_size:]

            rec = StationInfoRecord()
            rec.decode(station)
            self._stations.append(rec)

            if station_data == '\x04':
                return  # End of transmission marker reached. No more stations.


class WeatherDataUDPPacket(UDPPacket):
    """
    This is a packet that contains one or more weather data records.

    This packets type ID is 0x3

    The basic structure is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Sequence                        |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Authorisation code                                                |
    +------+                                                                   +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    |12  96| Weather data records...                                           |
    +------+----------------+----------------+----------------+----------------+

    Weather data records are variable length with a header. When more than one
    record is present each record is terminated with the 0x1E (record separator)
    character. The full record set is terminated with the 0x04 (end of
    transmission) character.
    """

    def __init__(self, sequence=0, authorisation_code=0):
        super(WeatherDataUDPPacket, self).__init__(0x03, sequence,
                                                   authorisation_code)

        self._records = []
        self._encoded_records = None

    def add_record(self, record):
        """
        Adds a record to the data packet. Note that if the record depends on
        another record either:
          - the server must already have that record OR
          - that record must have already been added to this packet

        :param record: Record to add
        :type record: WeatherRecord
        """
        self._records.append(record)

    @property
    def records(self):
        """
        List of records present in the packet. If the packet has just been
        decoded from binary you'll need to call decode_records to populate this
        list.
        :return: Record list
        :rtype: list[WeatherRecord]
        """
        return list(self._records)

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        packet_data = super(WeatherDataUDPPacket, self).encode()

        if len(self._records) == 1:
            packet_data += self._records[0].encode()
        else:
            for record in self._records:
                packet_data += record.encode()
                packet_data += '\x1E'

        packet_data += '\x04'

        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        # Decode the header
        super(WeatherDataUDPPacket, self).decode(packet_data)

        header_size = self._get_header_size()

        self._encoded_records = packet_data[header_size:]

    @staticmethod
    def _decode_record(record_data):
        if record_data[0] == "\x01":
            # Live record

            if len(record_data) < LiveDataRecord.HEADER_SIZE:
                # Not enough data to decode record header
                record = WeatherRecord()
                record.decode(record_data)
                return record

            record = LiveDataRecord()
            record.decode(record_data)
        elif record_data[0] == "\x02":
            # Sample record

            if len(record_data) < SampleDataRecord.HEADER_SIZE:
                # Not enough data to decode record header
                record = WeatherRecord()
                record.decode(record_data)
                return record

            record = SampleDataRecord()
            record.decode(record_data)
        else:
            # Unknown/unsupported record
            return None
        return record

    def decode_records(self, hardware_type_map):
        """
        Decodes all records in the packet. This isn't done by the usual decode
        function as it required hardware type information that isn't contained
        within the packet.
        :param hardware_type_map: Map of station ID to hardware type code
        :type hardware_type_map: dict
        """
        end_of_transmission = False

        data_buffer = self._encoded_records

        record_data = ""

        while len(data_buffer) > 0:
            point = data_buffer.find("\x1E")

            if point == -1:
                # Not found. Try the end of transmission marker instead
                point = data_buffer.find("\x04")
                if point == -1:
                    _log.msg("** DECODE ERROR: Next record marker not found.")
                    return
                else:
                    # This might mark the end of the packet.
                    end_of_transmission = True

            if len(data_buffer) == 1 and end_of_transmission and \
               len(record_data) == 0:
                break  # No more records.

            # Copy the records data from the buffer
            record_data += data_buffer[:point]

            # And then remove that data from the buffer. We use +2 to remove the
            # end of record marker too
            data_buffer = data_buffer[point+1:]

            # Decode the record so we know what type it is
            record = self._decode_record(record_data)
            if record is None:
                _log.msg("** DECODE ERROR: Invalid record type ID {0}".format(
                    ord(record_data[0])))
                return

            # If it is a live or sample record see if we've got all the data
            if isinstance(record, SampleDataRecord) or \
               isinstance(record, LiveDataRecord):

                # Scan through the data and look for any known subfield headers. We need to
                # pick out and parse these subfield headers so we can calculate the proper
                # length for the whole record.
                if isinstance(record, SampleDataRecord):
                    subfields = find_sample_subfield_ids(record_data[record.header_size:],
                                                         record.field_list,
                                                         hardware_type_map[record.station_id])
                else:
                    subfields = find_live_subfield_ids(record_data[record.header_size:],
                                                       record.field_list,
                                                       hardware_type_map[record.station_id])

                calculated_size = record.calculated_record_size(
                    hardware_type_map[record.station_id], subfields)

                if len(record_data) > calculated_size:
                    _log.msg("** DECODE ERROR: Misplaced end of record marker "
                             "at {0}".format(point))
                    return

                if len(record_data) == calculated_size:
                    # Record decoded!
                    self.add_record(record)
                    record_data = ""
                    continue

            # ELSE: Its a WeatherDataRecord - this means we don't even have
            # enough data to decode the header of whatever type of record it is
            # Continue fetching data.

            if len(data_buffer) == 0 and end_of_transmission:
                _log.msg("** DECODE ERROR: Unable to decode record - no more "
                         "data in packet.")
                return

            # The record must contain the end of record character as part of
            # its data. Continue on...
            if end_of_transmission:
                record_data += "\x04"
            else:
                record_data += "\x1E"

            # Go around the loop again to add on another chunk of data.


class SampleAcknowledgementUDPPacket(UDPPacket):
    """
    This packet contains a list of samples the server has committed to its
    database along with additional statistics the client can use to adjust how
    it sends data.

    This packets type ID is 0x4

    The basic structure is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Sequence                        |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Authorisation code                                                |
    +------+                                                                   +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    |12  96| Lost Live Recs | Acknowledgement records...                       |
    +------+----------------+----------------+----------------+----------------+

    The lost live recs field indicates how many of the last 256 live records
    arrived out of order or went missing entirely on their way to the server. If
    this value is quite high the client should probably stop differencing live
    records or use TCP instead

    One acknowledgement record looks like this:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Record timestamp                                                 |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Station ID     |
    +------+----------------+

    The list of acknowledgement records is terminated with the end of
    transmission marker (0x04)

    """

    _FMT_PACKET_DATA = "!B"

    _FMT_ACKNOWLEDGEMENT_RECORD = "!LB"

    def __init__(self, sequence=0, authorisation_code=0):
        super(SampleAcknowledgementUDPPacket, self).__init__(0x04, sequence,
                                                             authorisation_code)

        self._lost_live_records = 0
        self._acknowledgements = []

    @property
    def lost_live_records(self):
        """
        Returns how many of the last 256 live data transmissions were never
        received by the server or were received out of order.
        :return: Number of live data transmissions that went missing
        :rtype: int
        """
        return self._lost_live_records

    @lost_live_records.setter
    def lost_live_records(self, value):
        """
        Sets how many of the last 256 live data transmissions were never
        received by the server or were received out of order

        :param value: Number of missing transmissions
        :type value: int
        """

        if not (0 <= value <= 255):
            raise ValueError("Value must be between 0 and 255")

        self._lost_live_records = value

    def add_sample_acknowledgement(self, station_id, sample_timestamp):
        """
        Adds a sample acknowledgement to the packet. This must be a sample that
        the receiver has successfully committed to disk such the server is
        guaranteed (as much as possible) to be able to supply that sample on
        request in the future.

        :param station_id: Server ID for the weather station
        :type station_id: int
        :param sample_timestamp: Timestamp of the sample.
        :type sample_timestamp: datetime.datetime
        :return:
        """

        # Make sure the data fits
        if not (0 <= station_id <= 255):
            raise ValueError("Station ID must be between 0 and 255")

        self._acknowledgements.append((station_id,
                                       timestamp_encode(sample_timestamp)))

    def sample_acknowledgements(self):
        """
        Returns a list of all sample acknowledgements contained in the packet

        :return: List of sample acknowledgements
        :rtype: list[(int,datetime.datetime)]
        """
        return [(x[0], timestamp_decode(x[1])) for x in self._acknowledgements]

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        data = super(SampleAcknowledgementUDPPacket, self).encode()

        data += struct.pack(self._FMT_PACKET_DATA, self._lost_live_records)

        for acknowledgement in self._acknowledgements:
            data += struct.pack(self._FMT_ACKNOWLEDGEMENT_RECORD,
                                acknowledgement[1], acknowledgement[0])
            #                     Timestamp         station ID

        data += struct.pack(UDPPacket._FMT_ENDIANNESS + "c", "\x04")

        return data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        super(SampleAcknowledgementUDPPacket, self).decode(packet_data)

        header_size = struct.calcsize(self._HEADER_FORMAT)

        self._lost_live_records = struct.unpack_from(
            self._FMT_PACKET_DATA,
            packet_data,
            offset=struct.calcsize(self._HEADER_FORMAT)
        )[0]

        header_size += struct.calcsize(self._FMT_PACKET_DATA)

        data = packet_data[header_size:]

        record_size = struct.calcsize(self._FMT_ACKNOWLEDGEMENT_RECORD)

        while len(data) > 0:
            if len(data) == 1:
                if data[0] == '\x04':
                    break  # Done processing records
                _log.msg("Malformed Sample Acknowledgement Packet - unexpected "
                         "end of record set.")
                return

            record = data[:record_size]
            data = data[record_size:]

            time_stamp, station_id = struct.unpack(
                self._FMT_ACKNOWLEDGEMENT_RECORD, record)

            self._acknowledgements.append((station_id, time_stamp))
