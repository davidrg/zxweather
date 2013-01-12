# coding=utf-8
"""
Various web services
"""
import json
import web
from config import db
import gnupg
from database import get_station_id, get_station_type_code, get_station_type_id

__author__ = 'David Goodwin'

class data_load():
    """
    For loading data into the database. This is very basic master-slave
    replication for the sample and live_data tables in the zxweather
    database. It allows the web interface to be run on a remote system that
    does not have access to the main weather database. In this case the web
    interface has its own database with data being pushed into it through
    these routes.
    """

    def GET(self):
        """
        Return an error (you're supposed to POST, not GET)
        :return: Some JSON data.
        """

        response = json.dumps({'ld_u': False,
                               'sa_i': 0,
                               'stat': "ERROR: Invaild HTTP Method: GET"})
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(response)))
        return response

    @staticmethod
    def _update_station_info(station_info):
        """
        Updates information for the specified station id.
        :param station_info: Station configuration data.
        :type station_info: dict
        """

        station_id = get_station_id(station_info['sc'])

        query = """
        update station set title=$title, description=$description
        where station_id = $station
        """

        params = dict(title=station_info['ti'],
                      description=station_info['de'],
                      station=station_id)

        db.query(query, params)

    @staticmethod
    def _insert_station(station_info):

        station_id = get_station_id(station_info['sc'])

        # If the station already exists just update its info rather than
        # trying to insert it (as that will fail)
        if station_id is not None:
            data_load._update_station_info(station_info)
        else:

            station_type_id = get_station_type_id(station_info['tc'])

            # Do we support this hardware type code?
            if station_type_id is None:
                error_message = "ERROR: Unknown/unsupported station hardware "\
                                "type '{0}'. Likely caused by client/server "\
                                "version mismatch or customised "\
                                "(non-official) client code."\
                .format(station_info['tc'])
                raise Exception(error_message)

            query = """
            insert into station(code, title, description, station_type_id,
                                sample_interval)
                        values($code, $title, $description, $station_type,
                               $sample_interval)
            """

            params = dict(code = station_info['sc'],
                          title=station_info['ti'],
                          description=station_info['de'],
                          station_type=station_type_id,
                          sample_interval=station_info['si'])

            db.query(query, params)

    @staticmethod
    def _insert_stations(station_data):
        """
        Inserts or updates stations using the supplied data.
        :param station_data: New station data
        :type station_data: dict
        """
        for station in station_data:
            data_load._insert_station(station)

    @staticmethod
    def _update_live_data_record(live_data, station_id):
        """
        Updates live data for the specified station.
        :param live_data: Live data
        :param station_id: The station the live data is for
        :type station_id: int
        :return:
        """

        live_update_query = """update live_data
                set download_timestamp = $download_timestamp,
                    indoor_relative_humidity = $indoor_rh,
                    indoor_temperature = $indoor_temp,
                    relative_humidity = $rh,
                    temperature = $temp,
                    absolute_pressure = $pressure,
                    average_wind_speed = $avg_wind_speed,
                    gust_wind_speed = $gust_wind_speed,
                    wind_direction = $wind_dir
                where station_id = $station
                """

        params = dict(
            download_timestamp = live_data['ts'],
            invalid_data = live_data['id'],
            indoor_rh = live_data['ih'],
            indoor_temp = live_data['it'],
            rh = live_data['h'],
            temp = live_data['t'],
            pressure = live_data['p'],
            avg_wind_speed = live_data['wa'],
            gust_wind_speed = live_data['wg'],
            wind_dir = live_data['wd'],
            station = station_id
        )

        db.query(live_update_query, params)

    @staticmethod
    def _update_live_data(live_data):
        """
        Updates the live data in the database.
        :param live_data: New live data
        :return: True if successful
        :type: boolean
        """

        for ld in live_data:
            station_id = get_station_id(ld['sc'])

            if station_id is None:
                error_message = "ERROR: Unknown station code '{0}'. Re-send " \
                                "station configuration data. " \
                                "Consult Installation Referece for more " \
                                "details.".format(ld['sc'])
                raise Exception(error_message)

            data_load._update_live_data_record(ld, station_id)

        return True

    @staticmethod
    def _insert_sample(sample_data, station_id):
        """
        Inserts a single sample returning its sample ID.
        :param sample_data: Sample data to insert
        :type sample_data: dict
        :param station_id: The station this sample is for
        :type station_id: int
        :return: The new sample ID.
        :rtype: int
        """

        sample_insert_query = """
        insert into sample(download_timestamp, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, station_id)
        values($download_timestamp, $time_stamp,
            $indoor_relative_humidity, $indoor_temperature, $relative_humidity,
            $temperature, $absolute_pressure, $average_wind_speed,
            $gust_wind_speed, $wind_direction, $station_id)
        returning sample_id
        """

        params = dict(
            download_timestamp = sample_data['dts'],
            time_stamp = sample_data['ts'],
            indoor_relative_humidity = sample_data['ih'],
            indoor_temperature = sample_data['it'],
            relative_humidity = sample_data['h'],
            temperature = sample_data['t'],
            absolute_pressure = sample_data['p'],
            average_wind_speed = sample_data['wa'],
            gust_wind_speed = sample_data['wg'],
            wind_direction = sample_data['wd'],
            station_id = station_id
        )

        result = db.query(sample_insert_query, params)[0]

        return result.sample_id

    @staticmethod
    def _insert_wh1080_sample(wh1080_sample_data, sample_id):
        """
        Inserts sample data specific to WH1080-compatible hardware (into
        the wh1080_sample table).

        :param wh1080_sample_data: The sample data to insert
        :type wh1080_sample_data: dict
        :param sample_id: The sample this data belongs to
        :type sample_id: int
        """

        sample_insert_query = """
        insert into wh1080_sample(sample_id, sample_interval, record_number,
            last_in_batch, invalid_data, total_rain, rain_overflow)
        values($sample_id, $sample_interval, $record_number,
               $last_in_batch, $invalid_data, $total_rain, $rain_overflow)"""

        params = dict(
            sample_id = sample_id,
            sample_interval = wh1080_sample_data['si'],
            record_number = wh1080_sample_data['rn'],
            last_in_batch = wh1080_sample_data['lib'],
            invalid_data = wh1080_sample_data['id'],
            total_rain = wh1080_sample_data['rt'],
            rain_overflow = wh1080_sample_data['ro']
        )

        db.query(sample_insert_query, params)

    @staticmethod
    def _insert_samples(samples):
        """
        Inserts new samples into the database.
        :param samples: List of samples to insert.
        :type samples: list
        :return: Number of samples inserted
        :rtype: int
        """

        for sample in samples:
            # Insert each sample into the database
            station_id = get_station_id(sample['sc'])

            if station_id is None:
                error_message = "ERROR: Unknown station code '{0}'. Re-send "\
                                "station configuration data. "\
                                "Consult Installation Referece for more "\
                                "details.".format(sample['sc'])
                raise Exception(error_message)

            sample_id = data_load._insert_sample(sample, station_id)

            # Insert any hardware-specific data
            type_code = get_station_type_code(station_id)

            if type_code == 'FOWH1080':
                data_load._insert_wh1080_sample(sample, sample_id)

        return len(samples)

    @staticmethod
    def _create_gpg():
        """
        Creates a new GnuPG object.
        :return: A new GPG object.
        """

        from config import gpg_binary, gnupg_home

        if gpg_binary is not None:
            gpg = gnupg.GPG(gnupghome=gnupg_home, gpgbinary=gpg_binary)
        else:
            gpg = gnupg.GPG(gnupghome=gnupg_home)

        return gpg

    @staticmethod
    def _strip_pgp_garbage(data):
        """
        Strips the PGP signature garbage returning only the clear-text message.

        It is only intended to work with the output of db_push.py - it
        may not be generally useful or work with other PGP-signed stuff.

        See RFC-2440 for more details about this.

        :param data: Input data
        :type data: str
        :return: Clear-text message
        :rtype: str
        """

        lines = data.splitlines()

        output = ''
