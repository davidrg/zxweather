# coding=utf-8
import struct

from twisted.python import log

from zxw_push.common.data_codecs import timestamp_decode, timestamp_encode, find_sample_subfield_ids, \
    find_live_subfield_ids
from zxw_push.common.packets.common import Packet, StationInfoRecord, \
    SampleDataRecord, LiveDataRecord, WeatherRecord

def toHexString(string):
    """
    Converts the supplied string to hex.
    :param string: Input string
    :return:
    """

    if isinstance(string, bytearray):
        string = str(string)

    result = ""
    for char in string:

        hex_encoded = hex(ord(char))[2:]
        if len(hex_encoded) == 1:
            hex_encoded = '0' + hex_encoded

        result += r'\x{0}'.format(hex_encoded)
    return result


class TcpPacket(Packet):
    def __init__(self, packet_type):
        super(TcpPacket, self).__init__(packet_type)

    @staticmethod
    def packet_size_bytes_required():
        """
        This is the number of bytes from the start of the packet we need to see
        before we can figure out how big the entire packet is.
        :returns: Number of bytes packet_size() requires
        :rtype: int
        """
        return 1

    @staticmethod
    def packet_size(packet_header):
        """
        Given the packet header, calculate how big the full packet is.

        :param packet_header: The header from the packet. This should contain at
                              least the number of bytes returned by
                              packet_size_bytes_required()
        :type packet_header: bytearray
        :return: Number of bytes in the packet
        :rtype: int
        """
        return Packet._get_header_size()


class AuthenticateTCPPacket(TcpPacket):
    """
    This packet authenticates a TCP connection. Upon successful authentication
    the server will send back a StationInfoTCPPacket.

    This packets Type is 0x05. It is always 10 bytes long.

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Authorisation Code...           |
    +------+----------------+----------------+                                 +
    | 4  32|                                                                   |
    +------+                                 +----------------+----------------+
    | 8  64|                                 |
    +------+----------------+----------------+
    """

    _FMT_AUTH_CODE = "Q"

    _FMT = Packet._FMT_ENDIANNESS + _FMT_AUTH_CODE

    def __init__(self, authorisation_code=0):
        super(AuthenticateTCPPacket, self).__init__(0x05)

        if not (0 <= authorisation_code <= 0xFFFFFFFFFFFFFFFF):
            raise ValueError("Value must be between 0 and 0xFFFFFFFFFFFFFFFF")

        self._authorisation_code = authorisation_code

    @property
    def authorisation_code(self):
        return self._authorisation_code

    def encode(self):
        result = super(AuthenticateTCPPacket, self).encode()
        result += struct.pack(self._FMT, self._authorisation_code)
        return result

    def decode(self, packet_data):
        super(AuthenticateTCPPacket, self).decode(packet_data)

        header_size = self._get_header_size()

        packet_data = packet_data[header_size:]

        self._authorisation_code, = struct.unpack(self._FMT, packet_data)


    @staticmethod
    def packet_size(packet_header):
        return 10  # This packet is always exactly 10 bytes.


class AuthenticateFailedTCPPacket(TcpPacket):
    """
    This is a packet sent by the server when authentication fails. As there are
    no details about the authentication failure this packet is nothing more than
    the standard packet header with no payload.

    The packet type is 0x09.

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       |
    +------+----------------+----------------+
    """
    def __init__(self):
        super(AuthenticateFailedTCPPacket, self).__init__(0x09)


