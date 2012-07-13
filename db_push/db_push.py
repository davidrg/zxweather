# coding=utf-8
"""
An application for pushing database updates to a remote host running the
web interface.
"""
import json
from optparse import OptionParser
import psycopg2
import psycopg2.extensions
import requests
import select

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
        cur.execute(query + "where time_stamp > %s", (remote_max_ts,))
    else:
        print("Remote database is empty. Sending *all* samples.")
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
    :rtype: dict
    """

    url = site_url
    if not url.endswith("/"):
        url += "/"
    url += "ws/latest_sample"

    print url

    r = requests.get(url)

    if r.status_code != 200:
        raise Exception("Request failed: status code {0}".format(r.status_code))

    return r.json

def post_data(site_url, data):
    """
    Posts new data to the remote site
    :param site_url:
    :param data:
    :return:
    """

    url = site_url
    if not url.endswith("/"):
        url += "/"
    url += "ws/dataload"

    r = requests.post(url, data=json.dumps(data))

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

def update(cur, site_url, update_samples=True):
    """
    Performs an update
    :param cur: Database cursor
    :param site_url: Website URL
    :param update_samples: If samples should be updated or not
    :return:
    """

    live_data = get_live_data(cur)
    samples = []

    # Don't update samples if we know there is no new data locally. This
    # eliminates one round-trip to the remote host.
    if update_samples:
        sample_data_ts = get_current_timestamp(site_url)
        print("Updating from: {0}".format(sample_data_ts['max_ts']))
        samples = get_samples(cur, sample_data_ts['max_ts'])

    payload = {
        'v': 1,
        'ld_u': live_data,
        's': samples
    }

    print("Sending data...")
    response = post_data(site_url, payload)

    print("Response: {0}".format(response['stat']))
    print("Live Data Updated: {0}".format(response['ld_u']))
    print("Samples Inserted: {0}".format(response['sa_i']))

def listen_loop(con, cur, site_url):
    """
    Listens for update notifications from the database and sends the updates
    to the remote database.
    :param con: Database connection
    :param cur: Database cursor for queries
    :param site_url: Website URL
    """
    print ("Continuous mode")
    cur.execute("listen live_data_updated;")
    cur.execute("listen new_sample;")

    while True:
        if not (select.select([con],[],[],5) == ([],[],[])):
            update_live = False
            send_samples = False

            con.poll()
            while con.notifies:
                notify = con.notifies.pop()
                if notify.channel == "live_data_updated":
                    update_live = True
                elif notify.channel == "new_sample":
                    send_samples = True

            # If there is new data then send it.
            if update_live or send_samples:
                if not send_samples:
                    print("Only updating live data")
                update(cur, site_url, send_samples)

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
    Program entrypoint.
    :return:
    """

    options = parse_args()

    print("zxweather database replicator v1.0")
    print("\t(C) Copyright David Goodwin, 2012\n\n")

    con = connect_to_db(get_connection_string(options))
    cur = con.cursor()

    update(cur, options.site_url)

    if options.continuous:
        listen_loop(con, cur, options.site_url)


if __name__ == "__main__": main()