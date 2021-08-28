import datetime
import os
import struct
import time

import serial

from davis_logger.record_types.dmp import encode_date
from davis_logger.record_types.loop import deserialise_loop, encode_time, deserialise_loop2, get_packet_type
from davis_logger.record_types.util import CRC

port = "COM3"
baud = 19200
rain_collector_size = 0.2

print("Open serial port {0}, {1} baud".format(port, baud))
serial_port = serial.Serial(port, baud, timeout=1.2)

def toHexString(string):
    """
    Converts the supplied string to hex.
    :param string: Input string
    :return:
    """
    result = ""
    for char in string:

        hex_encoded = hex((char))[2:]
        if len(hex_encoded) == 1:
            hex_encoded += '0'

        result += r'\x{0}'.format(hex_encoded)
    return result


class WakeError(Exception):
    """
    Raised when an attempt to wake the weather station fails.
    """

    def __init__(self, retries):
        self.message = "Failed to wake weather station console after {0} " \
                       "retries".format(retries)

    def __str__(self):
        return self.message


class UnexpectedResponseError(Exception):
    """
    Exception thrown when an unexpected response is received from a weather
    station.
    """

    def __init__(self, response):
        self.message = "Unexpected response from weather station: " \
                       "{0}".format(toHexString(response))


    def __str__(self):
        return self.message


class CrcError(Exception):
    """
    Raised when the calculated CRC value for a packet does not match the
    supplied CRC value.
    """

    def __init__(self, expected, calculated):
        self.message = "CRC Error. Expected value was {0}, " \
                       "calculated was {1}".format(expected, calculated)


    def __str__(self):
        return self.message



def wake():
    """
    Wakes the console.
    """
    max_retries = 3
    retries = max_retries
    while retries > 0:

        retries -= 1
        serial_port.write(b'\n')
        result = serial_port.read(2)

        if result == b'\n\r':
            break

        if retries == 0:
            raise WakeError(max_retries)

        time.sleep(1.2)


def loop_test():
    # Ask for 5 loop packets
    print("Request 3 LOOP and 3 LOOP2 packets...")
    serial_port.write(b'LPS 3 200\n')

    buffer = bytearray()

    loop_received = 0
    command_ack = False
    while loop_received < 200:
        buffer.extend(serial_port.read(99))

        if not command_ack and buffer[0:1] == b'\x06':
            print("LPS accepted")
            command_ack = True
            buffer = buffer[1:]

        if len(buffer) >= 99:
            # We have a loop packet!
            packet = buffer[0:99]
            buffer = buffer[99:]
            #print("LOOP: {0}".format(toHexString(packet)))

            packet_data = packet[0:97]
            packet_crc = struct.unpack(CRC.FORMAT, packet[97:])[0]

            crc = CRC.calculate_crc(packet_data)

            if crc == packet_crc:
                #print(repr(packet))
                #print("CRC OK")

                if get_packet_type(packet_data) == 0:
                    loop1 = deserialise_loop(packet_data, rain_collector_size)
                    print("LOOP1: wind={0} 10mavg={1} dir={2}".format(
                        loop1.windSpeed, loop1.averageWindSpeed10min,
                        loop1.windDirection))
                else:
                    loop2 = deserialise_loop2(packet_data, rain_collector_size)
                    print("LOOP2: wind={0} 10mavg={1} dir={4} gust={2} 2mavg={3}".format(
                        loop2.windSpeed, loop2.averageWindSpeed10min,
                        loop2.windGust10m, loop2.averageWindSpeed2min,
                        loop2.windDirection))
            else:
                print("CRC ERROR")

            loop_received += 1



def dmp_test(from_time):
    print("Request DMP...")
    serial_port.write('DMPAFT\n')
    ack = serial_port.read(1)
    assert ack == '\x06'

    date_stamp = encode_date(from_time.date())
    time_stamp = encode_time(from_time.time())
    packed = struct.pack('<HH', date_stamp, time_stamp)
    crc = CRC.calculate_crc(packed)
    packed += struct.pack(CRC.FORMAT, crc)
    serial_port.write(packed)

    buffer = ''
    while len(buffer) < 7:
        buffer += serial_port.read(7)
    assert buffer[0] == '\x06'
    buffer = buffer[1:]

    payload = buffer[0:4]
    crc = struct.unpack(CRC.FORMAT, buffer[4:])[0]
    calculated_crc = CRC.calculate_crc(payload)  # Skip over the ACK

    if crc != calculated_crc:
        print("CRC Error")
        return

    page_count, first_record_location = struct.unpack('<HH', payload)
    print('Pages: {0}, First Record: {1}'.format(
            page_count, first_record_location))
    serial_port.write('\x1B')


def receivers():
    serial_port.write("RECEIVERS\n")
    result = serial_port.read(100)
    print("Result: {0}".format(toHexString(result)))
    print("Result: {0}".format(result))

    assert result[0:6] == "\n\rOK\n\r"

    bitmap = result[6]
    print(toHexString(bitmap))

    # The bitmap just comes back as 0x00 - nothing interesting

# Wake the station
print("Wake...")
wake()

#receivers()

loop_test()

#dmp_test(datetime.datetime(year=2021, month=8, day=25, hour=20, minute=0))