class StationInfoTCPPacket(TcpPacket):
    """
    This packet contains information about stations, image types and image
    sources known to the server. For each station this includes the station
    code, hardware type ID and a station ID. For image types and sources this
    is just a code and an ID.

    The packets payload always starts off with a single byte station count
    followed by a single byte image type count and a single byte image source
    count. After this come the station records, image type records and
    image source records.

    The packets Type is 0x06

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Station Count  | Image Type Cnt |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Image Src Cnt  | Station 1....                                    |
    +------+----------------+                                                  +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    |12  96| Station 2...                                                      |
    +------+----------------+----------------+----------------+----------------+
    """

    _FMT_PAYLOAD = "BBB"

    _FMT_CODE_MAP = "5sB"

    def __init__(self):
        super(StationInfoTCPPacket, self).__init__(0x06)
        self._stations = []
        self._image_types = []
        self._image_sources = []

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

    @property
    def image_types(self):
        """
        Returns a list of all image type code to id mappings in the packet
        :return: List of image type code to image type id mappings
        :rtype: [(str, int)]
        """

        return self._image_types

    def add_image_type(self, type_code, type_id):
        self._image_types.append((type_code, type_id))

    @property
    def image_sources(self):
        """
        Returns a list of all image source code to id mappings in the packet
        :return: List of image source code to image source id mappings
        :rtype: [(str, int)]
        """
        return self._image_sources

    def add_image_source(self, source_code, source_id):
        self._image_sources.append((source_code, source_id))

    def encode(self):
        """
        Returns the encoded form of the packet which can be transmitted over a
        network.

        :rtype: bytearray
        """
        packet_data = super(StationInfoTCPPacket, self).encode()

        packet_data += struct.pack(self._FMT_ENDIANNESS +
                                   self._FMT_PAYLOAD,
                                   len(self._stations),
                                   len(self._image_types),
                                   len(self._image_sources))

        for station in self._stations:
            packet_data += station.encode()

        for image_type in self._image_types:
            packet_data += struct.pack(self._FMT_CODE_MAP,
                                       image_type[0].encode('utf-8'),
                                       image_type[1])

        for image_source in self._image_sources:
            packet_data += struct.pack(self._FMT_CODE_MAP,
                                       image_source[0].encode('utf-8'),
                                       image_source[1])

        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        # Decode the packet header
        super(StationInfoTCPPacket, self).decode(packet_data)

        header_size = self._get_header_size()

        packet_data = packet_data[header_size:]

        station_count, image_type_count, image_source_count = \
            struct.unpack_from(self._FMT_ENDIANNESS +
                               self._FMT_PAYLOAD, packet_data)

        # Throw away the header data and decode all of the packets

        station_size = StationInfoRecord.size()
        code_map_size = struct.calcsize(self._FMT_CODE_MAP)

        station_offset = struct.calcsize(self._FMT_PAYLOAD)
        station_list_size = station_size * station_count
        image_type_offset = station_offset + station_list_size
        image_type_list_size = code_map_size * image_type_count
        image_source_offset = image_type_offset + image_type_list_size

        station_data = packet_data[station_offset:image_type_offset]
        image_type_data = packet_data[image_type_offset:image_source_offset]
        image_source_data = packet_data[image_source_offset:]

        while len(station_data) > 0:
            if len(station_data) < station_size:
                log.msg("Station Info Response Packet is malformed - "
                        "insufficient data for station record")
                return

            station = station_data[:station_size]
            station_data = station_data[station_size:]

            rec = StationInfoRecord()
            rec.decode(station)
            self._stations.append(rec)

            if 0 < len(station_data) < station_size:
                log.msg("*** ERROR: Malformed station record - "
                        "insufficient data")

        self._image_types = self._unpack_code_map(image_type_data)
        self._image_sources = self._unpack_code_map(image_source_data)

    def _unpack_code_map(self, code_map_data):
        results = []
        code_map_size = struct.calcsize(self._FMT_CODE_MAP)

        while len(code_map_data) > 0:
            if len(code_map_data) < code_map_size:
                log.msg("Station Info Response Packet is malformed - "
                        "insufficient data for code map record")
                return None

            code_map = code_map_data[:code_map_size]
            code_map_data = code_map_data[code_map_size:]

            code, value_id = struct.unpack(self._FMT_CODE_MAP, code_map)

            results.append((code.split(b"\x00")[0].decode('utf-8'), value_id))

            if 0 < len(code_map_data) < code_map_size:
                log.msg("*** ERROR: Malformed code map record - "
                        "insufficient data")

        return results

    @staticmethod
    def packet_size_bytes_required():
        return StationInfoTCPPacket._get_header_size() + \
               struct.calcsize(StationInfoTCPPacket._FMT_PAYLOAD)

    @staticmethod
    def packet_size(packet_header):

        # Packet size depends on the number of records it contains. To figure
        # this out we need to get at the Station Count which is stored in the
        # third byte of the packet.

        header_size = StationInfoTCPPacket._get_header_size()
        interesting_data = packet_header[header_size:]

        station_count, image_type_count, image_source_count = \
            struct.unpack_from(StationInfoTCPPacket._FMT_ENDIANNESS +
                               StationInfoTCPPacket._FMT_PAYLOAD,
                               interesting_data)

        station_size = StationInfoRecord.size()
        code_map_size = struct.calcsize(StationInfoTCPPacket._FMT_CODE_MAP)

        station_list_size = station_size * station_count
        image_type_list_size = code_map_size * image_type_count
        image_source_list_size = code_map_size * image_source_count

        payload_header_size = struct.calcsize(
                StationInfoTCPPacket._FMT_PAYLOAD)

        packet_size = header_size + payload_header_size + \
            station_list_size + image_type_list_size + image_source_list_size

        return packet_size


