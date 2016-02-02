# coding=utf-8
import struct

from zxw_push.common.data_codecs import get_field_ids_set, set_field_ids, \
    calculate_encoded_size, timestamp_encode, timestamp_decode

HARDWARE_TYPE_CODES = {
    0x01: "GENERIC",
    0x02: "FOWH1080",
    0x03: "DAVIS"
}

HARDWARE_TYPE_IDS = {
    "GENERIC": 0x01,
    "FOWH1080": 0x02,
    "DAVIS": 0x03
}


class Packet(object):
    """
    Common functionality for all TCP and UDP packets.

    This class is not intended to be sent over the wire on its own. As such
    it has no packet type ID.

    The basic header for all packets is:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       |
    +------+----------------+----------------+


    Fields:
       Packet Type         - Identifies what sort of packet this is
       Reserved            - Reserved for future use
    """

    # All packets use big endian
    _FMT_ENDIANNESS = "!"

    # unsigned char for packet type (0-255)
    _FMT_PACKET_TYPE = "B"

    # Three bytes reserved for future use
    _FMT_RESERVED = "B"

    _HEADER_FORMAT = _FMT_ENDIANNESS + _FMT_PACKET_TYPE + _FMT_RESERVED

    def __init__(self, packet_type):
        self._packet_type = packet_type

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

    @staticmethod
    def _get_header_size():
        """
        Returns the size of the header.
        :rtype: int
        """
        return struct.calcsize(Packet._HEADER_FORMAT)

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        return struct.pack( self._HEADER_FORMAT, self._packet_type, 0)

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        """

        self._packet_type, _ = struct.unpack_from(self._HEADER_FORMAT,
                                                  packet_data)


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


class StationInfoRecord(object):

    _STATION_FMT = "!5sBB"

    def __init__(self, station_code=None, hardware_type=None, station_id=None):

        assert (station_code is None or len(station_code) <= 5)

        self._station_code = station_code

        self._hardware_type_id = None

        if hardware_type is not None:
            self._hardware_type_id = HARDWARE_TYPE_IDS[hardware_type.upper()]

        assert (self._hardware_type_id is None or
                0 <= self._hardware_type_id <= 254)
        assert (station_id is None or 0 <= station_id <= 254)

        self._station_id = station_id

    @property
    def station_code(self):
        return self._station_code

    @property
    def hardware_type(self):
        return HARDWARE_TYPE_CODES[self._hardware_type_id]

    @property
    def station_id(self):
        return self._station_id

    def encode(self):
        return struct.pack(self._STATION_FMT,
                           self._station_code,
                           self._hardware_type_id,
                           self._station_id)

    def decode(self, data):
        self._station_code, self._hardware_type_id, self._station_id = \
            struct.unpack(self._STATION_FMT, data)

        self._station_code = self._station_code.split("\x00")[0]

    @staticmethod
    def size():
        return struct.calcsize(StationInfoRecord._STATION_FMT)

    def __str__(self):
        t = (self.station_code, self._hardware_type_id, self._station_id)
        return str(t)

    def __eq__(self, other):
        if other.station_code == self.station_code and \
                        other.hardware_type == self.hardware_type and \
                        other.station_id == self.station_id:
            return True
        return False
