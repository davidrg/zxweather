#!/usr/bin/env python
# coding=utf-8
"""
This is an example program which subscribes to a live or sample weather data
feed from a remote zxweather server and writes the data to stdout for
processing by other scripts and programs (such as rrdtool).

The data is output one line per record in a tab-delimited format. Record
timestamps are in ISO8601 format by default. Supplying the --unix-time parameter
will give unix time instead.

Live data format:
  Received timestamp
  Temperature
  Dew point
  Apparent temperature
  wind chill
  relative humidity
  indoor temperature
  indoor relative humidity
  absolute pressure
  average wind speed
  wind direction

  Davis-specific fields:
    Barometer trend
    Current rain rate
    Storm rain total
    Storm rain start date
    Transmitter battery status
    Console battery voltage
    Forecast icon
    Forecast rule ID

Sample data format:
  Recorded timestamp
  Temperature
  Dew point
  Apparent temperature
  Wind chill
  Relative humidity
  Indoor temperature
  Indoor relative humidity
  Absolute pressure
  Average wind speed
  Gust wind speed
  Wind direction
  Rainfall
"""
import argparse
import json
import socket
import sys
import datetime
import time

__author__ = 'david'


client_name = "weather_feed"
client_version = "1"

STATE_INIT = 0
STATE_STATION_INFO = 1
STATE_STATION_INFO_RESPONSE = 2
STATE_SUBSCRIBE = 3
STATE_STREAMING = 4

HW_TYPE_GENERIC = 0
HW_TYPE_FOWH1080 = HW_TYPE_GENERIC
HW_TYPE_DAVIS = 1


def readLine(socket):
    """
    Buffers input from the socket and returns it a line at a time.

    :param socket: Network socket
    :type socket: socket.socket
    :return: A single line of text
    :rtype: str
    """

    input_buffer = socket.recv(4096)
    done = False

    while not done:
        if "\n" in input_buffer:
            (line, input_buffer) = input_buffer.split("\n", 1)
            yield line.strip()
        else:
            incoming_data = socket.recv(4096)
            if not incoming_data:
                done = True
            else:
                input_buffer = input_buffer + incoming_data
    if buffer:
        yield input_buffer.strip()


def sendNextCommand(socket, current_state, station_code, live_mode):
    """
    Sends the next command to the server

    :param socket: Network socket
    :type socket: socket.socket
    :param current_state: The current state we're in
    :type current_state: int
    :param station_code: The weather station we're working with
    :type station_code: str
    :param live_mode: True if we've been asked to receive live data, False for
                      samples.
    :type live_mode: bool
    :returns: The new state
    :rtype: int
    """

    if current_state == STATE_INIT:
        command = \
            'set client "{client_name}"/version="{client_version}"'.format(
                client_name=client_name, client_version=client_version)
        next_state = STATE_STATION_INFO
    elif current_state == STATE_STATION_INFO:
        command = 'show station "{station_code}"/json'.format(
            station_code=station_code)
        next_state = STATE_STATION_INFO_RESPONSE
    elif current_state == STATE_SUBSCRIBE:
        if live_mode:
            data_type = "live"
        else:
            data_type = "samples"

        command = 'subscribe "{station_code}"/{type}'.format(
            station_code=station_code, type=data_type)
        next_state = STATE_STREAMING
    else:
        raise Exception("Something that should never happen just happened. "
                        "oops.")

    #print(str(current_state) + '-' + str(next_state) + '<' + command + '\n')
    socket.sendall(command + '\n')
    return next_state


