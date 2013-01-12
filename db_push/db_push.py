# coding=utf-8
"""
An application for pushing database updates to a remote host running the
web interface.
"""
from __future__ import print_function
import json
from optparse import OptionParser
import psycopg2
import psycopg2.extensions
import requests
import select
import gnupg

# Required packages:
#  - requests
#  - python-gnupg

__author__ = 'David Goodwin'

def get_live_data(cur):
    """
    Gets live data from the database.
    :param cur: Database cursor to perform the select with.
    :return: Dict containing live data.
    """

    query = """select ld.download_timestamp::varchar,
       ld.indoor_relative_humidity,
       ld.indoor_temperature,
       ld.relative_humidity,
       ld.temperature,
       ld.absolute_pressure,
       ld.average_wind_speed,
       ld.gust_wind_speed,
       ld.wind_direction,
       s.code
from live_data ld
inner join station s on s.station_id = ld.station_id"""

    cur.execute(query)
    result = cur.fetchall()

    live_data = []

    for row in result:
        ld = {
            'ts': row[0], # download_timestamp
            'ih': row[1], # indoor_relative_humidity
            'it': row[2], # indoor_temperature
            'h' : row[3], # relative_humidity
            't' : row[4], # temperature
            'p' : row[5], # absolute_pressure
            'wa': row[6], # average_wind_speed
            'wg': row[7], # gust_wind_speed
            'wd': row[8], # wind_direction
            'sc': row[9], # station code
        }

        live_data.append(ld)

    return live_data

def get_station_id(cur, station_code):
    """
    Gets the ID for the specified station code
    :param cur: Database cursor
    :param station_code: Station code to look up
    :type station_code: str or unicode
    :return: Station ID
    :rtype: int or None
    """

    cur.execute("select station_id from station where code = %s",
                (station_code,))

    result = cur.fetchall()

    if len(result) > 0:
        return result[0][0]
    else:
        return None

def get_station_type(cur, station_id):
    """
    Gets the type code for the specified station
    :param cur: Database cursor
    :param station_id: Station
    :type station_id: int or None
    :return: Station hardware type code
    :rtype: str, unicode or None
    """

    if station_id is None:
        return None

    cur.execute("""select st.code
from station s
inner join station_type st on st.station_type_id = s.station_type_id
where s.station_id = %s""",
                (station_id,))

    result = cur.fetchall()

    if len(result) > 0:
        return result[0][0]
    else:
        return None


