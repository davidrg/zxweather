# coding=utf-8
"""
A tool for getting and setting the stations date, time and timezone settings
"""
import datetime
import os
import struct
import textwrap

__author__ = 'david'

import argparse
import serial


class CRC(object):
    """
    For calculating the 16bit CRC on data coming back from the weather station.
    """

    # From section XII, Davis part 07395.801 rev 2.5 (30-JUL-2012)
    # "Vantage Pro, Vantage Pro2 and Vantage Vue Serial Communication Reference
    # Manual"
    crc_table = [
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    ]

    FORMAT = '>H'

    @staticmethod
    def calculate_crc(byte_string):
        """
        Calculates the CRC value for the supplied string.
        :param byte_string: The string of bytes to calculate the CRC for
        :type byte_string: str
        :return: The CRC value
        :rtype: int
        """

        crc = 0

        for byte in byte_string:
            data = ord(byte)
            table_idx = ((crc >> 8) ^ data) & 0xffff
            crc = (CRC.crc_table[table_idx] ^ (crc << 8)) & 0xffff

        return crc


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


class DavisWeatherStation(object):
    """
    Class for interfacing with davis weather stations.
    """

    _ACK = '\x06'

    # These are all the supported time zones
    time_zones = [
        (0, -1200, '(GMT-12:00) Eniwetok, Kwajalein'),
        (1, -1100, '(GMT-11:00) Midway Island, Samoa'),
        (2, -1000, '(GMT-10:00) Hawaii'),
        (3, -900, '(GMT-09:00) Alaska'),
        (4, -800, '(GMT-08:00) Pacific Time, Tijuana'),
        (5, -700, '(GMT-07:00) Mountain Time'),
        (6, -600, '(GMT-06:00) Central Time'),
        (7, -600, '(GMT-06:00) Mexico City'),
        (8, -600, '(GMT-06:00) Central America'),
        (9, -500, '(GMT-05.00) Bogota, Lima, Quito'),
        (10, -500, '(GMT-05:00) Eastern Time'),
        (11, -400, '(GMT-04:00) Atlantic Time'),
        (12, -400, '(GMT-04.00) Caracas, La Paz, Santiago'),
        (13, -330, '(GMT-03.30) Newfoundland'),
        (14, -300, '(GMT-03.00) Brasilia'),
        (15, -300, '(GMT-03.00) Buenos Aires, Georgetown, Greenland'),
        (16, -200, '(GMT-02.00) Mid-Atlantic'),
        (17, -100, '(GMT-01:00) Azores, Cape Verde Is.'),
        (18, 0,
         '(GMT)       Greenwich Mean Time, Dublin, Edinburgh, Lisbon, London'),
        (19, 0, '(GMT)       Monrovia, Casablanca'),
        (20, 100,
         '(GMT+01.00) Berlin, Rome, Amsterdam, Bern, Stockholm, Vienna'),
        (21, 100,
         '(GMT+01.00) Paris, Madrid, Brussels, Copenhagen, W Central Africa'),
        (22, 100,
         '(GMT+01.00) Prague, Belgrade, Bratislava, Budapest, Ljubljana'),
        (23, 200,
         '(GMT+02.00) Athens, Helsinki, Istanbul, Minsk, Riga, Tallinn'),
        (24, 200, '(GMT+02:00) Cairo'),
        (25, 200, '(GMT+02.00) Eastern Europe, Bucharest'),
        (26, 200, '(GMT+02:00) Harare, Pretoria'),
        (27, 200, '(GMT+02.00) Israel, Jerusalem'),
        (28, 300, '(GMT+03:00) Baghdad, Kuwait, Nairobi, Riyadh'),
        (29, 300, '(GMT+03.00) Moscow, St. Petersburg, Volgograd'),
        (30, 330, '(GMT+03:30) Tehran'),
        (
            31, 400,
            '(GMT+04:00) Abu Dhabi, Muscat, Baku, Tblisi, Yerevan, Kazan'),
        (32, 430, '(GMT+04:30) Kabul'),
        (33, 500, '(GMT+05:00) Islamabad, Karachi, Ekaterinburg, Tashkent'),
        (34, 530, '(GMT+05:30) Bombay, Calcutta, Madras, New Delhi, Chennai'),
        (35, 600, '(GMT+06:00) Almaty, Dhaka, Colombo, Novosibirsk, Astana'),
        (36, 700, '(GMT+07:00) Bangkok, Jakarta, Hanoi, Krasnoyarsk'),
        (37, 800,
         '(GMT+08:00) Beijing, Chongqing, Urumqi, Irkutsk, Ulaan Bataar'),
        (38, 800,
         '(GMT+08:00) Hong Kong, Perth, Singapore, Taipei, Kuala Lumpur'),
        (39, 900, '(GMT+09:00) Tokyo, Osaka, Sapporo, Seoul, Yakutsk'),
        (40, 930, '(GMT+09:30) Adelaide'),
        (41, 930, '(GMT+09:30) Darwin'),
        (42, 1000, '(GMT+10:00) Brisbane, Melbourne, Sydney, Canberra'),
        (43, 1000, '(GMT+10.00) Hobart, Guam, Port Moresby, Vladivostok'),
        (44, 1100, '(GMT+11:00) Magadan, Solomon Is, New Caledonia'),
        (45, 1200, '(GMT+12:00) Fiji, Kamchatka, Marshall Is.'),
        (46, 1200, '(GMT+12:00) Wellington, Auckland')
    ]

    def __init__(self, port):
        self._port = port
        self._hw_type = None

    def _wake(self):
        """
        Wakes the console.
        """
        max_retries = 3
        retries = max_retries
        while retries > 0:

            retries -= 1
            self._port.write('\n')
            result = self._port.read(2)

            if result == '\n\r':
                break

            if retries == 0:
                raise WakeError(max_retries)

    @property
    def hardware_type(self):
        """
        The weather stations hardware type.
        :rtype: int,str
        """

        if self._hw_type is not None:
            return self._hw_type

        self._wake()

        self._port.write('WRD\x12\x4D\n')
        result = self._port.read(2)

        if result[0] == self._ACK:
            station_type = ord(result[1])
            hw_type = "Unknown"
            if station_type == 0:
                hw_type = "Wizard III"
            elif station_type == 1:
                hw_type = "Wizard II"
            elif station_type == 2:
                hw_type = "Monitor"
            elif station_type == 3:
                hw_type = "Perception"
            elif station_type == 4:
                hw_type = "GroWeather"
            elif station_type == 5:
                hw_type = "Energy Enviromonitor"
            elif station_type == 6:
                hw_type = "Health Enviromonitor"
            elif station_type == 16:
                hw_type = "Vantage Pro, Vantage Pro2"
            elif station_type == 17:
                hw_type = "Vantage Vue"

            self._hw_type = station_type, hw_type
            return self._hw_type

        raise UnexpectedResponseError(result)

    def get_time(self):
        """
        Gets the current time and date on the weather station.
        :returns: Current date and time on the weather station
        :rtype: datetime.datetime
        """

        self._wake()

        self._port.write("GETTIME\n")
        response = self._port.read(9)

        if response[0] == self._ACK:

            packet_crc = struct.unpack(CRC.FORMAT, response[7:])[0]
            calculated_crc = CRC.calculate_crc(response[1:7])

            if packet_crc != calculated_crc:
                raise CrcError(packet_crc, calculated_crc)

            seconds = ord(response[1])
            minutes = ord(response[2])
            hour = ord(response[3])
            day = ord(response[4])
            month = ord(response[5])
            year = ord(response[6]) + 1900

            timestamp = datetime.datetime(year=year, month=month, day=day,
                                          hour=hour,
                                          minute=minutes, second=seconds)
            return timestamp
        else:
            raise UnexpectedResponseError(response)

    def set_time(self, timestamp):
        """
        Sets the current date and time on the weather station
        :param timestamp: New date and time for the weather station
        :type timestamp: datetime.datetime
        """

        seconds = chr(timestamp.second)
        minutes = chr(timestamp.minute)
        hour = chr(timestamp.hour)
        day = chr(timestamp.day)
        month = chr(timestamp.month)
        year = chr(timestamp.year - 1900)

        data = seconds + minutes + hour + day + month + year
        data += struct.pack(CRC.FORMAT, CRC.calculate_crc(data))

        self._wake()

        # > SETTIME\n
        # < ACK
        # > data
        # < ACK

        self._port.write("SETTIME\n")

        response = self._port.read(1)

        if response != self._ACK:
            raise UnexpectedResponseError(response)

        self._port.write(data)

        response = self._port.read(1)

        if response != self._ACK:
            raise UnexpectedResponseError(response)

        # Done!

    def get_time_zone(self):
        """
        Gets the current timezone setting on the weather station.
        :returns: Timezone ID, timezone description
        :rtype: int,str
        """
        self._wake()

        self._port.write("EEBRD 11 1\n")
        response = self._port.read(4)  # ACK, 1 byte of data, 2 bytes of CRC

        if response[0] == self._ACK:
            data = response[1]

            packet_crc = struct.unpack(CRC.FORMAT, response[2:])[0]
            calculated_crc = CRC.calculate_crc(data)

            if packet_crc != calculated_crc:
                raise CrcError(packet_crc, calculated_crc)

            tz_id = ord(data)

            if tz_id < 0 or tz_id > len(self.time_zones):
                raise UnexpectedResponseError("Invalid timezone ID {0}".format(tz_id))

            return tz_id, self.time_zones[tz_id][2]

        raise UnexpectedResponseError(response)

    def set_time_zone(self, timezone_id):
        """
        Sets the station to the specified time zone ID.

        :param timezone_id: Time zone to change the station to
        :type timezone_id: int
        """

        if timezone_id < 0 or timezone_id > len(self.time_zones):
            raise Exception("Invalid time zone ID {0}".format(timezone_id))

        data = chr(timezone_id)

        data += struct.pack(CRC.FORMAT, CRC.calculate_crc(data))

        # > EEBWR 11 1
        # < ACK
        # > data
        # < ACK

        self._wake()

        # Set the time zone ID
        self._port.write("EEBWR 11 1\n")
        response = self._port.read(1)

        if response == self._ACK:
            # Console is ready for data
            self._port.write(data)

            response = self._port.read(1)

            if response != self._ACK:
                raise UnexpectedResponseError(response)

            # Set time zone mode (in case the user has entered a GMT offset
            # through WeatherLink or the stations setup screen)
            self._port.write("EEBWR 16 1\n")
            response = self._port.read(1)
            if response == self._ACK:
                data = chr(0)
                data += struct.pack(CRC.FORMAT, CRC.calculate_crc(data))

                self._port.write(data)

                response = self._port.read(1)

                if response != self._ACK:
                    raise UnexpectedResponseError(response)

                return

        raise UnexpectedResponseError(response)

    def get_auto_dst_enabled(self):
        """
        Gets whether automatic daylight savings is turned on or not
        :returns: True if automatic daylight savings is on
        :rtype: bool
        """
        self._wake()

        self._port.write("EEBRD 12 1\n")
        response = self._port.read(4)  # ACK, 1 byte of data, 2 bytes of CRC

        if response[0] == self._ACK:
            data = response[1]

            packet_crc = struct.unpack(CRC.FORMAT, response[2:])[0]
            calculated_crc = CRC.calculate_crc(data)

            if packet_crc != calculated_crc:
                raise CrcError(packet_crc, calculated_crc)

            result = ord(data)
            if result == 1:
                return False  # 1 = manual DST
            elif result == 0:
                return True  # 0 = automatic DST

            raise Exception("Invalid daylight savings mode {0}".format(result))

        raise UnexpectedResponseError(response)

    def set_auto_dst_enabled(self, is_enabled):
        """
        Turns automatic daylight savings on or off (manual daylight savings).

        :param is_enabled: True for automatic daylight savings, False for manual
                            daylight savings.
        :type is_enabled: bool
        """

        if is_enabled:
            data = chr(0)
        else:
            data = chr(1)

        crc = CRC.calculate_crc(data)

        packed_crc = struct.pack(CRC.FORMAT, crc)

        data += packed_crc

        # > EEBWR 12 1
        # < ACK
        # > data
        # < ACK

        self._wake()

        self._port.write("EEBWR 12 1\n")
        response = self._port.read(1)

        if response == self._ACK:
            # Console is ready for data
            self._port.write(data)

            response = self._port.read(1)

            if response == self._ACK:
                return

            else:
                raise UnexpectedResponseError(response)

        raise UnexpectedResponseError(response)

    def get_manual_dst_on(self):
        """
        Gets whether daylight savings is turned on or not (only used when
        automatic daylight savings is turned off)
        :returns: True if daylight savings is on
        :rtype: bool
        """
        self._wake()

        self._port.write("EEBRD 13 1\n")
        response = self._port.read(4)  # ACK, 1 byte of data, 2 bytes of CRC

        if response[0] == self._ACK:
            data = response[1]

            packet_crc = struct.unpack(CRC.FORMAT, response[2:])[0]
            calculated_crc = CRC.calculate_crc(data)

            if packet_crc != calculated_crc:
                raise CrcError(packet_crc, calculated_crc)

            result = ord(data)
            if result == 1:
                return True
            elif result == 0:
                return False

        raise UnexpectedResponseError(response)

    def set_manual_dst_on(self, is_on):
        """
        Turns daylight savings on or off when automatic daylight savings is
        turned off.
        :param is_on: If daylight savings is on or not
        :type is_on: bool
        """

        if is_on:
            data = chr(1)
        else:
            data = chr(0)

        crc = CRC.calculate_crc(data)

        packed_crc = struct.pack(CRC.FORMAT, crc)

        data += packed_crc

        # > EEBWR 13 1
        # < ACK
        # > data
        # < ACK

        self._wake()

        self._port.write("EEBWR 13 1\n")
        response = self._port.read(1)

        if response == self._ACK:
            # Console is ready for data
            self._port.write(data)

            response = self._port.read(1)

            if response == self._ACK:
                return

            else:
                raise UnexpectedResponseError(response)

        raise UnexpectedResponseError(response)


