# coding=utf-8
"""
Data logger for Davis Vantage Vue weather stations.
"""
import csv
from datetime import datetime, timedelta
import json
import os
import sys
import psycopg2
from twisted.application import service
from twisted.enterprise import adbapi
from twisted.internet import reactor
from twisted.internet.protocol import Protocol
from twisted.internet.serialport import SerialPort
from twisted.python import log
from davis_logger.davis import DavisWeatherStation

__author__ = 'david'

# 8 pm 10-feb-2012:

class DavisLoggerProtocol(Protocol):
    """
    Bridges communications with the weather station and handles logging data.
    """

    # The number of loop packets to request. Apparently there is an undocumented
    # maximum value of around 220. Probably best to keep this number 200 or
    # lower.
    _loopPacketRequestSize = 100

    def __init__(self, database_pool, station_id, latest_date, latest_time,
                 sample_error_file):
        self.station = DavisWeatherStation()

        # Setup the watchdog. This will give the data logger a kick if it stalls
        # (this can happen sometimes when data gets lost due to noise on the
        # serial cable)
        self._data_last_received = datetime.now()
        self._watchdog_reset = False
        self._reschedule_watchdog()

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

        # Fired when the init stuff has completed.
        self.station.InitCompleted += self._init_completed

        self._error_state = False

        self.sample_error_file = sample_error_file

    def dataReceived(self, data):
        """
        Passes all received data on to the weather station.
        :param data: Data to pass on
        """
        #log.msg('>> {0}'.format(toHexString(data)))
        self.station.dataReceived(data)

        self._data_last_received = datetime.now()

    def _sendData(self, data):
        #log.msg('<< {0}'.format(toHexString(data)))
        self.transport.write(data)

    def connectionMade(self):
        """ Called to start logging data. """
        log.msg('Logger started.')
        log.msg('Latest date: {0} - Latest time: {1} - Station: {2}'.format(
            self._latest_date, self._latest_time, self._station_id))
        #self.station.getLoopPackets(5)

        self.station.initialise()

    def _init_completed(self, stationType, hardwareType, version, versionDate,
                        stationTime, rainCollectorSizeName):

        log.msg('Station Type: {0} - {1}'.format(stationType, hardwareType))

        if stationType != 17:
            log.msg('WARNING: Unsupported station: {0}'.format(hardwareType))

        log.msg('Firmware Version: {0} ({1})'.format(versionDate, version))
        log.msg('Station Time: {0}'.format(stationTime))
        log.msg('Rain Collector Size: {0}'.format(rainCollectorSizeName))

        # Bring the database up-to-date
        self._lastRecord = -1
        self.station.getLoopPackets(self._loopPacketRequestSize)
        #self._fetch_samples()

    def _fetch_samples(self):
        self.station.getSamples((self._latest_date, self._latest_time))

    def _loopPacketReceived(self, loop):
        self._store_live(loop)

        nextRec = loop.nextRecord
        if nextRec != self._lastRecord:
            log.msg('New Page. Fetch Samples...')
            self._lastRecord = nextRec
            #self.station.cancelLoop()
            self._fetch_samples()

    def _loopFinished(self):
        self.station.getLoopPackets(self._loopPacketRequestSize)

    def _samplesArrived(self, sampleList):

        if self._error_state:
            self._write_samples_to_error_log(sampleList)
            log.msg("** WARNING: logger operating in error state. Stop logger, "
                    "correct error, merge sample error log into database and "
                    "restart logger.")
            log.msg("** NOTICE: Redirected {0} samples to sample error log"
                    .format(len(sampleList)))
        else:
            self._database_pool.runInteraction(self._store_samples_int, sampleList)

        # This obviously relies on the sample list being ordered.
        if len(sampleList) > 0:
            last = sampleList[-1]
            self._latest_date = last.dateInteger
            self._latest_time = last.timeInteger
            log.msg('Last record: {0} {1}'.format(last.dateStamp, last.timeStamp))
        log.msg('Received {0} archive records'.format(len(sampleList)))
        self.station.getLoopPackets(self._loopPacketRequestSize)

    def _write_samples_to_error_log(self, samples):
        with open(self.sample_error_file, "ab") as csvfile:
            csvwriter = csv.writer(csvfile)
            for sample in samples:
                csvwriter.writerow([
                    self._station_id,
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
                ])

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

        try:
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
        except psycopg2.Error as e:
            log.msg("""Database exception trying to insert samples:-
{0}
If error is violation of station_timestamp_unique constraint this may be caused
by a change in time zone due to daylight savings.""".format(e.pgerror))
            self._error_state = True

            log.msg("Logger is now in error state. All data will be redirected "
                    "to error_samples.csv")

            self._write_samples_to_error_log(samples)


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

    def _reschedule_watchdog(self, interval=60):
        reactor.callLater(interval, self._watchdog)

    def _watchdog(self):

        if self._data_last_received < datetime.now() - timedelta(0, 60):
            # No data has been received in the last minute. This indicates
            # the data logger has stalled (under normal operation we should be
            # receiving LOOP packets every 2 seconds or so). Time to restart it:

            if self._watchdog_reset:
                log.msg('Watchdog: Previous data logger restart failed.')

            log.msg('Watchdog: No data received from weather station since {0}'
                    .format(self._data_last_received))
            log.msg('Watchdog: Attempting to restart data logger...')
            self._watchdog_reset = True
            self.station.reset()
        elif self._watchdog_reset:
            log.msg('Watchdog: data logger restart was successful. ')

        self._reschedule_watchdog()

class DavisService(service.Service):
    def __init__(self, database, station, port, baud, sample_error_file):
        self.dbc = database
        self.station_code = station
        self.serial_port = port
        self.baud_rate = baud
        self.station_id = 0
        self.sample_error_file = sample_error_file

        if not os.access(self.sample_error_file, os.W_OK):
            raise Exception("Unable to write to failed sample output file {0}".format(self.sample_error_file))

    def startService(self):
        # This will kick off everything else.
        self._database_connect()
        service.Service.startService(self)

    def stopService(self):
        service.Service.stopService(self)
        # We don't really need to do anything to shutdown the logger service.
        # All we could really do is disconnect from the DB and tell the
        # console to stop sending live data. Thats not really necessary though -
        # the console will stop on its own after a while and the the database
        # will disconnect when the application exits.

    def _store_latest_rec(self, result, station_id):

        if len(result) == 0:
            latest_time = 0
            latest_date = 0
        else:
            latest_time = result[0][0]
            latest_date = result[0][1]

        logger = DavisLoggerProtocol(
            self.database_pool, station_id, latest_date, latest_time,
            self.sample_error_file)

        self.sp = SerialPort(logger,
                             self.serial_port,
                             reactor,
                             baudrate=self.baud_rate)


    def _store_station_id(self, result):
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

        self.database_pool.runQuery(query, (station_id,)).addCallback(
            self._store_latest_rec, station_id)

    def _database_connect(self,):
        self.database_pool = adbapi.ConnectionPool("psycopg2", self.dbc)
        self.database_pool.runQuery(
            "select station_id from station where code = %s",
            (self.station_code,)).addCallback(self._store_station_id)