def get_samples(cur, remote_max_ts):
    """
    Gets a list of samples that need to be sent to the remote database based
    on the max timestamp present in the remote database.
    :param cur: Cursor to use for database work
    :param remote_max_ts: Maximum sample timestamp in the remote database.
    :return: List of samples to send
    :rtype: list
    """

    base_query = """
select s.download_timestamp::varchar,
       s.time_stamp::varchar,
       s.indoor_relative_humidity,
       s.indoor_temperature,
       s.relative_humidity,
       s.temperature,
       s.absolute_pressure,
       s.average_wind_speed,
       s.gust_wind_speed,
       s.wind_direction,
       s.rainfall{hw_columns}
from sample s
where station_id = %s
    """

    wh1080_cols = """,

       wh1080.sample_interval,
       wh1080.record_number,
       wh1080.last_in_batch,
       wh1080.invalid_data,
       wh1080.total_rain,
       wh1080.rain_overflow
    """
    wh1080_tables = "left outer join wh1080_sample wh1080 on wh1080.sample_id = s.sample_id"

    samples = []

    if len(remote_max_ts) == 0:
        print("Unable to send samples: no stations configured on remote system.")
        return

    for rmt in remote_max_ts:
        max_ts = rmt['max_ts']
        station_code = rmt['sc']
        station_id = get_station_id(cur,station_code)
        if station_id is None:
            print("WARNING: Remote weather station {0} not found in local "
                  "database. Ignoring.".format(station_code))
            continue

        hardware_type = get_station_type(cur, station_id)

        if hardware_type == "FOWH1080":
            hardware_columns = wh1080_cols
            hardware_tables = wh1080_tables
        elif hardware_type == "GENERIC":
            # 'GENERIC' type has no special tables.
            hardware_columns = ""
            hardware_tables = ""
        else:
            # Its not any station type we know about so we'll just sync the
            # standard set of sample data and issue a warning.
            print("WARNING: Unrecognised station hardware type. Syncing basic "
                  "sample data only.")
            hardware_columns = ""
            hardware_tables = ""

        query = base_query.format(hw_columns=hardware_columns,
                                  hw_tables=hardware_tables)


        if max_ts is not None:
            print("Updating from {0} for station {1}".format(max_ts, station_code))

            query += "and time_stamp > %s "
            query += "order by time_stamp asc"
            cur.execute(query, (station_id, max_ts,))
        else:
            print("Remote database is empty for station {1}. Sending *all* "
                  "samples.".format(station_code))
            query = base_query + "order by time_stamp asc"
            cur.execute(query, (station_id,))

        result = cur.fetchall()

        for row in result:
            # Standard sample data:
            sample = {
                'dts': row[0],  # download_timestamp
                'ts' : row[1],  # time_stamp
                'ih' : row[2],  # indoor_relative_humidity
                'it' : row[3],  # indoor_temperature
                'h'  : row[4],  # relative_humidity
                't'  : row[5],  # temperature
                'p'  : row[6],  # absolute_pressure
                'wa' : row[7],  # average_wind_speed
                'wg' : row[8],  # gust_wind_speed
                'wd' : row[9],  # wind_direction
                'r'  : row[10], # Rainfall
                'sc' : station_code,    # Station Code
            }

            if hardware_type == 'FOWH1080':
                # Handle data specific to hardware compatible with the
                # Fine Offset WH1080.

                sample['si'] = row[11]  # sample_interval (wh1080)
                sample['rn'] = row[12]  # record_number (wh1080)
                sample['lib']= row[13]  # last_in_batch (wh1080)
                sample['id'] = row[14]  # invalid_data (wh1080)
                sample['rt'] = row[15]  # total_rain (wh1080)
                sample['ro'] = row[16]  # rain_overflow (wh1080)

            samples.append(sample)

    return samples

def get_current_timestamp(site_url):
    """
    Gets the most recent sample timestamp in the remote database.
    :param site_url: URL of remote site
    :return: Timestamp data
    :rtype: dict or None
    """

    url = site_url
    if not url.endswith("/"):
        url += "/"
    url += "ws/latest_sample"

    try:
        r = requests.get(url)
    except Exception as ex:
        print("Request failed: {0}".format(ex.message))
        return None

    if r.status_code != 200:
        print("Request failed: status code {0}".format(r.status_code))

    return r.json

def post_data(site_url, data, gpg, key_id):
    """
    Posts new data to the remote site
    :param site_url: URL to send data to
    :param data: Data to send
    :param gpg: GnuPG object to use for signing
    :param key_id: Key to sign with
    :return: response JSON data.
    """

    url = site_url
    if not url.endswith("/"):
        url += "/"
    url += "ws/dataload"

    # Sign the data we are sending
    signed_data = str(gpg.sign(json.dumps(data), keyid=key_id))

    #print(signed_data)
#    with open("signed-junk.txt",'w') as f:
#        f.write(signed_data)

    r = requests.post(url, data=signed_data)

    return r.json

