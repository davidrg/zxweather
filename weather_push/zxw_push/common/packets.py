# coding=utf-8
"""
weather-push binary streaming protocol packet definitions
"""
from math import log
import struct
import datetime

from zxw_push.common.data_codecs import get_field_ids_set, set_field_ids, \
    timestamp_encode, timestamp_decode, calculate_encoded_size

__author__ = 'david'

# Max UDP payload is 65,507 bytes

_HARDWARE_TYPE_CODES = {
    0x01: "GENERIC",
    0x02: "WH1080",
    0x03: "DAVIS"
}

_HARDWARE_TYPE_IDS = {
    "GENERIC": 0x01,
    "WH1080": 0x02,
    "DAVIS": 0x03
}


def _size_of_int(n):
    if n == 0:
        return 1
    return int(log(n, 256)) + 1


class Packet(object):
    """
    Common functionality for all packets.

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

    # All packets use big endian
    _FMT_ENDIANNESS = "!"

    # unsigned char for packet type (0-255)
    _FMT_PACKET_TYPE = "B"

    # Three bytes reserved for future use
    _FMT_RESERVED = "B"

    # unsigned 16bit int (short) for the sequence number
    _FMT_SEQUENCE = "H"

    # 64bit unsigned integer for authorisation code
    _FMT_AUTH_CODE = "Q"

    # This is the common header for all packets
    _HEADER_FORMAT = _FMT_ENDIANNESS + _FMT_PACKET_TYPE + _FMT_RESERVED + \
        _FMT_SEQUENCE + _FMT_AUTH_CODE

    def __init__(self):
        self._packet_type = None
        self._authorisation_code = None
        self._sequence = None

    @property
    def packet_type(self):
        """
        Returns the packet type ID
        """
        return self._packet_type

    @packet_type.setter
    def packet_type(self, value):
        """
        Sets the packet type ID
        :param value: 8-bit packet type ID
        :type value: int
        """
        if not (0 <= value <= 255):
            raise ValueError("Value must be between 0 and 255")

        self._packet_type = value

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


class StationInfoRequestPacket(Packet):
    """
    Requests information about the stations the server knows about and will
    receive data for given the supplied authorisation code.

    This packets type id is 0x1

    The packet contains no additional data beyond the standard packet header.
    """

    def __init__(self, sequence=0, authorisation_code=0):
        super(StationInfoRequestPacket, self).__init__()

        self.authorisation_code = authorisation_code
        self.packet_type = 0x01
        self.sequence = sequence

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        return super(StationInfoRequestPacket, self).encode()

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        super(StationInfoRequestPacket, self).decode(packet_data)


class StationInfoResponsePacket(Packet):
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

    _STATION_FMT = Packet._FMT_ENDIANNESS + "5sBB"

    def __init__(self, sequence=0, authorisation_code=0):
        super(StationInfoResponsePacket, self).__init__()

        self.authorisation_code = authorisation_code
        self.packet_type = 0x02
        self.sequence = sequence

        self._stations = []

    @property
    def stations(self):
        """
        Returns a list of all the stations in the packet.

        :return: List of stations
        :rtype: [(str, str)]
        """
        return [(station[0], _HARDWARE_TYPE_CODES[station[1]], station[2])
                for station in self._stations]

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

        assert (len(station_code) <= 5)

        hardware_id = _HARDWARE_TYPE_IDS[hardware_type.upper()]

        assert (0 <= hardware_id <= 254)
        assert (0 <= station_id <= 254)

        self._stations.append((station_code, hardware_id, station_id))

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        packet_data = super(StationInfoResponsePacket, self).encode()

        for station in self._stations:
            encoded_station = struct.pack(self._STATION_FMT,
                                          station[0],  # Station code
                                          station[1],  # Hardware Type ID
                                          station[2])  # Station ID
            packet_data += encoded_station

        # Add on the end of transmission marker to signal there are no more
        # stations in the list
        packet_data += struct.pack(Packet._FMT_ENDIANNESS + "c", "\x04")
        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        # Decode the packet header
        super(StationInfoResponsePacket, self).decode(packet_data)

        # Throw away the header data and decode all of the packets
        header_size = struct.calcsize(self._HEADER_FORMAT)

        station_size = struct.calcsize(self._STATION_FMT)

        station_data = packet_data[header_size:]

        while True:
            if len(station_data) < station_size:
                # TODO: Log a warning - the packet is malformed
                pass

            station = station_data[:station_size]
            station_data = station_data[station_size:]

            station_code, hw_type_id, station_id = struct.unpack(
                self._STATION_FMT, station)

            # Station code will be padded with null bytes. Un-pad it.
            station_code = station_code.split("\x00")[0]

            self._stations.append((station_code, hw_type_id, station_id))

            if station_data == '\x04':
                return  # End of transmission marker reached. No more stations.


class WeatherRecord(object):
    """
    Base class for all record types. It has no record type ID.

    All records start with a header that looks like this:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Record Type   |
    +------+----------------+

    """

    # All packets use big endian
    _FMT_ENDIANNESS = "!"

    # unsigned char for packet type (0-255)
    _FMT_RECORD_TYPE = "B"

    _FMT_HEADER = _FMT_ENDIANNESS + _FMT_RECORD_TYPE

    def __init__(self):
        self._record_type = None
        self._station_id = None
        self._fields = None
        self._field_data = None

    @property
    def record_type(self):
        """
        Returns the record type ID
        """
        return self._record_type

    @record_type.setter
    def record_type(self, value):
        """
        Sets the record type ID
        :param value: 8-bit record type ID
        :type value: int
        """
        if not (0 <= value <= 255):
            raise ValueError("Value must be between 0 and 255")

        self._record_type = value

    @property
    def station_id(self):
        """
        Returns the station ID
        :return: station ID
        :rtype: int
        """
        return self._station_id

    @station_id.setter
    def station_id(self, value):
        """
        Sets the station ID.
        :param value: Station ID
        :type value: int
        """
        if not (0 <= value <= 255):
            raise ValueError("Value must be between 0 and 255")

        self._station_id = value

    @property
    def field_list(self):
        """
        A list of Field IDs present in the field data
        """
        return get_field_ids_set(self._fields)

    @field_list.setter
    def field_list(self, field_ids):
        """
        Sets the list of field IDs present in the field data
        :param field_ids: List of fields present in the field data
        :type field_ids: list[int]
        """
        self._fields = set_field_ids(field_ids)

    @property
    def field_data(self):
        """
        Returns the raw binary encoded field data
        :return: field data
        :rtype: bytearray
        """
        return self._field_data

    @field_data.setter
    def field_data(self, value):
        """
        sets the raw field data

        :param value: Raw binary encoded field data
        :type value: bytearray
        """
        self._field_data = value

    def _get_record_header(self):
        return struct.pack(self._FMT_HEADER, self.record_type)

    def _load_record_header(self, packet_data):
        self._record_type = struct.unpack_from(self._FMT_HEADER, packet_data)[0]

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        return self._get_record_header()

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        self._load_record_header(packet_data)

    def calculated_record_size(self, hardware_type):
        """
        Returns the calculated size of the record based on the specified
        hardware type
        :param hardware_type: Type of weather station that generated the record
        :type hardware_type: str
        :returns: Expected size of the record
        :rtype: int
        """
        return struct.calcsize(WeatherRecord._FMT_HEADER)


class LiveDataRecord(WeatherRecord):
    """
    This is a record that contains live weather data.

    Its record type ID is 0x01

    The basic structure is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Record Type   | Station ID     | Sequence ID                     |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Fields Bits                                                       |
    +------+----------------+----------------+----------------+----------------+
    | 8  64| Field set...                                                      |
    +------+----------------+----------------+----------------+----------------+

    The field set block is variable length. Its length can be computed based on
    which bits are set in the fields bitmask.
    """

    _RECORD_HEADER = '!BHL'
    HEADER_SIZE = 8

    def __init__(self):
        super(LiveDataRecord, self).__init__()
        self.record_type = 0x01  # Live
        self._sequence_id = None

    @property
    def sequence_id(self):
        """
        Gets the records live data sequence number
        """
        return self._sequence_id

    @sequence_id.setter
    def sequence_id(self, value):
        """
        Sets the records live data sequence number. This must be greater
        than the sequence number on the last record sent.
        :param value: Sequence number
        :type value: int
        """
        if not (0 <= value <= 65535):
            raise ValueError("Value must be between 0 and 65535")
        self._sequence_id = value

    def encode(self):
        """
        Encodes the live data record and returns its binary representation
        """
        packed = super(LiveDataRecord, self).encode()

        packed += struct.pack(self._RECORD_HEADER,
                              self.station_id,
                              self.sequence_id,
                              self._fields)

        packed += self._field_data

        return packed

    def decode(self, data):
        """
        Decodes the binary representation of a live data record and updates
        this objects values with those from the supplied data.
        :param data: Binary live data record
        :type data: bytearray
        """
        super(LiveDataRecord, self).decode(data)

        base_header_size = struct.calcsize(self._FMT_HEADER)

        self._station_id, self._sequence_id, self._fields = struct.unpack_from(
            self._RECORD_HEADER,
            data,
            base_header_size
        )

        header_size = base_header_size + struct.calcsize(self._RECORD_HEADER)

        self.field_data = data[header_size:]

    def encoded_size(self):
        """
        Returns how big the encoded form of the record should be
        """
        return len(self._field_data) + \
            struct.calcsize(self._RECORD_HEADER) + \
            struct.calcsize(self._FMT_HEADER)

    def calculated_record_size(self, hardware_type):
        """
        Returns the calculated size of the record based on the specified
        hardware type
        :param hardware_type: Type of weather station that generated the record
        :type hardware_type: str
        :returns: Expected size of the record
        :rtype: int
        """
        base_header = struct.calcsize(WeatherRecord._FMT_HEADER)
        record_header = struct.calcsize(self._RECORD_HEADER)
        field_size = calculate_encoded_size(self.field_list, hardware_type,
                                            True)
        return base_header + record_header + field_size


class SampleDataRecord(WeatherRecord):
    """
    This is a record that contains sample weather data.

    Its record type ID is 0x02

    The basic structure is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Record Type   | Station ID     | Timestamp...                    |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| ... Timestamp                   | Download timestamp...           |
    +------+----------------+----------------+----------------+----------------+
    | 8  64| ... Download timestamp          | Field bits...                   |
    +------+----------------+----------------+----------------+----------------+
    |12  96| ... Field bits                  | Field data...                   |
    +------+----------------+----------------+----------------+----------------+

    The field set block is variable length. Its length can be computed based on
    which bits are set in the fields bitmask.
    """

    _RECORD_HEADER = '!BLLL'

    HEADER_SIZE = 14

    def __init__(self):
        super(SampleDataRecord, self).__init__()
        self.record_type = 0x02  # Sample
        self._timestamp = None
        self._download_timestamp = None

    @property
    def timestamp(self):
        """
        Gets the samples timestamp
        :return: Sample timestamp
        :rtype: datetime.datetime
        """
        return self._timestamp

    @timestamp.setter
    def timestamp(self, value):
        """
        Sets the time when this sample was originally taken by the weather
        station hardware
        :param value: Sample timestamp
        :type value: datetime.datetime
        """
        self._timestamp = value

    @property
    def download_timestamp(self):
        """
        Gets the time when this sample was originally downloaded from the
        weather station hardware
        :return: sample download timestamp
        :rtype: datetime.datetime
        """
        return self._download_timestamp

    @download_timestamp.setter
    def download_timestamp(self, value):
        """
        Sets the time when the data was originally downloaded from the weather
        station hardware.

        :param value: Weather station download time
        :type value: datetime.datetime
        """
        self._download_timestamp = value

    def encode(self):
        """
        Encodes this sample data record and returns its binary representation
        :return: Binary representation of this object
        :rtype: bytearray
        """
        packed = super(SampleDataRecord, self).encode()

        packed += struct.pack(self._RECORD_HEADER,
                              self.station_id,
                              timestamp_encode(self._timestamp),
                              timestamp_encode(self._download_timestamp),
                              self._fields)

        packed += self._field_data

        return packed

    def decode(self, data):
        """
        Decodes the binary representation of a sample data record and updates
        this objects values with those from the supplied data.

        :param data: Binary sample data record
        :type data: bytearray
        """
        super(SampleDataRecord, self).decode(data)

        base_header_size = struct.calcsize(self._FMT_HEADER)

        self._station_id, timestamp, download_timestamp, self._fields = \
            struct.unpack_from(
                self._RECORD_HEADER,
                data,
                base_header_size
            )

        self.timestamp = timestamp_decode(timestamp)
        self.download_timestamp = timestamp_decode(download_timestamp)

        header_size = base_header_size + struct.calcsize(self._RECORD_HEADER)

        self.field_data = data[header_size:]

    def encoded_size(self):
        """
        Returns how big the encoded form of the record should be
        """
        return len(self._field_data) + \
            struct.calcsize(self._RECORD_HEADER) + \
            struct.calcsize(self._FMT_HEADER)

    def calculated_record_size(self, hardware_type):
        """
        Returns the calculated size of the record based on the specified
        hardware type
        :param hardware_type: Type of weather station that generated the record
        :type hardware_type: str
        :returns: Expected size of the record
        :rtype: int
        """
        base_header = struct.calcsize(WeatherRecord._FMT_HEADER)
        record_header = struct.calcsize(self._RECORD_HEADER)
        field_size = calculate_encoded_size(self.field_list, hardware_type,
                                            False)
        return base_header + record_header + field_size


class WeatherDataPacket(Packet):
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
        super(WeatherDataPacket, self).__init__()

        self.authorisation_code = authorisation_code
        self.packet_type = 0x03
        self.sequence = sequence
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
        packet_data = super(WeatherDataPacket, self).encode()

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
        super(WeatherDataPacket, self).decode(packet_data)

        header_size = struct.calcsize(self._HEADER_FORMAT)

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
                    # TODO: print("Next record marker not found")
                    return
                else:
                    # This might mark the end of the packet.
                    end_of_transmission = True

            if len(data_buffer) == 1 and end_of_transmission:
                break  # No more records.

            # Copy the records data from the buffer
            record_data += data_buffer[:point]

            # And then remove that data from the buffer. We use +2 to remove the
            # end of record marker too
            data_buffer = data_buffer[point+1:]

            # Decode the record so we know what type it is
            record = self._decode_record(record_data)
            if record is None:
                # TODO: print("Invalid record ID")
                return

            # If it is a live or sample record see if we've got all the data
            if isinstance(record, SampleDataRecord) or \
               isinstance(record, LiveDataRecord):

                calculated_size = record.calculated_record_size(
                    hardware_type_map[record.station_id])

                if len(record_data) > calculated_size:
                    # TODO: print("Corrupt packet - misplaced end of
                    # record marker")
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
                # TODO: print("Unable to decode record - no more data in
                # packet! Packet"
                #      " is malformed.")
                return

            # The record must contain the end of record character as part of
            # its data. Continue on...
            if end_of_transmission:
                record_data += "\x04"
            else:
                record_data += "\x1E"

            # Go around the loop again to add on another chunk of data.


class SampleAcknowledgementPacket(Packet):
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
    records.

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
        super(SampleAcknowledgementPacket, self).__init__()

        self.authorisation_code = authorisation_code
        self.packet_type = 0x04
        self.sequence = sequence

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
        data = super(SampleAcknowledgementPacket, self).encode()

        data += struct.pack(self._FMT_PACKET_DATA, self._lost_live_records)

        for acknowledgement in self._acknowledgements:
            data += struct.pack(self._FMT_ACKNOWLEDGEMENT_RECORD,
                                acknowledgement[1], acknowledgement[0])
            #                     Timestamp         station ID

        data += struct.pack(Packet._FMT_ENDIANNESS + "c", "\x04")

        return data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        super(SampleAcknowledgementPacket, self).decode(packet_data)

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
                # TODO: Log error - malformed packet

            record = data[:record_size]
            data = data[record_size:]

            time_stamp, station_id = struct.unpack(
                self._FMT_ACKNOWLEDGEMENT_RECORD, record)

            self._acknowledgements.append((station_id, time_stamp))


_PACKET_TYPES = {
    0x01: StationInfoRequestPacket,
    0x02: StationInfoResponsePacket,
    0x03: WeatherDataPacket,
    0x04: SampleAcknowledgementPacket
}


def decode_packet(packet_data):
    """
    Decodes the supplied packet data and returns it as an instance of one of
    the packet classes. If decoding fails then None is returned.

    :param packet_data: Input packet data
    :type packet_data: bytearray
    :return: The decoded packet
    :rtype: StationInfoRequestPacket or StationInfoResponsePacket or None
    """
    header = Packet()
    header.decode(packet_data)

    if header.packet_type not in _PACKET_TYPES.keys():
        # TODO: Log a warning - invalid packet type id
        return None

    packet = _PACKET_TYPES[header.packet_type]()

    packet.decode(packet_data)

    return packet