class WeatherDataTCPPacket(TcpPacket):
    """
    This packet contains one or more Weather Data records.

    The packets payload always starts off with the length of the packet as a
    16bit unsigned integer and is followed by a variable length list of variable
    length weather data records.

    The packets Type is 0x07

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Length                          |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Records...                                                        |
    +------+                                                                   +
    | 8  64|                                                                   |
    +------+----------------+----------------+----------------+----------------+

    When more than one record is present records are terminated with 0x1E
    (ASCII Record Separator). The final record is unterminated.

    """

    _FMT_LENGTH = "H"

    def __init__(self, log_output=log.msg):
        super(WeatherDataTCPPacket, self).__init__(0x07)
        self._records = []
        self._encoded_records = []
        self._length = None
        self._log = log_output

    @staticmethod
    def packet_size_bytes_required():
        """
        This is the number of bytes from the start of the packet we need to see
        before we can figure out how big the entire packet is.
        :returns: Number of bytes packet_size() requires
        :rtype: int
        """

        # We need access to the length field (bytes 3 and 4) to know how big
        # the packet is.

        return WeatherDataTCPPacket._get_header_size() + struct.calcsize(
                WeatherDataTCPPacket._FMT_LENGTH)

    @staticmethod
    def packet_size(packet_header):

        header_size = WeatherDataTCPPacket._get_header_size()
        interesting_data = packet_header[header_size:]

        # Number
        size = struct.unpack_from(WeatherDataTCPPacket._FMT_ENDIANNESS +
                                  WeatherDataTCPPacket._FMT_LENGTH,
                                  interesting_data)[0]

        return size

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
        header_data = super(WeatherDataTCPPacket, self).encode()

        packet_data = b''

        if len(self._records) == 1:
            packet_data += self._records[0].encode()
        else:
            for record in self._records:
                packet_data += record.encode()
                packet_data += b'\x1E'

        length_size = struct.calcsize(self._FMT_LENGTH)

        packet_length = len(header_data) + length_size + len(packet_data)

        if packet_length > 65535:
            raise Exception("WeatherDataTCPPacket is over-full. Maximum allowed size is 65535 bytes. "
                            "This packet is {0} and contains {1} records.".format(
                packet_length, len(self._records)
            ))

        packet_size = struct.pack(self._FMT_ENDIANNESS + self._FMT_LENGTH,
                                  packet_length)

        packet_data = header_data + packet_size + packet_data

        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        """

        # Decode the header
        super(WeatherDataTCPPacket, self).decode(packet_data)

        payload = packet_data[self._get_header_size():]

        header_size = struct.calcsize(self._FMT_LENGTH)

        header = payload[:header_size]

        self._length = struct.unpack(self._FMT_ENDIANNESS + self._FMT_LENGTH,
                                     header)[0]

        self._encoded_records = payload[header_size:]

    @staticmethod
    def _decode_record(record_data):
        if record_data[0:1] == b"\x01":
            # Live record

            if len(record_data) < LiveDataRecord.HEADER_SIZE:
                # Not enough data to decode record header
                record = WeatherRecord()
                record.decode(record_data)
                return record

            record = LiveDataRecord()
            record.decode(record_data)
        elif record_data[0:1] == b"\x02":
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

        data_buffer = self._encoded_records

        record_data = b""

        while len(data_buffer) > 0 or len(record_data) > 0:

            rs = False

            if len(data_buffer) > 0:
                point = data_buffer.find(b"\x1E")  # Find record separators

                # Get the records data
                if point == -1:
                    # No record separator. This packet likely contains
                    # a single record.
                    record_data += data_buffer
                    data_buffer = b''
                    rs = False
                else:
                    # Copy the records data from the buffer
                    record_data += data_buffer[:point]
                    # And then remove that data from the buffer. We use +2 to
                    # remove the end of record marker too
                    data_buffer = data_buffer[point+1:]
                    rs = True

            # Decode the record so we know what type it is
            record = self._decode_record(record_data)
            if record is None:
                self._log("** DECODE ERROR: Invalid record type ID {0}. Full record is:\n{1}".format(
                    ord(record_data[0]), toHexString(record_data)))
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

                # If subfields comes back None it means there was insufficient data to
                # search for subfields. We need to continue accreting data.
                if subfields is not None:
                    calculated_size = record.calculated_record_size(
                        hardware_type_map[record.station_id], subfields)

                    if len(record_data) > calculated_size:
                        self._log("** DECODE ERROR: Misplaced end of record marker "
                                "at {0}".format(point))
                        return

                    if len(record_data) == calculated_size:
                        # Record decoded!
                        self.add_record(record)
                        record_data = b""
                        continue

            # ELSE: Its a WeatherDataRecord - this means we don't even have
            # enough data to decode the header of whatever type of record it is
            # Continue fetching data.

            if len(data_buffer) == 0 and not rs:
                self._log("** DECODE ERROR: Unable to decode record - no more "
                        "data in packet.")
                return

            # The record must contain the end of record character as part of
            # its data. Continue on...
            record_data += b"\x1E"

            # Go around the loop again to add on another chunk of data.


class SampleAcknowledgementTCPPacket(TcpPacket):
    """
    This packet contains a list of samples the server has committed to its
    database. It consists of a record count followed by a variable length
    list of fixed size records.

    The packets Type ID is 0x08.

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Record Count   | Record 1       |
    +------+----------------+----------------+----------------+                +
    | 4  32|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    | 8  64| Record 2...                                                       |
    +------+----------------+----------------+----------------+----------------+

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
    """

    _FMT_PACKET_DATA = "!B"

    _FMT_ACKNOWLEDGEMENT_RECORD = "!LB"

    def __init__(self):
        super(SampleAcknowledgementTCPPacket, self).__init__(0x08)
        self._acknowledgements = []

    @staticmethod
    def packet_size_bytes_required():
        return SampleAcknowledgementTCPPacket._get_header_size() + \
               struct.calcsize(SampleAcknowledgementTCPPacket._FMT_PACKET_DATA)

    @staticmethod
    def packet_size(packet_header):

        # Packet size depends on the number of records it contains. To figure
        # this out we need to get at the Record Count which is stored in the
        # third byte of the packet.

        header_size = SampleAcknowledgementTCPPacket._get_header_size()
        interesting_data = packet_header[header_size:]

        record_count = struct.unpack_from(
                SampleAcknowledgementTCPPacket._FMT_PACKET_DATA,
                interesting_data)[0]

        record_size = struct.calcsize(
                SampleAcknowledgementTCPPacket._FMT_ACKNOWLEDGEMENT_RECORD)

        payload_header_size = struct.calcsize(
                SampleAcknowledgementTCPPacket._FMT_PACKET_DATA)

        packet_size = header_size + payload_header_size + \
            record_size * record_count

        return packet_size

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
        :return: True if there is more space in the packet, False if the packet
                 is full
        :rtype: bool
        """

        # Make sure the data fits
        if not (0 <= station_id <= 255):
            raise ValueError("Station ID must be between 0 and 255")

        if len(self._acknowledgements) >= 255:
            return False

        self._acknowledgements.append((station_id,
                                       timestamp_encode(sample_timestamp)))

        return True

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
        data = super(SampleAcknowledgementTCPPacket, self).encode()

        data += struct.pack(self._FMT_PACKET_DATA, len(self._acknowledgements))

        for acknowledgement in self._acknowledgements:
            data += struct.pack(self._FMT_ACKNOWLEDGEMENT_RECORD,
                                acknowledgement[1], acknowledgement[0])
            #                     Timestamp         station ID

        return data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        super(SampleAcknowledgementTCPPacket, self).decode(packet_data)

        header_size = struct.calcsize(self._HEADER_FORMAT)

        header_size += struct.calcsize(self._FMT_PACKET_DATA)

        data = packet_data[header_size:]

        record_size = struct.calcsize(self._FMT_ACKNOWLEDGEMENT_RECORD)

        while len(data) > 0:
            if len(data) < record_size:
                log.msg("Malformed Sample Acknowledgement Packet - insufficient"
                        "data")
                return

            record = data[:record_size]
            data = data[record_size:]

            time_stamp, station_id = struct.unpack(
                self._FMT_ACKNOWLEDGEMENT_RECORD, record)

            self._acknowledgements.append((station_id, time_stamp))


