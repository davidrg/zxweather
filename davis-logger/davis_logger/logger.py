# coding=utf-8
"""
Data logger for Davis Vantage Vue weather stations.
"""
from datetime import datetime
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

# 8 pm 10-feb-2012: \x4a\x1a\xd0\x07

class DavisLoggerProtocol(Protocol):
    """
    Bridges communications with the weather station and handles logging data.
    """

    def __init__(self):
        self.station = DavisWeatherStation()

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
        self.station.getLoopPackets(5)

    def _loopPacketReceived(self, loop):
        log.msg('LOOP: ' + repr(loop))
        self._store_live(loop)

    def _loopFinished(self):
        log.msg('LOOP finished')

    def _samplesArrived(self, sampleList):
        log.msg('DMP finished: ' + repr(sampleList))

    def _store_live(self, loop):
        """
        :param loop:
        :type loop: davis_logger.record_types.loop.Loop
        :return:
        """
        global database_pool, station_id

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

        database_pool.runOperation(
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
                station_id
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

        database_pool.runOperation(
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
                station_id
            )
        )


def _store_station_id(result):
    global station_id
    station_id = result[0][0]
    log.msg('Station ID: {0}'.format(station_id))


def _database_connect(conn_str, station_code):
    global database_pool
    database_pool = adbapi.ConnectionPool("psycopg2", conn_str)
    deferred = database_pool.runQuery(
        "select station_id from station where code = %s", (station_code,))
    deferred.addCallback(_store_station_id)


log.startLogging(sys.stdout)

_database_connect("host=localhost port=5432 user=zxweather password=password "
                  "dbname=davis_test", "rua2")

SerialPort(DavisLoggerProtocol(), 'COM1', reactor, baudrate=19200)

reactor.run()