#        headers = []

        in_message_block = False
        in_message = False

        for line in lines:
            if line.startswith("-----BEGIN PGP SIGNED MESSAGE"):
                in_message_block = True # We're in the message bit now.
            elif line.startswith("-----BEGIN PGP SIGNATURE"):
                # We've hit the end of the message and come across the
                # signature data.
                break
            elif in_message_block:
                # We're in the message block.
                if line == '' and not in_message:
                    in_message = True # We've found the message
                elif in_message:
                    # We're in the message body. Start copying.
                    if line.startswith("- "):
                        # Its dash-escaped.
                        line = line[2:]

                    if output != '':
                        output += '\n'
                    output += line
#                else:
#                    # Its a header
#                    headers.append(line)
        #print headers
        return output

    @staticmethod
    def _fetch_and_verify_data():
        """
        Fetches the POSTed data and verifies its signature.
        :return: error string, json data
        :rtype: string, dict
        """
        from config import key_fingerprint

        post_data = web.data()

        gpg = data_load._create_gpg()

        verified = gpg.verify(str(post_data))

#        print "Username: " + str(verified.username)
#        print "Status: " + str(verified.status)
#        print "Pubkey_Fingerprint: " + str(verified.pubkey_fingerprint)
#        print "Timestamp: " + str(verified.timestamp)
#        print "Creation_Date: " + str(verified.creation_date)
#        print "Expire_Timestamp: " + str(verified.expire_timestamp)
#        print "Valid: " + str(verified.valid)
#        print "Fingerprint: " + str(verified.fingerprint)
#        print "Key_ID: " + str(verified.key_id)
#        print "Signature_ID: " + str(verified.signature_id)
#        print "Sig_Timestamp: " + str(verified.sig_timestamp)
#        print "Data: " + str(verified.data)
#        print "stderr: \n_______________\n" + str(verified.stderr) + "_______________"

        if not verified:
            # Bad or missing signature
            return "ERROR: Verification failed - bad or missing signature. " \
                   "Status: {0}".format(verified.status), None
        elif verified.pubkey_fingerprint != key_fingerprint and \
             key_fingerprint is not None:
            # Signature was valid but the data was signed with the wrong key.
            return "ERROR: Invalid key fingerprint.", None

        # Signature checks out. Strip away the PGP stuff and continue to
        # load the data.
        post_data = data_load._strip_pgp_garbage(post_data)

        try:
            data = json.loads(post_data)
            return None, data
        except Exception as ex:
            return "ERROR: {0}".format(ex.message), None

    @staticmethod
    def _load_data(data):
        """
        Updates live data and loads samples into the database.
        :param data:
        :return: error message, if live data was updated, number of samples inserted
        :rtype: str, bool, int
        """
        live_updated = False
        samples_inserted = 0

        if data['v'] == 2:
            # Data structure version 2

            t = db.transaction()

            try :
                # Station Information
                if 'si' in data:
                    data_load._insert_stations(data['si'])

                # Live Data
                if 'ld_u' in data:
                    live_updated = data_load._update_live_data(data['ld_u'])

                # Samples
                if 's' in data:
                    samples_inserted = data_load._insert_samples(data['s'])

            except Exception as e:
                t.rollback()
                error = e.message
            else:
                t.commit()
                error = "OK"
        elif data['v'] == 1:
            error = 'ERROR: API v1 (zxweather v0.1) is unsupported. Please upgrade to a newer version of db_push.py.'
        else:
            error = 'ERROR: Unsupported structure version'

        return error, live_updated, samples_inserted

    def POST(self):
        """
        Receives JSON-encoded data and loads it into the database
        :return: status message in JSON format.
        """

        from config import enable_data_loading

        live_updated = False
        samples_inserted = 0

        if enable_data_loading:
            error, data = self._fetch_and_verify_data()

            if error is None and data is not None:
                # We have data and there was no error message
                error, live_updated, samples_inserted = self._load_data(data)
        else:
            error = "ERROR: data loading is disabled"

        response = json.dumps({'ld_u': live_updated,
                               'sa_i': samples_inserted,
                               'stat': error})
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(response)))
        return response


class latest_sample():
    """
    Gets the age of the latest sample. Used for data replication.
    """

    def GET(self):
        """
        Gets some JSON data containing the age of the most recent sample.
        :return: Age of the most recent sample, JSON encoded.
        """

        result = db.query("""select st.code as "code",
       max(s.time_stamp)::Varchar as "max_ts"
from station st
left outer join sample s on s.station_id = st.station_id
group by st.code""")

        stations = []

        for row in result:
            stations.append({
                'sc': row.code,
                'max_ts': row.max_ts
            })

        response = json.dumps({'mts': stations})
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(response)))
        return response