def main():
    """
    Entry point.
    """

    try:
        width = int(os.environ['COLUMNS'])
    except (KeyError, ValueError):
        width = 80
    width -= 2

    epilog = """
The following time zones are accepted by the --set-time-zone argument.

ID  Time Zone
--  """

    epilog += '-' * (width - 4) + '\n'

    for tz in DavisWeatherStation.time_zones:
        epilog += "{0:2}  {1}\n".format(tz[0], tz[2])

    parser = argparse.ArgumentParser(
        description='\n'.join(textwrap.wrap(
            "Tool for changing time settings on Davis weather stations."
            " When run without any options it reports the current "
            "settings.", width)),
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("serial_port", metavar='port', type=str,
                        help="Serial port the weather station is connected to.")
    parser.add_argument("--baud-rate", dest="baud_rate", type=int,
                        default=19200,
                        help="Serial port baud rate. Default is 19200.")

    parser.add_argument("--set-time-zone", dest="timezone", type=int,
                        choices=range(0, 47),
                        help="Changes the timezone to the specified timezone "
                             "ID. See the list below for valid timezone IDs.")

    parser.add_argument("--set-time", dest="time", type=str,
                        help="Sets the current station time. Format is "
                             "DD-MMM-YYYY HH:MM:SS. For example, "
                             "08-JAN-2015 07:57:00. To set the time to the "
                             "computers clock use the value 'NOW' instead.")

    group = parser.add_mutually_exclusive_group()
    group.add_argument("--set-automatic-dst-on", dest="automatic_dst",
                       action="store_true",
                       default=None,
                       help="Turns automatic daylight savings on. If you're in "
                            "North America (except Saskatchewan, Arizona, "
                            "Hawaii and the Mexican State of Sonora) or "
                            "Europe the weather station should be able to "
                            "manage daylight savings automatically.")
    group.add_argument("--set-automatic-dst-off", dest="automatic_dst",
                       action="store_false",
                       default=None,
                       help="Turns automatic daylight savings off. Use this if "
                            "you're in an area that doesn't observe daylight "
                            "savings or are located outside North "
                            "America/Europe. You can use --set-dst-on and "
                            "--set-dst-off to turn daylight savings on and off"
                            " manually.")

    group = parser.add_mutually_exclusive_group()
    group.add_argument("--set-dst-on", dest="dst", action="store_true",
                       default=None,
                       help="Turns daylight savings on. Only used if "
                            "automatic daylight savings is disabled.")
    group.add_argument("--set-dst-off", dest="dst", action="store_false",
                       default=None,
                       help="Turns daylight savings on. Only used if "
                            "automatic daylight savings is disabled.")

    args = parser.parse_args()

    ser = serial.Serial(args.serial_port, args.baud_rate, timeout=1.2)

    station = DavisWeatherStation(ser)

    hw_type_id, hw_type_str = station.hardware_type

    print("Console type: {0}".format(hw_type_str))

    if hw_type_id not in [16, 17]:
        print("Error: unsupported weather station model")
        exit(1)

    if args.timezone is not None or args.automatic_dst is not None or args.dst \
            is not None or args.time is not None:
        if args.timezone is not None:
            # Set the time zone
            tz_str = DavisWeatherStation.time_zones[args.timezone][2]
            print("Setting time zone to: {0}...".format(tz_str))

            station.set_time_zone(args.timezone)

        if args.automatic_dst is not None:
            # Set automatic DST
            if args.automatic_dst:
                print("Turning automatic daylight savings ON...")
            else:
                print("Turning automatic daylight savings OFF...")

            station.set_auto_dst_enabled(args.automatic_dst)
        if args.dst is not None:
            # Set DST on or off
            if args.dst:
                print("Turning daylight savings ON...")
            else:
                print("Turning daylight savings OFF...")
            station.set_manual_dst_on(args.dst)
        if args.time is not None:
            # Set the clock
            ts = None
            if args.time == 'NOW':
                ts = datetime.datetime.now()
                print("Setting station clock to computer time "
                      "({0})...".format(ts))
            else:
                try:
                    ts = datetime.datetime.strptime(args.time,
                                                    "%d-%b-%Y %H:%M:%S")
                except ValueError:
                    print("Invalid datetime string.")
                    exit(1)

                print("Setting station clock to: {0}...".format(ts))

            station.set_time(ts)

    print("\nCurrent settings:")

    current_time = station.get_time()
    print("Current time on weather station: {0}".format(current_time))

    tz_id, tz_str = station.get_time_zone()
    print("Current time zone: {0} - {1}".format(tz_id, tz_str))

    auto_dst = station.get_auto_dst_enabled()
    mode = "MANUAL"
    if auto_dst:
        mode = "AUTOMATIC"
    print("Daylight savings mode: {0}".format(mode))

    if not auto_dst:
        dst_on = station.get_manual_dst_on()
        print("Daylight savings is on: {0}".format(dst_on))

    exit(0)


if __name__ == "__main__":
    main()