class ImageTCPPacket(TcpPacket):
    """
    This packet contains a single image.

    The packets Type ID is 0x10.

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Length...                       |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| ...Length                       | Image Type     | Image Source   |
    +------+----------------+----------------+----------------+----------------+
    | 8  64| Timestamp                                                         |
    +------+----------------+----------------+----------------+----------------+
    |12  96| Text Length                                                       |
    +------+----------------+----------------+----------------+----------------+
    |16 128| Variable length data...                                           |
    +------+----------------+----------------+----------------+----------------+

    The variable length data field contains the text followed by the image data.
    The location for the image data within the packet is 16+text_length. The
    length of the image data is packet_length-(16+text_length).

    The text segment of the variable length data field consists of the following
    values separated by 0x1E:
      * Title
      * Description
      * MIME Type
      * Metadata
    """

    _FMT_PAYLOAD_HEADER = "LBBLL"

    def __init__(self, image_type_id=None, image_source_id=None, timestamp=None,
                 title=None, description=None, mime_type=None, metadata=None,
                 image_data=None):
        super(ImageTCPPacket, self).__init__(0x10)

        if image_type_id is None or image_source_id is None or \
                timestamp is None or image_data is None:
            # A packet to encode requires all of these things. So we're probably
            # creating a blank packet to decode into.
            return

        if title is None:
            title = ''
        if description is None:
            description = ''
        if mime_type is None:
            mime_type = ''
        if metadata is None:
            metadata = ''

        self._image_type_id = image_type_id
        self._image_source_id = image_source_id
        self._timestamp = timestamp_encode(timestamp)
        self._title = title.encode('utf-8')
        self._description = description.encode('utf-8')
        self._mime_type = mime_type.encode('utf-8')
        self._metadata = metadata.encode('utf-8')
        self._image_data = image_data

    @property
    def image_type_id(self):
        return self._image_type_id

    @property
    def image_source_id(self):
        return self._image_source_id

    @property
    def timestamp(self):
        return timestamp_decode(self._timestamp)

    @property
    def title(self):
        return self._title

    @property
    def description(self):
        return self._description

    @property
    def mime_type(self):
        return self._mime_type

    @property
    def metadata(self):
        return self._metadata

    @property
    def image_data(self):
        return self._image_data

    def encode(self):
        text_section = self._title + b'\x1E' + \
            self._description + b'\x1E' + self._mime_type + b'\x1E' + \
            self._metadata

        text_length = len(text_section)

        total_length = self._get_header_size() + \
            struct.calcsize(self._FMT_ENDIANNESS + self._FMT_PAYLOAD_HEADER) + text_length + \
            len(self._image_data)

        packet_data = super(ImageTCPPacket, self).encode()

        packet_data += struct.pack(self._FMT_ENDIANNESS +
                                   self._FMT_PAYLOAD_HEADER,
                                   total_length,
                                   self._image_type_id,
                                   self._image_source_id,
                                   self._timestamp,
                                   text_length)

        packet_data += text_section

        packet_data += self._image_data

        assert(len(packet_data) == total_length)

        return packet_data

    def decode(self, packet_data):
        data_len = len(packet_data)
        super(ImageTCPPacket, self).decode(packet_data)

        packet_data = packet_data[self._get_header_size():]

        total_length, self._image_type_id, self._image_source_id, \
            self._timestamp, text_length = struct.unpack_from(
                self._FMT_ENDIANNESS + self._FMT_PAYLOAD_HEADER, packet_data)

        if data_len != total_length:
            log.msg("*** Packet data does not match expected size of packet. "
                    "Expected {0} bytes, got {1}. Unable to decode.".format(
                        total_length, data_len))
            return

        payload_header_size = struct.calcsize(
            self._FMT_ENDIANNESS + self._FMT_PAYLOAD_HEADER)
        packet_data = packet_data[payload_header_size:]

        # packet data now contains the vardata section only.
        text_data = packet_data[:text_length]
        self._image_data = packet_data[text_length:]

        # text data consists of the following fields separated by 0x1E:
        #  + title
        #  + description
        #  + mime type
        #  + metadata

        bits = text_data.split(b'\x1E')
        self._title = bits[0].decode('utf-8')
        self._description = bits[1].decode('utf-8')
        self._mime_type = bits[2].decode('utf-8')
        self._metadata = bits[3].decode('utf-8')

    @staticmethod
    def packet_size_bytes_required():
        # packet type - 1 byte
        # reserved    - 1 byte
        # length      - 4 bytes
        return 6

    @staticmethod
    def packet_size(packet_header):

        return struct.unpack_from(ImageTCPPacket._FMT_ENDIANNESS + "L",
                             packet_header[2:])[0]


