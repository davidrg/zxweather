# coding=utf-8
import struct

from twisted.python import log
from twisted.internet import reactor

from zxw_push.common.data_codecs import timestamp_decode, timestamp_encode
from zxw_push.common.packets.common import Packet, StationInfoRecord, \
    SampleDataRecord, LiveDataRecord, WeatherRecord


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

        if not (0 <= authorisation_code <= 0xFFFFFFFF):
            raise ValueError("Value must be between 0 and 0xFFFFFFFF")

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

        self._authorisation_code = struct.unpack(self._FMT, packet_data)[0]

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
    This packet contains information about stations known to the server. This
    includes the station code, hardware type ID and a station ID.

    The packets payload always starts off with a single byte station count
    followed by one or more station records. The packets length is determined
    by the number of these records.

    The packets Type is 0x06

    +------+----------------+----------------+----------------+----------------+
    |Octet |       0        |       1        |       2        |       3        |
    +------+----------------+----------------+----------------+----------------+
    |   Bit| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7| 0 1 2 3 4 5 6 7|
    +------+----------------+----------------+----------------+----------------+
    | 0   0|  Packet Type   | Reserved       | Station Count  | Station 1      |
    +------+----------------+----------------+----------------+                +
    | 4  32|                                                                   |
    +------+                                 +----------------+----------------+
    | 8  64|                                 | Station 2...                    |
    +------+----------------+----------------+                                 +
    |12  96|                                                                   |
    +------+----------------+----------------+----------------+----------------+
    """

    _FMT_STATION_COUNT = "B"

    def __init__(self):
        super(StationInfoTCPPacket, self).__init__(0x06)
        self._stations = []

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
        packet_data = super(StationInfoTCPPacket, self).encode()

        packet_data += struct.pack(self._FMT_ENDIANNESS +
                                   self._FMT_STATION_COUNT,
                                   len(self._stations))

        for station in self._stations:
            packet_data += station.encode()

        return packet_data

    def decode(self, packet_data):
        """
        Decodes the packet data.
        :param packet_data: Data to decode
        :type packet_data: bytearray
        """

        # Decode the packet header
        super(StationInfoTCPPacket, self).decode(packet_data)

        # Throw away the header data and decode all of the packets
        header_size = self._get_header_size() + \
                      struct.calcsize(self._FMT_STATION_COUNT)

        # station_size = struct.calcsize(self._STATION_FMT)
        station_size = StationInfoRecord.size()

        station_data = packet_data[header_size:]

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

            if len(station_data) < station_size:
                log.msg("*** ERROR: Malformed station record - "
                        "insufficient data")

    @staticmethod
    def packet_size_bytes_required():
        return StationInfoTCPPacket._get_header_size() + \
               struct.calcsize(StationInfoTCPPacket._FMT_STATION_COUNT)

    @staticmethod
    def packet_size(packet_header):

        # Packet size depends on the number of records it contains. To figure
        # this out we need to get at the Station Count which is stored in the
        # third byte of the packet.

        header_size = StationInfoTCPPacket._get_header_size()
        interesting_data = packet_header[header_size:]

        record_count = struct.unpack_from(
                StationInfoTCPPacket._FMT_ENDIANNESS +
                StationInfoTCPPacket._FMT_STATION_COUNT,
                interesting_data)[0]

        record_size = StationInfoRecord.size()
        payload_header_size = struct.calcsize(
                StationInfoTCPPacket._FMT_STATION_COUNT)

        packet_size = header_size + payload_header_size + \
            record_size * record_count

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

    def __init__(self):
        super(WeatherDataTCPPacket, self).__init__(0x07)
        self._records = []
        self._encoded_records = []
        self._length = None

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

        packet_data = ''

        if len(self._records) == 1:
            log.msg("Add single record")###
            packet_data += self._records[0].encode()
        else:
            for record in self._records:
                log.msg("add record")###
                packet_data += record.encode()
                packet_data += '\x1E'

        length_size = struct.calcsize(self._FMT_LENGTH)

        packet_length = len(header_data) + length_size + len(packet_data)

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

        data_buffer = self._encoded_records

        record_data = ""

        while len(data_buffer) > 0 or len(record_data) > 0:

            rs = False

            if len(data_buffer) > 0:
                point = data_buffer.find("\x1E")  # Find record separators

                # Get the records data
                if point == -1:
                    # No record separator. This packet likely contains
                    # a single record.
                    record_data += data_buffer
                    data_buffer = ''
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
                log.msg("** DECODE ERROR: Invalid record type ID {0}".format(
                    ord(record_data[0])))

                return

            # If it is a live or sample record see if we've got all the data
            if isinstance(record, SampleDataRecord) or \
               isinstance(record, LiveDataRecord):

                calculated_size = record.calculated_record_size(
                    hardware_type_map[record.station_id])

                if len(record_data) > calculated_size:
                    log.msg("** DECODE ERROR: Misplaced end of record marker "
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

            if len(data_buffer) == 0 and not rs:
                log.msg("** DECODE ERROR: Unable to decode record - no more "
                        "data in packet.")
                return

            # The record must contain the end of record character as part of
            # its data. Continue on...
            record_data += "\x1E"

            # Go around the loop again to add on another chunk of data.


class SampleAcknowledgementTCPPacket(Packet):
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

        if len(self._acknowledgements) > 255:
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
