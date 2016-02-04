from twisted.python import log as _log

from zxw_push.common.packets.common import Packet, WeatherRecord, \
    LiveDataRecord, SampleDataRecord
from zxw_push.common.packets.tcp_packets import AuthenticateTCPPacket, \
    StationInfoTCPPacket, WeatherDataTCPPacket, SampleAcknowledgementTCPPacket, \
    AuthenticateFailedTCPPacket, ImageTCPPacket, ImageAcknowledgementTCPPacket
from zxw_push.common.packets.udp_packets import StationInfoRequestUDPPacket, \
    StationInfoResponseUDPPacket, WeatherDataUDPPacket, \
    SampleAcknowledgementUDPPacket, UDPPacket

_PACKET_TYPES = {
    # UDP Packets. These include an auth code and sequence number in every
    # packet. Variable length packets tend to be terminated with the End of
    # Transmission character.
    0x01: StationInfoRequestUDPPacket,
    0x02: StationInfoResponseUDPPacket,
    0x03: WeatherDataUDPPacket,
    0x04: SampleAcknowledgementUDPPacket,

    # TCP Packets. The auth code is sent once upon connection. Variable length
    # packets all include a size up-front to make decoding easier.
    0x05: AuthenticateTCPPacket,
    0x06: StationInfoTCPPacket,
    0x07: WeatherDataTCPPacket,
    0x08: SampleAcknowledgementTCPPacket,
    0x09: AuthenticateFailedTCPPacket,
    0x10: ImageTCPPacket,
    0x11: ImageAcknowledgementTCPPacket,
}

_TCP_PACKET_TYPES = [
    0x05,   # AuthenticateTCPPacket
    0x06,   # StationInfoTCPPacket
    0x07,   # WeatherDataTCPPacket
    0x08,   # SampleAcknowledgementTCPPacket
    0x09,   # AuthenticateFailedTCPPacket
    0x10,   # ImageTCPPacket
    0x11,   # ImageAcknowledgementTCPPacket
]


def decode_packet(packet_data):
    """
    Decodes the supplied packet data and returns it as an instance of one of
    the packet classes. If decoding fails then None is returned.

    :param packet_data: Input packet data
    :type packet_data: bytearray
    :return: The decoded packet
    :rtype: StationInfoRequestUDPPacket or StationInfoResponseUDPPacket or None
    """
    header = Packet(0)
    header.decode(packet_data)

    if header.packet_type not in _PACKET_TYPES.keys():
        _log.msg("*** WARNING: Invalid packet type {0}".format(
            header.packet_type))
        return None

    packet = _PACKET_TYPES[header.packet_type]()

    packet.decode(packet_data)

    return packet


def get_data_required_for_size_calculation(data):
    header = Packet(0)
    header.decode(data)
    packet_type = header.packet_type

    if packet_type in _TCP_PACKET_TYPES:
        return _PACKET_TYPES[packet_type].packet_size_bytes_required()

    raise Exception("Invalid TCP Packet Type: {0}".format(packet_type))


def get_packet_size(data):
    # Try to figure out the packet type. This should be the first byte in the
    # buffer

    header = Packet(0)
    header.decode(data)
    packet_type = header.packet_type

    if packet_type in _TCP_PACKET_TYPES:
        return _PACKET_TYPES[packet_type].packet_size(data)

    raise Exception("Invalid TCP Packet Type: {0}".format(packet_type))