class ImageAcknowledgementTCPPacket(TcpPacket):
    """
    This packet contains a list of images the server has committed to its
    database. It consists of a record count followed by a variable length
    list of fixed size records.

    The packets Type ID is 0x11.

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Record Count   | Record 1       |
    +------+----------------+----------------+----------------+                +
    | 4  32|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    | 8  64| Record 2...                                                       |
    +------+----------------+----------------+----------------+----------------+

    One acknowledgement record looks like this:
    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Image timestamp                                                  |
    +------+----------------+----------------+----------------+----------------+
    | 4  32| Source ID      | Type ID        |
    +------+----------------+----------------+
    """

    _FMT_HEADER = "B"
    _FMT_RECORD = "LBB"

    def __init__(self):
        super(ImageAcknowledgementTCPPacket, self).__init__(0x11)
        self._images = []

    def add_image(self, source_id, type_id, timestamp):
        self._images.append((source_id, type_id, timestamp))

    @property
    def images(self):
        return self._images

    def encode(self):
        packet_data = super(ImageAcknowledgementTCPPacket, self).encode()

        packet_data += struct.pack(self._FMT_ENDIANNESS +self._FMT_HEADER,
                                   len(self._images))

        for image in self._images:
            packet_data += struct.pack(self._FMT_ENDIANNESS + self._FMT_RECORD,
                                       timestamp_encode(image[2]),  # timestamp
                                       image[0],  # source
                                       image[1])  # type

        return packet_data

    def decode(self, packet_data):
        super(ImageAcknowledgementTCPPacket, self).decode(packet_data)

        packet_data = packet_data[self._get_header_size():]

        record_count, = struct.unpack_from(
            self._FMT_ENDIANNESS + self._FMT_HEADER,
            packet_data)

        record_data = packet_data[struct.calcsize(self._FMT_ENDIANNESS +
                                                  self._FMT_HEADER):]

        record_size = struct.calcsize(self._FMT_ENDIANNESS + self._FMT_RECORD)

        if len(record_data) != record_count * record_size:
            log.msg("*** ERROR: Record section size is incorrect. "
                    "Expected {0}, got {1}. Unable to decode packet.".format(
                        record_count*record_size, len(record_data)))

        while len(record_data) > 0:
            ts, src, typ = struct.unpack_from(
                self._FMT_ENDIANNESS + self._FMT_RECORD, record_data)
            self._images.append((src, typ, timestamp_decode(ts)))

            record_data = record_data[record_size:]

    @staticmethod
    def packet_size_bytes_required():
        header_size = struct.calcsize(
            ImageAcknowledgementTCPPacket._FMT_ENDIANNESS +
            ImageAcknowledgementTCPPacket._FMT_HEADER)
        packet_header_size = ImageAcknowledgementTCPPacket._get_header_size()
        return header_size + packet_header_size

    @staticmethod
    def packet_size(packet_header):
        packet_type, reserved, record_count = struct.unpack_from(
            "!BBB", packet_header)

        record_size = struct.calcsize(
            ImageAcknowledgementTCPPacket._FMT_ENDIANNESS +
            ImageAcknowledgementTCPPacket._FMT_RECORD)
        header_size = struct.calcsize(
            ImageAcknowledgementTCPPacket._FMT_ENDIANNESS +
            ImageAcknowledgementTCPPacket._FMT_HEADER)
        packet_header_size = ImageAcknowledgementTCPPacket._get_header_size()

        return packet_header_size + header_size + record_count * record_size