def parse_args():
    """
    Parses args.
    :return:
    """

    # Configure and run the option parser
    parser = OptionParser()
    parser.add_option("-t", "--database",dest="dbname",
                      help="Database name")
    parser.add_option("-n", "--host", dest="hostname",
                      help="PostgreSQL Server Hostname")
    parser.add_option("-u","--user",dest="username",
                      help="PostgreSQL Username")
    parser.add_option("-p","--password",dest="password",
                      help="PostgreSQL Password")
    parser.add_option("-s", "--site", dest="site_url",
                      help="Remote site URL (eg, http://example.com/)")
    parser.add_option("-c", "--continuous", action="store_true",
                      dest="continuous", default=False,
                      help="Stay running sending updates to the remote " \
                           "database as they appear in the local one.")
    parser.add_option("-d", "--gpg-home-directory", dest="gpg_home",
                      help="GnuPG Home Directory for db_push tool")
    parser.add_option("-b","--gpg-binary",dest="gpg_binary",
                      help="Name of GnuPG Binary")
    parser.add_option("-k","--key-id",dest="key_id",
                      help="Fingerprint of the key to sign with")
#    parser.add_option("-r","--reconfigure-stations", action="store_true",
#                        dest="send_stations", default=False,
#                        help="Add station data to remote database on first "
#                             "update.")

    (options, args) = parser.parse_args()

    return options

def connect_to_db(connection_string):
    """
    Connects to the database returning the connection object.
    :param connection_string: Command-line options
    :return: Database connection object
    """

    print("Connecting to database...")


    con = psycopg2.connect(connection_string)

    # We shouldn't be doing any committing - this is just so notifications
    # work properly.
    con.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    cur = con.cursor()

    cur.execute("select version()")
    data = cur.fetchone()
    print("Server version: {0}".format(data[0]))
    cur.close()

    return con

def build_payload(live_data, samples, station_info=None):
    """
    Builds the JSON data structure to be sent to the remote system.
    :param live_data: Live data to be sent
    :type live_data: list or None
    :param samples: List of samples to send
    :type samples: list or None
    :param station_info: Station data to send to remote system
    :type station_info: list or None
    :return: data structure to send
    :rtype: dict
    """

    payload = {'v': 2}
    if live_data is not None:
        payload['ld_u'] = live_data
    if samples is not None:
        payload['s'] = samples
    if station_info is not None:
        payload['si'] = station_info

    return payload

def send_data(site_url, payload, gpg, key_id):
    """
    Sends data and handles the response.
    :param site_url: URL to send it to
    :param payload: Payload to send
    :param gpg: GPG object for signing
    :param key_id: Key to sign with
    :returns: success or failure
    :rtype: bool
    """

    print("Sending data...")
    try:
        response = post_data(site_url, payload, gpg, key_id)
    except Exception as ex:
        print("Failed to post new data: {0}".format(ex.message))
        return False

    if response is None:
        print("ERROR: Bad response.")
        return False
    else:
        print("Response: {0}".format(response['stat']))
        print("Live Data Updated: {0}".format(response['ld_u']))
        print("Samples Inserted: {0}".format(response['sa_i']))

        if response['stat'] == "OK":
            return True
        else:
            return False

def sample_chunk_update(samples, site_url, gpg, key_id, station_info):
    """
    Sends samples (and only samples) in chunks of 500.
    :param samples: List of samples to send
    :param site_url: Site URL
    :param gpg: GPG object for signing data
    :param key_id: Signing key to use.
    :param station_info: Any station info to send in the first chunk
    :type station_info: list or None
    """

    def chunks(lst):
        """
        Iterate over a list in chunks of 500
        :param lst: List to iterate over.
        """
        for i in xrange(0, len(lst), 50):
            yield lst[i:i+50]

    chunk_count = len(samples) / 50

    i = 0
    for chunk in chunks(samples):
        print("Chunk {0}/{1}".format(i, chunk_count))
        i += 1
        payload = build_payload(None, chunk, station_info)
        result = send_data(site_url, payload, gpg, key_id)

        station_info = None

        if not result:
            # Chunk failed to send.
            return

