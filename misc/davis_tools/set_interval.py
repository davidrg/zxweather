# coding=utf-8
"""
A tool for getting and setting the stations logging interval
"""
import argparse
import serial

__author__ = 'david'

ACK = '\x06'

def wake_console(port):
    """
    Wakes the console. Exits with code 1 if wake fails.
    :param port: Serial port
    """
    retries = 3
    while retries > 0:

        retries -= 1
        port.write('\n')
        result = port.read(2)

        if result == '\n\r':
            break

        if retries == 0:
            print("Failed to wake weather station console")
            exit(1)


def check_hardware_type(port):
    """
    Checks the hardware type is supported (Currently vantage Pro2 and Vantage
    Vue). Exits with code 2 if unsupported.
    :param port: Serial port
    """
    port.write('WRD\x12\x4D\n')
    result = port.read(2)

    if result[0] == ACK:
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
        print("Console type: {0}".format(hw_type))

        if station_type not in [16,17]:
            print("Error: unsupported weather station model")
            exit(2)


def get_archive_interval(port):
    """
    Prints the current archive interval to the console.
    :param port: Serial port
    """
    # Get archive interval/period
    port.write("EEBRD 2D 01\n")
    result = port.read(4)
    if result[0] == ACK:
        interval = ord(result[1])
        print("Current archive interval: {0} minutes".format(interval))


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


def clear_archive_memory(port):
    """
    Clears the consoles archive memory. Exits with code 3 on unexpected
    response.
    :param port: Serial port
    :return:
    """
    port.write("CLRLOG\n")
    result = port.read(1)
    if result == ACK:
        print("Archive memory cleared.")
    else:
        print("Unexpected command response:")
        print toHexString(result)
        exit(3)


def set_archive_interval(port, interval):
    """
    Sets the archive interval to the specified value. On unexpected response
    exits with code 4.
    :param port: Serial port
    :param interval: New archive interval
    """
    port.write("SETPER {0}\n".format(interval))
    result = port.read(6)

    # The manual says to expect an ACK but we know better...
    if result == "\n\rOK\n\r":
        print("Archive interval successfully set to {0} minutes.".format(
            interval))

        # Clear the stations archive memory so we don't upset WeatherLink.
        clear_archive_memory(port)
    else:
        print("Unexpected command response:")
        print toHexString(result)
        exit(4)


def main():
    """
    Entry point.
    """
    parser = argparse.ArgumentParser(
        description="Tool for setting interval on Davis weather stations. If "
                    "run without specifying an interval it will display the"
                    " current setting on the weather station.")
    parser.add_argument("serial_port", metavar='port', type=str,
                        help="Serial port the weather station is connected to")
    parser.add_argument("--baud-rate", dest="baud_rate", type=int,
                        default=19200,
                        help="Serial port baud rate. Default is 19200.")
    parser.add_argument("--set-interval", dest="interval", type=int,
                        choices=[1, 5, 10, 15, 30, 60, 120],
                        help="New logging interval in minutes. The recommended"
                             " setting for zxweather is 5. WARNING: using this"
                             " option will clear the weather stations archive"
                             " memory.")
    args = parser.parse_args()

    ser = serial.Serial(args.serial_port, args.baud_rate, timeout=1.2)

    wake_console(ser)
    check_hardware_type(ser)

    if args.interval is None:
        get_archive_interval(ser)
    else:
        set_archive_interval(ser, args.interval)


if __name__ == "__main__":
    main()