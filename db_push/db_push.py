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

    query = """select download_timestamp::varchar, invalid_Data,
    indoor_relative_humidity, indoor_temperature, relative_humidity,
    temperature, absolute_pressure, average_wind_speed, gust_wind_speed,
    wind_direction
from live_data"""

    cur.execute(query)

    result = cur.fetchone()

    live_data = {
        'ts': result[0], # download_timestamp
        'id': result[1], # invalid_data
        'ih': result[2], # indoor_relative_humidity
        'it': result[3], # indoor_temperature
        'h' : result[4], # relative_humidity
        't' : result[5], # temperature
        'p' : result[6], # absolute_pressure
        'wa': result[7], # average_wind_speed
        'wg': result[8], # gust_wind_speed
        'wd': result[9], # wind_direction
    }

    return live_data

def get_samples(cur, remote_max_ts):
    """
    Gets a list of samples that need to be sent to the remote database based
    on the max timestamp present in the remote database.
    :param cur: Cursor to use for database work
    :param remote_max_ts: Maximum sample timestamp in the remote database.
    :return: List of samples to send
    :rtype: list
    """

    query = """select sample_interval, record_number,
    download_timestamp::varchar, last_in_batch, invalid_data,
    time_stamp::varchar, indoor_relative_humidity,
    indoor_temperature, relative_humidity, temperature, absolute_pressure,
    average_wind_speed, gust_wind_speed, wind_direction, total_rain,
    rain_overflow
from sample
    """

    if remote_max_ts is not None:
        query += "where time_stamp > %s "
        query += "order by time_stamp asc"
        cur.execute(query, (remote_max_ts,))
    else:
        print("Remote database is empty. Sending *all* samples.")
        query += "order by time_stamp asc"
        cur.execute(query)

    result = cur.fetchall()

    samples = []

    for row in result:
        sample = {
            'si' : row[0],  # sample_interval
            'rn' : row[1],  # record_number
            'dts': row[2],  # download_timestamp
            'lib': row[3],  # last_in_batch
            'id' : row[4],  # invalid_data
            'ts' : row[5],  # time_stamp
            'ih' : row[6],  # indoor_relative_humidity
            'it' : row[7],  # indoor_temperature
            'h'  : row[8],  # relative_humidity
            't'  : row[9],  # temperature
            'p'  : row[10], # absolute_pressure
            'wa' : row[11], # average_wind_speed
            'wg' : row[12], # gust_wind_speed
            'wd' : row[13], # wind_direction
            'rt' : row[14], # total_rain
            'ro' : row[15], # rain_overflow
        }
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
    with open("signed-junk.txt",'w') as f:
        f.write(signed_data)

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

def build_payload(live_data, samples):
    """
    Builds the JSON data structure to be sent to the remote system.
    :param live_data: Live data to be sent
    :type live_data: dict or None
    :param samples: List of samples to send
    :type samples: list or None
    :return: data structure to send
    :rtype: dict
    """

    payload = {'v': 1}
    if live_data is not None:
        payload['ld_u'] = live_data
    if samples is not None:
        payload['s'] = samples

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

def sample_chunk_update(samples, site_url, gpg, key_id):
    """
    Sends samples (and only samples) in chunks of 500.
    :param samples: List of samples to send
    :param site_url: Site URL
    :param gpg: GPG object for signing data
    :param key_id: Signing key to use.
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
        payload = build_payload(None, chunk)
        result = send_data(site_url, payload, gpg, key_id)

        if not result:
            # Chunk failed to send.
            return



def update(cur, site_url, gpg, key_id, update_samples=True):
    """
    Performs an update
    :param cur: Database cursor
    :param site_url: Website URL
    :type site_url: str or Unicode
    :param gpg: GnuPG object.
    :param update_samples: If samples should be updated or not
    :param key_id: Key to sign with
    :type key_id: str
    :type update_samples: boolean
    :return:
    """

    live_data = get_live_data(cur)
    samples = []

    # Don't update samples if we know there is no new data locally. This
    # eliminates one round-trip to the remote host.
    if update_samples:
        sample_data_ts = get_current_timestamp(site_url)
        if sample_data_ts is None:
            return # Failed to get current timestamp.
        print("Updating from: {0}".format(sample_data_ts['max_ts']))
        samples = get_samples(cur, sample_data_ts['max_ts'])

        if len(samples) > 50:
            print("Performing chunked sample load...")
            # There is lots of samples (remote database might be empty).
            # We'll chop these up and send them 500 at a time.
            sample_chunk_update(samples, site_url, gpg, key_id)
            # All samples should have been sent to the remote database. We
            # don't want to send them again.
            samples = None


    payload = build_payload(live_data, samples)

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

    print("zxweather database replicator v1.0")
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

    if options.continuous:
        listen_loop(con, cur, options.site_url, gpg, key_id)


if __name__ == "__main__": main()