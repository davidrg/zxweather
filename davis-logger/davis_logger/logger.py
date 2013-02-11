# coding=utf-8
"""
Data logger for Davis Vantage Vue weather stations.
"""
from datetime import datetime
import struct
import sys
from twisted.enterprise import adbapi
from twisted.internet import reactor
from twisted.internet.defer import Deferred
from twisted.internet.protocol import Protocol
from twisted.internet.serialport import SerialPort
from twisted.python import log
from davis_logger.davis import DavisWeatherStation
from davis_logger.util import toHexString

__author__ = 'david'

# 8 pm 10-feb-2012:

class DavisLoggerProtocol(Protocol):
    """
    Bridges communications with the weather station and handles logging data.
    """

    def __init__(self, database_pool, station_id, latest_date, latest_time):
        self.station = DavisWeatherStation()

        self._database_pool = database_pool
        self._station_id = station_id
        self._latest_date = latest_date
        self._latest_time = latest_time

        # Subscribe to outgoing data event
        self.station.sendData += self._sendData

        # This is fired when ever a loop packet arrives
        self.station.loopDataReceived += self._loopPacketReceived

        # Called when ever new samples are ready (the DMPAFT / getSamples()
        # command has finished).
        self.station.dumpFinished += self._samplesArrived

        # Fired when ever the loop packet subscription expires.
        self.station.loopFinished += self._loopFinished

    def dataReceived(self, data):
        """
        Passes all received data on to the weather station.
        :param data: Data to pass on
        """
        #log.msg('>> {0}'.format(toHexString(data)))
        self.station.dataReceived(data)

    def _sendData(self, data):
        #log.msg('<< {0}'.format(toHexString(data)))
        self.transport.write(data)

    def connectionMade(self):
        """ Called to start logging data. """
        log.msg('Latest date: {0} - Latest time: {1} - Station: {2}'.format(
            self._latest_date, self._latest_time, self._station_id))
        #self.station.getLoopPackets(5)

        self._fetch_samples()

    def _fetch_samples(self):
        self.station.getSamples((self._latest_date, self._latest_time))

    def _loopPacketReceived(self, loop):
        log.msg('LOOP: ' + repr(loop))
        self._store_live(loop)

    def _loopFinished(self):
        log.msg('LOOP finished')

    def _samplesArrived(self, sampleList):
        self._database_pool.runInteraction(self._store_samples_int, sampleList)

        # This obviously relies on the sample list being ordered.
        if len(sampleList) > 0:
            last = sampleList[-1]
            self._latest_date = last.dateInteger
            self._latest_time = last.timeInteger
            log.msg('Last record: {0} {1}'.format(last.dateStamp, last.timeStamp))
        log.msg('Received {0} archive records'.format(len(sampleList)))

    def _store_samples_int(self, txn, samples):
        """
        :param txn: Database cursor
        :param samples: Samples to insert
        :type samples: list of davis_logger.record_types.dmp.Dmp
        """

        base_query = """
            insert into sample(download_timestamp, time_stamp,
                indoor_relative_humidity, indoor_temperature, relative_humidity,
                temperature, absolute_pressure, average_wind_speed,
                gust_wind_speed, wind_direction, rainfall, station_id)
            values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            returning sample_id
                """

        davis_query = """
                insert into davis_sample(
                    sample_id, record_time, record_date, high_temperature,
                    low_temperature, high_rain_rate, solar_radiation,
                    wind_sample_count, gust_wind_direction, average_uv_index,
                    evapotranspiration, high_solar_radiation, high_uv_index,
                    forecast_rule_id)
                values(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
                """

        for sample in samples:
            txn.execute(base_query,
                        (
                            datetime.now(),  # Download timestamp
                            datetime.combine(sample.dateStamp,
                                             sample.timeStamp),
                            sample.insideHumidity,
                            sample.insideTemperature,
                            sample.outsideHumidity,
                            sample.outsideTemperature,
                            sample.barometer,
                            sample.averageWindSpeed,
                            sample.highWindSpeed,
                            sample.prevailingWindDirection,
                            sample.rainfall,
                            self._station_id
                        ))

            sample_id = txn.fetchone()[0]

            txn.execute(davis_query,
                        (
                            sample_id,
                            sample.timeInteger,
                            sample.dateInteger,
                            sample.highOutsideTemperature,
                            sample.lowOutsideTemperature,
                            sample.highRainRate,
                            sample.solarRadiation,
                            sample.numberOfWindSamples,
                            sample.highWindSpeedDirection,
                            sample.averageUVIndex,
                            sample.ET,
                            sample.highSolarRadiation,
                            sample.highUVIndex,
                            sample.forecastRule
                        ))

    def _store_live(self, loop):
        """
        :param loop:
        :type loop: davis_logger.record_types.loop.Loop
        :return:
        """
        query = """update live_data
                set download_timestamp = %s,
                    indoor_relative_humidity = %s,
                    indoor_temperature = %s,
                    relative_humidity = %s,
                    temperature = %s,
                    absolute_pressure = %s,
                    average_wind_speed = %s,
                    gust_wind_speed = %s,
                    wind_direction = %s
                where station_id = %s
                """

        self._database_pool.runOperation(
            query,
            (
                datetime.now(),  # Download TS
                loop.insideHumidity,
                loop.insideTemperature,
                loop.outsideHumidity,
                loop.outsideTemperature,
                loop.barometer,
                loop.windSpeed,
                None,  # Gust wind speed isn't supported for live data
                loop.windDirection,
                self._station_id
            )
        )

        query = """
                update davis_live_data
                set bar_trend = %s,
                    rain_rate = %s,
                    storm_rain = %s,
                    current_storm_start_date = %s,
                    transmitter_battery = %s,
                    console_battery_voltage = %s,
                    forecast_icon = %s,
                    forecast_rule_id = %s
                where station_id = %s
                """

        self._database_pool.runOperation(
            query,
            (
                loop.barTrend,
                loop.rainRate,
                loop.stormRain,
                loop.startDateOfCurrentStorm,
                loop.transmitterBatteryStatus,
                loop.consoleBatteryVoltage,
                loop.forecastIcons,
                loop.forecastRuleNumber,
                self._station_id
            )
        )

def _store_latest_rec(result, database_pool, station_id):

    if len(result) == 0:
        latest_time = 0
        latest_date = 0
    else:
        latest_time = result[0][0]
        latest_date = result[0][1]

    logger = DavisLoggerProtocol(
        database_pool, station_id, latest_date, latest_time)

    SerialPort(logger, 'COM1', reactor, baudrate=19200)


def _store_station_id(result, database_pool):
    station_id = result[0][0]
    log.msg('Station ID: {0}'.format(station_id))

    query = """
            select record_time, record_date
            from sample s
            inner join davis_sample ds on ds.sample_id = s.sample_id
            where station_id = %s
            order by time_stamp desc
            fetch first 1 rows only
            """

    database_pool.runQuery(query, (station_id,)).addCallback(
        _store_latest_rec, database_pool, station_id)


def _database_connect(conn_str, station_code):
    global database_pool
    database_pool = adbapi.ConnectionPool("psycopg2", conn_str)
    database_pool.runQuery(
        "select station_id from station where code = %s",
        (station_code,)).addCallback(_store_station_id, database_pool)


log.startLogging(sys.stdout)

# This sets everything running.
_database_connect("host=localhost port=5432 user=zxweather password=password "
                  "dbname=davis_test", "rua2")

reactor.run()