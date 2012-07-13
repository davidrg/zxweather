# coding=utf-8
"""
An application for pushing database updates to a remote host running the
web interface.
"""
import json
from optparse import OptionParser
import psycopg2
import requests

__author__ = 'David Goodwin'






# Structure:
# {'v': 1,
#  'ld_u': {... live data ...},
#  's': [... samples ...}}

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
            'si' : row[0],
            'rn' : row[1],
            'dts': row[2],
            'lib': row[3],
            'id' : row[4],
            'ts' : row[5],
            'ih' : row[6],
            'it' : row[7],
            'h'  : row[8],
            't'  : row[9],
            'p'  : row[10],
            'wa' : row[11],
            'wg' : row[12],
            'wd' : row[13],
            'rt' : row[14],
            'ro' : row[15],
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

def main():
    """
    Program entrypoint.
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

    (options, args) = parser.parse_args()

    print("zxweather database replicator v1.0")
    print("\t(C) Copyright David Goodwin, 2012\n\n")

    print("Connecting to database...")
    connection_string = "host=" + options.hostname
    connection_string += " user=" + options.username
    connection_string += " password=" + options.password
    connection_string += " dbname=" + options.dbname

    con = psycopg2.connect(connection_string)

    cur = con.cursor()

    cur.execute("select version()")
    data = cur.fetchone()
    print("Server version: {0}".format(data[0]))

    sample_data_ts = get_current_timestamp(options.site_url)

    print sample_data_ts

    payload = {
        'v': 1,
        'ld_u': get_live_data(cur),
        's': get_samples(cur, sample_data_ts['max_ts'])
    }

    #print payload

    response = post_data(options.site_url, payload)
    print response

if __name__ == "__main__": main()