def get_station_info(cur):
    """
    Gets data for all stations in the database.
    :param cur: Database cursor
    :return: A list of all stations in the database
    :rtype: list
    """

    query = """select s.code,
       s.title,
       s.description,
       st.code as "station_type_code",
       s.sample_interval
from station s
inner join station_type st on st.station_type_id = s.station_type_id"""


    cur.execute(query)
    rows = cur.fetchall()

    stations = []

    for row in rows:
        station = {
            'sc': row[0],
            'ti': row[1],
            'de': row[2],
            'tc': row[3],
            'si': row[4]
        }
        stations.append(station)

    return stations

def update(cur, site_url, gpg, key_id, update_samples=True, send_station_info=False):
    """
    Performs an update
    :param cur: Database cursor
    :param site_url: Website URL
    :type site_url: str or Unicode
    :param gpg: GnuPG object.
    :param update_samples: If samples should be updated or not
    :type update_samples: boolean
    :param key_id: Key to sign with
    :type key_id: str
    :param send_station_info: If station information should be sent.
    :type send_station_info: boolean
    """

    live_data = get_live_data(cur)
    samples = []
    station_info = None

    if send_station_info:
        print("Sending station data in this update.")
        station_info = get_station_info(cur)

    # Don't update samples if we know there is no new data locally. This
    # eliminates one round-trip to the remote host.
    if update_samples:
        sample_data_ts = get_current_timestamp(site_url)
        if sample_data_ts is None:
            return # Failed to get current timestamp.

        samples = get_samples(cur, sample_data_ts['mts'])

        if len(samples) > 50:
            print("Performing chunked sample load...")
            # There is lots of samples (remote database might be empty).
            # We'll chop these up and send them 500 at a time.
            sample_chunk_update(samples, site_url, gpg, key_id, station_info)
            # All samples should have been sent to the remote database. We
            # don't want to send them again.
            samples = None

    payload = build_payload(live_data, samples, station_info)
    send_data(site_url, payload, gpg, key_id)

def listen_loop(con, cur, site_url, gpg, key_id):
    """
    Listens for update notifications from the database and sends the updates
    to the remote database.
    :param con: Database connection
    :param cur: Database cursor for queries
    :param site_url: Website URL
    :type site_url: str or Unicode
    :param gpg: GnuPG object
    :param key_id: Key to sign with
    :type key_id: str
    """
    print ("Continuous mode. Waiting for new data...")
    cur.execute("listen new_sample;")
    cur.execute("listen update_complete;")

    send_samples = False

    while True:
        print('.',end="")
        if not (select.select([con],[],[],5) == ([],[],[])):
            print("")
            con.poll()
            while con.notifies:
                notify = con.notifies.pop()
                if notify.channel == "new_sample":
                    send_samples = True
                elif notify.channel == "update_complete":
                    # The update service has finished making changes. We can
                    # send data now.
                    if not send_samples:
                        print("Only updating live data")
                    update(cur, site_url, gpg, key_id, send_samples)
                    send_samples = False

def get_connection_string(options):
    """
    Gets the database connection string.
    :param options: Command-line options
    :return: Database connection string
    :rtype: str
    """
    connection_string = "host=" + options.hostname
    connection_string += " user=" + options.username
    connection_string += " password=" + options.password
    connection_string += " dbname=" + options.dbname

    return connection_string


def main():
    """
    Program entry point.
    :return:
    """

    options = parse_args()

    print("zxweather database replicator v2.0")
    print("\t(C) Copyright David Goodwin, 2012\n\n")

    con = connect_to_db(get_connection_string(options))
    cur = con.cursor()

    if options.gpg_binary is not None:
        gpg = gnupg.GPG(gnupghome=options.gpg_home, gpgbinary=options.gpg_binary)
    else:
        gpg = gnupg.GPG(gnupghome=options.gpg_home)

    key_id = options.key_id

    print("Performing update...")
    update(cur, options.site_url, gpg, key_id)

    # Not going to allow sending station config. This tool is about to go away
    # so there isn't much point putting the time required into testing that
    # this functionality works properly.
    #,
#           send_station_info=options.send_stations)

    if options.continuous:
        listen_loop(con, cur, options.site_url, gpg, key_id)


if __name__ == "__main__": main()