def processDataLine(line, hardware_type, live_mode, iso_time):
    """
    Processes a single line of weather data.
    :param line: Line of weather data
    :type line: str
    :param hardware_type: Type of weather station the data came from
    :type hardware_type: int
    :param live_mode: If we're receiving live data
    :type live_mode: bool
    :param iso_time: If we should output timestamps in ISO8601 format instead
                     of unix time
    :type iso_time: bool
    """
    if line == "" or line.startswith("#"):
        # Its just an informational message. We can ignore it.
        return

    parts = line.split(',')

    if live_mode:
        # We're going to be dealing with live data

        expectedLength = 11  # For HW_TYPE_GENERIC and HW_TYPE_FOWH1080

        if hardware_type == HW_TYPE_DAVIS:
            # The server sends some extra data for davis weather stations
            expectedLength = 19

        if len(parts) < expectedLength:
            sys.stderr.write("Warning: data line had {part_count} when at "
                             "least {expected} parts were expected. Data line "
                             "will be ignored: {line}"
                             .format(part_count=len(parts),
                                     expected=expectedLength,
                                     line=line))
            return

        if parts[0] != 'l':
            sys.stderr.write("Warning: expected live data (type 'l'), "
                             "got type '{type}'. Line will be ignored"
                             .format(type=parts[0]))
            return

        # Live data isn't sent with a timestamp. Replace the row type info with
        # a timestamp
        if iso_time:
            parts[0] = datetime.datetime.now().isoformat()
        else:
            parts[0] = str(int(time.mktime(
                datetime.datetime.now().timetuple())))
    else:
        expectedLength = 14
        if len(parts) < expectedLength:
            sys.stderr.write("Warning: data line had {part_count} when at "
                             "least {expected} parts were expected. Data line "
                             "will be ignored: {line}"
                             .format(part_count=len(parts),
                                     expected=expectedLength,
                                     line=line))
            return

        if parts[0] != 's':
            sys.stderr.write("Warning: expected sample data (type 's'), "
                             "got type '{type}'. Line will be ignored"
                             .format(type=parts[0]))
            return

        # Convert timestamp to ISO8601
        ts = datetime.datetime.strptime(parts[1], "%Y-%m-%d %H:%M:%S")

        if iso_time:
            parts[1] = ts.isoformat()
        else:
            parts[1] = str(int(time.mktime(ts.timetuple())))

        # Throw away the row type info.
        del parts[0]

    result = '\t'.join(parts)
    sys.stdout.write(result + '\n')


def main():
    """
    Program entry point
    """
    parser = argparse.ArgumentParser(
        description="Streams weather data from a remote zxweather server to "
                    "stdout for processing by local scripts")
    parser.add_argument('hostname', metavar='hostname', type=str,
                        help="Remote server hostname")
    parser.add_argument('port', metavar='port', type=int,
                        help="Raw TCP port the remote server is listening on")
    parser.add_argument('station_code', metavar='station', type=str,
                        help="Code for the weather station to subscribe to")
    parser.add_argument('--samples', action='store_true')
    parser.add_argument('--unix-time', action='store_true')

    args = parser.parse_args()

    live_mode = True
    if args.samples:
        # We've been asked to do samples instead
        live_mode = False

    iso_time = True
    if args.unix_time:
        iso_time = False

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((args.hostname, args.port))

    hw_type = 0
    state = STATE_INIT

    # Throw away any init stuff the server may have sent us
    sock.recv(4096)
    sock.sendall('\n')  # This should give us a command prompt

    for line in readLine(sock):
        #print('#' + str(state) + "--" + repr(line))
        if line == '':
            continue

        if line == "_ok" or state == STATE_INIT:
            # We're at the command prompt. Send the next command.
            state = sendNextCommand(sock, state, args.station_code, live_mode)
        elif state == STATE_STATION_INFO_RESPONSE:
            station_info = json.loads(line)
            hw_type_code = station_info['hardware_type_code']
            if hw_type_code == 'FOWH1080':
                hw_type = HW_TYPE_FOWH1080
            elif hw_type_code == 'GENERIC':
                hw_type = HW_TYPE_GENERIC
            elif hw_type_code == 'DAVIS':
                hw_type = HW_TYPE_DAVIS
            else:
                sys.stderr.write(
                    'Warning: unrecognised station type "{station_type}. '
                    'Treating as GENERIC.'.format(
                        station_type=hw_type_code))

            state = STATE_SUBSCRIBE
        elif state == STATE_STREAMING:
            processDataLine(line, hw_type, live_mode, iso_time)

if __name__ == "__main__":
    main()