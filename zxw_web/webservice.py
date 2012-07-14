# coding=utf-8
"""
Various web services
"""
import json
import web
from config import db

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
                               'stat': "Invaild Method: GET"})
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(response)))
        return response

    @staticmethod
    def _update_live_data(live_data):
        """
        Updates the live data in the database.
        :param live_data: New live data
        :return: True if successful
        :type: boolean
        """

        live_update_query = """update live_data
                set download_timestamp = $download_timestamp,
                    invalid_data = $invalid_data,
                    indoor_relative_humidity = $indoor_rh,
                    indoor_temperature = $indoor_temp,
                    relative_humidity = $rh,
                    temperature = $temp,
                    absolute_pressure = $pressure,
                    average_wind_speed = $avg_wind_speed,
                    gust_wind_speed = $gust_wind_speed,
                    wind_direction = $wind_dir
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
            wind_dir = live_data['wd']
        )

        db.query(live_update_query, params)
        return True

    @staticmethod
    def _insert_samples(samples):
        """
        Inserts new samples into the database.
        :param samples: List of samples to insert.
        :type samples: list
        :return: Number of samples inserted
        :rtype: int
        """

        sample_insert_query = """
        insert into sample(sample_interval, record_number,
            download_timestamp, last_in_batch, invalid_data, time_stamp,
            indoor_relative_humidity, indoor_temperature, relative_humidity,
            temperature, absolute_pressure, average_wind_speed,
            gust_wind_speed, wind_direction, total_rain, rain_overflow)
        values($sample_interval, $record_number,
            $download_timestamp, $last_in_batch, $invalid_data, $time_stamp,
            $indoor_relative_humidity, $indoor_temperature, $relative_humidity,
            $temperature, $absolute_pressure, $average_wind_speed,
            $gust_wind_speed, $wind_direction, $total_rain, $rain_overflow)"""

        for sample in samples:
            # Insert each sample into the database
            params = dict(
                sample_interval = sample['si'],
                record_number = sample['rn'],
                download_timestamp = sample['dts'],
                last_in_batch = sample['lib'],
                invalid_data = sample['id'],
                time_stamp = sample['ts'],
                indoor_relative_humidity = sample['ih'],
                indoor_temperature = sample['it'],
                relative_humidity = sample['h'],
                temperature = sample['t'],
                absolute_pressure = sample['p'],
                average_wind_speed = sample['wa'],
                gust_wind_speed = sample['wg'],
                wind_direction = sample['wd'],
                total_rain = sample['rt'],
                rain_overflow = sample['ro']
            )

            db.query(sample_insert_query, params)

        return len(samples)

    @staticmethod
    def _fetch_and_verify_data():
        """
        Fetches the POSTed data and verifies its signature.
        :return: error string, json data
        :rtype: string, dict
        """

        post_data = web.data()

        # TODO: Verify signature

        data = json.loads(post_data)

        return None, data


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

        if data['v'] == 1:
            # Data structure version 1

            t = db.transaction()

            try :
                if 'ld_u' in data:
                    live_updated = data_load._update_live_data(data['ld_u'])

                if 's' in data:
                    samples_inserted = data_load._insert_samples(data['s'])
            except Exception as e:
                t.rollback()
                error = e.message
            else:
                t.commit()
                error = "OK"
        else:
            error = 'ERROR: Unsupported structure version'

        return error, live_updated, samples_inserted

    def POST(self):
        """
        Receives JSON-encoded data and loads it into the database
        :return: status message in JSON format.
        """

        live_updated = False
        samples_inserted = 0

        error, data = self._fetch_and_verify_data()

        if error is None:
            error, live_updated, samples_inserted = self._load_data(data)

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

        result = db.query("""select max(time_stamp)::varchar as "max_ts" from sample""")

        response = json.dumps({'max_ts': result[0].max_ts})
        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(response)))
        return response