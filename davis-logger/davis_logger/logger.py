# coding=utf-8
"""
Data logger for Davis Vantage Pro2 and Vue weather stations.
"""
import csv
from datetime import datetime, timedelta, time
import json
import os

import psycopg2
from twisted.application import service
from twisted.enterprise import adbapi
from twisted.internet import reactor, protocol, defer
from twisted.internet.defer import returnValue
from twisted.internet.error import ReactorNotRunning
from twisted.internet.protocol import Protocol
from twisted.internet.serialport import SerialPort
from twisted.python import log

from davis_logger.davis import DavisWeatherStation
from davis_logger.dst_switcher import DstInfo, DstSwitcher, NullDstSwitcher
from davis_logger.record_types.dmp import decode_date, decode_time

__author__ = 'david'


# Classic style class is required by twisted - not something we can fix.
# noinspection PyClassicStyleClass
from davis_logger.record_types.loop import Loop, Loop2, LiveData


class DavisLoggerProtocol(Protocol):
    """
    Bridges communications with the weather station and handles logging data.
    """

    def __init__(self, database_pool, station_id, latest_date, latest_time,
                 sample_error_file, auto_dst, time_zone, latest_ts,
                 database_live_data_enabled, mq_publisher, archive_interval,
                 station_config_json):
        self.station = DavisWeatherStation()

        self._auto_dst = auto_dst
        self._time_zone = time_zone
        self._latest_ts = latest_ts
        self._database_live_data_enabled = database_live_data_enabled
        self._mq_publisher = mq_publisher
        self._db_archive_interval = archive_interval
        self._station_config_json = station_config_json
        self._live_data = None

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
        self.station.sendData += self._send_data

        # This is fired when ever a loop packet arrives
        self.station.loopDataReceived += self._loop_packet_received
        self.station.loop2DataReceived += self._loop2_packet_received

        # Called when ever new samples are ready (the DMPAFT / getSamples()
        # command has finished).
        self.station.dumpFinished += self._samples_arrived

        # Fired when the init stuff has completed.
        self.station.InitCompleted += self._init_completed

        self._error_state = False

        self.sample_error_file = sample_error_file

    def dataReceived(self, data):
        """
        Passes all received data on to the weather station.
        :param data: Data to pass on
        """
        # log.msg('>> {0}'.format(toHexString(data)))
        self.station.data_received(data)

        self._data_last_received = datetime.now()

    def _send_data(self, data):
        # log.msg('<< {0}'.format(toHexString(data)))
        self.transport.write(data)

    def connectionMade(self):
        """ Called to start logging data. """
        log.msg('Logger started.')
        log.msg('Latest date: {0} - Latest time: {1} - Station: {2}'.format(
            self._latest_date, self._latest_time, self._station_id))

        self.station.initialise()

    def connectionLost(self, reason):
        log.msg("*** Connection lost ***")
        log.msg("Connection lost reason: {0}".format(reason))

    def _check_configured_sensors(self, station_list):
        """
        Compares the sensors that are (or probably are) being received by the
        console or envoy to those that are enabled in the database.
        :param station_list: List of stations/transmitters configured on the
                             console or envoy.
        """
        sensors_present = []
        receiving_from = "Receiving from:"
        for station in station_list:
            via = ""
            if station.repeater_id is not None:
                via = " via repeater {0}.".format(station.repeater_id)

            temp_hum = ""
            if station.humidity_sensor_id is not None and station.temperature_sensor_id is not None:
                temp_hum = " (extra temp sensor {0}, humidity {1})".format(
                    station.temperature_sensor_id, station.humidity_sensor_id)
            elif station.humidity_sensor_id is not None:
                temp_hum = " (extra humidity sensor {0})".format(station.humidity_sensor_id)
            elif station.temperature_sensor_id is not None:
                temp_hum = " (extra temperature sensor {0})".format(station.temperature_sensor_id)

            if "Soil" in station.type:
                sensors_present.append("soil_moisture")

            if "Leaf" in station.type:
                sensors_present.append("leaf_wetness")

            if station.humidity_sensor_id is not None \
                    and station.humidity_sensor_id < 2:
                # zxweather only supports two extra humidity sensors.
                sensors_present.append("extra_humidity_{0}".format(
                    station.humidity_sensor_id + 1))

            if station.temperature_sensor_id is not None \
                and station.temperature_sensor_id < 3:
                # zxweather only supports three extra temperature sensors
                sensors_present.append("extra_humidity_{0}".format(
                    station.humidity_sensor_id + 1))

            receiving_from += "\nID {id}: {type}{temphum}{via}".format(
                id=station.tx_id, type=station.type, via=via, temphum=temp_hum)
        log.msg(receiving_from)

        sensor_names = {
            "extra_humidity_1": "Extra humidity #1",
            "extra_humidity_2": "Extra humidity #2",
            "extra_temperature_1": "Extra temperature #1",
            "extra_temperature_2": "Extra temperature #2",
            "extra_temperature_3": "Extra temperature #3",
            "leaf_wetness": "One or more leaf wetness sensors",
            "soil_moisture": "One or more soil moisture sensors"
        }

        # We don't really care if parsing the station config json fails as this
        # is just a helpful sanity check - if sensor config is wrong it doesn't
        # actually affect the logger, it may just mean extra sensor data isn't
        # visible in the UI.
        # noinspection PyBroadException
        try:
            config = json.loads(self._station_config_json)["sensor_config"]

            enabled_sensors = []
            for k in config:
                is_enabled = config[k]["enabled"]
                if not is_enabled:
                    continue
                if k in ("extra_humidity_1", "extra_humidity_2",
                         "extra_temperature_1", "extra_temperature_2",
                         "extra_temperature_3"):
                    enabled_sensors.append(k)
                elif k in ("leaf_wetness_1", "leaf_wetness_2"):
                    enabled_sensors.append("leaf_wetness")
                elif k in ("soil_moisture_1", "soil_moisture_2",
                           "soil_moisture_3", "soil_moisture_4"):
                    enabled_sensors.append("soil_moisture")

            # Now lets see if the configuration on the console matches the
            # database.
            console_only = list(set(sensors_present) - set(enabled_sensors))
            configured_only = list(set(enabled_sensors) - set(sensors_present))

            console_only_msg = None
            for sensor_id in console_only:
                if console_only_msg is None:
                    console_only_msg = \
                        "The following may be available on the console or " \
                        "envoy but are not configured in the database:"
                console_only_msg += "\n\t- {0}".format(sensor_names[sensor_id])
            if console_only_msg is not None:
                console_only_msg += \
                    "\nData for these sensors, if present, will still be " \
                    "logged but will not be visible in the UI until they have " \
                    "been configured in the database."

            db_only_msg = None
            for sensor_id in configured_only:
                if db_only_msg is None:
                    db_only_msg = \
                        "The following are configured in the database but do " \
                        "not appear to be available from the console or envoy:"
                db_only_msg += "\n\t- {0}".format(sensor_names[sensor_id])

            sensor_config_msg = "Sensor config in the database looks reasonable."
            if db_only_msg is not None or console_only_msg is not None:
                sensor_config_msg = "Sensor configuration check:"
                if console_only_msg is not None:
                    sensor_config_msg += "\n" + console_only_msg
                if db_only_msg is not None:
                    if console_only_msg is not None:
                        sensor_config_msg += "\n"
                    sensor_config_msg += "\n" + db_only_msg

            log.msg(sensor_config_msg)

        except:
            pass  # Guess no sensor config check log message. Oh well, we tried.

    def _init_completed(self, station_type, hardware_type, version, version_date,
                        station_time, rain_collector_size_name, archive_interval,
                        auto_dst_on, dst_on, station_list, loop2_enabled):

        log.msg('Station Type: {0} - {1}'.format(station_type, hardware_type))

        log.msg('Firmware Version: {0} ({1})'.format(version_date, version))
        log.msg('Station Time: {0}'.format(station_time))
        log.msg('Rain Collector Size: {0}'.format(rain_collector_size_name))
        log.msg('Archive Interval: {0} minutes'.format(archive_interval))
        log.msg('Station Auto DST Enabled: {0}'.format(auto_dst_on))
        log.msg('Station Manual Daylight Savings - DST On: {0}'.format(dst_on))
        log.msg('LOOP2 data available: {0}'.format(loop2_enabled))

        self._check_configured_sensors(station_list)

        if archive_interval * 60 != self._db_archive_interval:
            log.msg('**** CONFIGURATION ERROR ****\nConsole archive interval '
                    '({0} seconds) does not match the database\nsetting for '
                    'this station ({1} seconds). This *will* cause incorrect\n'
                    'functionality in a number of areas. You *must* correct '
                    'the database\nsetting or change the archive interval on '
                    'the weather station console\nor envoy.'.format(
                        archive_interval*60, self._db_archive_interval))
            log.msg("Unable to safely proceed - logger stopped. Correct "
                    "configuration error and try again.")
            try:
                reactor.stop()
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                pass
            return

        if archive_interval != 5:
            log.msg('WARNING: Archive interval should be set to\n5 minutes. '
                    'While your current setting of {0} will probably work as '
                    'long as the\nsetting for this station in the database '
                    'matches it is untested. It is\nrecommended that you change'
                    'it to 5 minutes via WeatherLink.'.format(archive_interval))

        dst_is_off = not dst_on
        self._suppress_dst_fix = auto_dst_on

        if self._auto_dst:

            if auto_dst_on:
                log.msg("NOTICE: The station has automatic daylight savings "
                        "turned on. This program will not attempt to alter "
                        "daylight savings but an attempt will be made to fix"
                        " any incoming samples with an incorrect timestamp.")

            dst_info = DstInfo(self._time_zone)
            self._dst_switcher = DstSwitcher(dst_info, archive_interval,
                                             self._latest_ts)
            if dst_is_off:
                self._dst_switcher.suppress_dst_off()
        else:
            # If auto daylight savings is disabled then use a dummy switcher
            # that doesn't do anything.
            self._dst_switcher = NullDstSwitcher()

        self._live_data = LiveData(not loop2_enabled)

        # Bring the database up-to-date
        self._lastRecord = -1
        self.station.start_live_data()

    def _fetch_samples(self):
        d = decode_date(self._latest_date)
        t = decode_time(self._latest_time)

        self.station.get_samples(datetime.combine(d, t))

    def _loop_packet_received(self, loop):
        self._publish_live(loop)

        next_rec = loop.nextRecord
        if next_rec != self._lastRecord:
            log.msg('New Page. Fetch Samples...')
            self._lastRecord = next_rec
            self._fetch_samples()

    def _loop2_packet_received(self, loop2):
        self._publish_live(loop2)

    def _samples_arrived(self, sample_list):
        if len(sample_list) == 0:
            log.msg("No new samples received.")
            return

        adjusted_samples = [self._dst_switcher.process_sample(sample)
                            for sample in sample_list]

        if self._dst_switcher.station_time_needs_adjusting and \
                not self._suppress_dst_fix:

            # This will automatically pause looping while it adjusts config and
            # resume once its finished.
            self.station.set_manual_daylight_savings_state(
                self._dst_switcher.new_dst_state_is_on, None)

        if self._error_state:
            self._write_samples_to_error_log(adjusted_samples)
            log.msg("** WARNING: logger operating in error state. Stop logger, "
                    "correct error, merge sample error log into database and "
                    "restart logger.")
            log.msg("** NOTICE: Redirected {0} samples to sample error log"
                    .format(len(adjusted_samples)))
        else:
            self._database_pool.runInteraction(self._store_samples_int,
                                               adjusted_samples)

        # This obviously relies on the sample list being ordered.
        if len(adjusted_samples) > 0:
            last = adjusted_samples[-1]
            self._latest_date = last.dateInteger
            self._latest_time = last.timeInteger
            log.msg(
                'Last record: {0} {1}'.format(last.dateStamp, last.timeStamp))
        log.msg('Received {0} archive records'.format(len(adjusted_samples)))

        # We must keep track of the last timestamp saved to the database so we
        # can correctly reinitialise the DstSwitcher after a watchdog timer
        # initiated logger restart. If we initialise it with some very old
        # timestamp on the other side of a DST Start transition it could result
        # in the station clock being incorrectly put forward an hour every time
        # the watchdog timer restarts the logger.
        last_sample = adjusted_samples[-1]
        self._latest_ts = datetime.combine(last_sample.dateStamp,
                                           last_sample.timeStamp)

    def _write_samples_to_error_log(self, samples):
        with open(self.sample_error_file, "ab") as csvfile:
            csvwriter = csv.writer(csvfile)
            for sample in samples:
                csvwriter.writerow([
                    self._station_id,
                    datetime.now(),  # Download timestamp
                    datetime.combine(sample.dateStamp,
                                     sample.timeStamp),
                    sample.timeZone,
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
                temperature, absolute_pressure, mean_sea_level_pressure,
                average_wind_speed, gust_wind_speed, wind_direction, rainfall, 
                station_id)
            values(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            returning sample_id
                """

        davis_query = """
                insert into davis_sample(
                    sample_id, record_time, record_date, high_temperature,
                    low_temperature, high_rain_rate, solar_radiation,
                    wind_sample_count, gust_wind_direction, average_uv_index,
                    evapotranspiration, high_solar_radiation, high_uv_index,
                    forecast_rule_id, leaf_wetness_1, leaf_wetness_2, 
                    leaf_temperature_1, leaf_temperature_2, soil_moisture_1, 
                    soil_moisture_2, soil_moisture_3, soil_moisture_4, 
                    soil_temperature_1, soil_temperature_2, soil_temperature_3, 
                    soil_temperature_4, extra_humidity_1, extra_humidity_2, 
                    extra_temperature_1, extra_temperature_2, 
                    extra_temperature_3)
                values(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,
                       %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
                """

        try:
            for sample in samples:

                t = sample.timeStamp

                if sample.timeZone is not None:
                    # timeZone is only present on samples adjusted by the
                    # DST Switcher.
                    t = time(t.hour, t.minute, t.second, t.microsecond,
                             psycopg2.tz.FixedOffsetTimezone(
                                 offset=sample.timeZone))

                ts = datetime.combine(sample.dateStamp, t)

                txn.execute(base_query,
                            (
                                datetime.now(),  # Download timestamp
                                ts,
                                sample.insideHumidity,
                                sample.insideTemperature,
                                sample.outsideHumidity,
                                sample.outsideTemperature,
                                # TODO: Remove the absolute pressure column from
                                #       the query once everything has been
                                #       updated.
                                sample.barometer,
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
                                sample.forecastRule,
                                sample.leafWetness[0],
                                sample.leafWetness[1],
                                sample.leafTemperature[0],
                                sample.leafTemperature[1],
                                sample.soilMoistures[0],
                                sample.soilMoistures[1],
                                sample.soilMoistures[2],
                                sample.soilMoistures[3],
                                sample.soilTemperatures[0],
                                sample.soilTemperatures[1],
                                sample.soilTemperatures[2],
                                sample.soilTemperatures[3],
                                sample.extraHumidities[0],
                                sample.extraHumidities[1],
                                sample.extraTemperatures[0],
                                sample.extraTemperatures[1],
                                sample.extraTemperatures[2],
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

    def _publish_live(self, loop):

        if isinstance(loop, Loop2):
            self._live_data.update_loop2(loop)
        else:
            self._live_data.update_loop(loop)

        if not self._live_data.ready:
            return  # Not ready yet.

        if self._mq_publisher is not None:
            self._mq_publisher.publish_live(self._live_data)

        if self._database_live_data_enabled:
            self._store_live()

    def _store_live(self):
        """
        Updates the database live data tables with the latest weather data.
        """

        query = """update live_data
                set download_timestamp = %s,
                    indoor_relative_humidity = %s,
                    indoor_temperature = %s,
                    relative_humidity = %s,
                    temperature = %s,
                    mean_sea_level_pressure = %s,
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
                self._live_data.insideHumidity,
                self._live_data.insideTemperature,
                self._live_data.outsideHumidity,
                self._live_data.outsideTemperature,
                self._live_data.barometer,
                self._live_data.absoluteBarometricPressure,  # Loop2 only
                self._live_data.windSpeed,
                None,  # Gust wind speed isn't supported for live data
                self._live_data.windDirection,
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
                    forecast_rule_id = %s,
                    uv_index = %s,
                    solar_radiation = %s,
                    average_wind_speed_10m = %s,
                    
                    -- Loop1 only values
                    leaf_wetness_1 = %s, 
                    leaf_wetness_2 = %s, 
                    leaf_temperature_1 = %s, 
                    leaf_temperature_2 = %s, 
                    soil_moisture_1 = %s, 
                    soil_moisture_2 = %s, 
                    soil_moisture_3 = %s, 
                    soil_moisture_4 = %s, 
                    soil_temperature_1 = %s, 
                    soil_temperature_2 = %s, 
                    soil_temperature_3 = %s, 
                    soil_temperature_4 = %s, 
                    extra_humidity_1 = %s, 
                    extra_humidity_2 = %s, 
                    extra_temperature_1 = %s, 
                    extra_temperature_2 = %s, 
                    extra_temperature_3 = %s,
                    
                    -- Loop2 only values
                    average_wind_speed_2m = %s,
                    gust_wind_speed_10m = %s,
                    gust_wind_direction_10m = %s,
                    heat_index = %s,
                    thsw_index = %s,
                    altimeter_setting = %s
                where station_id = %s
                """

        self._database_pool.runOperation(
            query,
            (
                self._live_data.barTrend,
                self._live_data.rainRate,
                self._live_data.stormRain,
                self._live_data.startDateOfCurrentStorm,
                self._live_data.transmitterBatteryStatus,    # Loop1 only
                self._live_data.consoleBatteryVoltage,    # Loop1 only
                self._live_data.forecastIcons,    # Loop1 only
                self._live_data.forecastRuleNumber,  # Loop1 only
                self._live_data.UV,
                self._live_data.solarRadiation,
                self._live_data.averageWindSpeed10min,

                # Loop1
                self._live_data.leafWetness[0],
                self._live_data.leafWetness[1],
                self._live_data.leafTemperatures[0],
                self._live_data.leafTemperatures[1],
                self._live_data.soilMoistures[0],
                self._live_data.soilMoistures[1],
                self._live_data.soilMoistures[2],
                self._live_data.soilMoistures[3],
                self._live_data.soilTemperatures[0],
                self._live_data.soilTemperatures[1],
                self._live_data.soilTemperatures[2],
                self._live_data.soilTemperatures[3],
                self._live_data.extraHumidities[0],
                self._live_data.extraHumidities[1],
                self._live_data.extraTemperatures[0],
                self._live_data.extraTemperatures[1],
                self._live_data.extraTemperatures[2],

                # Loop2
                self._live_data.averageWindSpeed2min,
                self._live_data.windGust10m,
                self._live_data.windGust10mDirection,
                self._live_data.heatIndex,
                self._live_data.thswIndex,
                self._live_data.altimeterSetting,

                # Station ID
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
            self._watchdog_reset = False

        self._reschedule_watchdog()


class MQPublisher(object):
    """
    Handles publishing data to a message broker.
    """

    def __init__(self, channel, station_code, exchange):
        """
        Sets up the publisher.
        """
        import pika
        self._channel = channel
        self._key_prefix = station_code.lower() + "."
        self._properties = pika.BasicProperties(
            app_id='DavisLogger',
            content_type='application/json',
            type='DavisLive')
        self._exchange = exchange

    def publish_live(self, loop_data):
        """
        Publishes the live data to the message broker.
        :param loop_data: Object providing a combined view of the latest Loop
                          and Loop2 packets
        :type loop_data: LiveData
        """

        loop_dict = loop_data.to_dict()

        routing_key = self._key_prefix + "live"
        self._channel.basic_publish(self._exchange, routing_key,
                                    json.dumps(loop_dict),
                                    self._properties)


class DavisService(service.Service):
    """
    twistd service for the Davis Vantage Data Logger
    """
    def __init__(self, database, station, port, baud, sample_error_file,
                 auto_dst, time_zone, mq_host, mq_port, mq_exchange,
                 mq_username, mq_password, mq_vhost):
        self.dbc = database
        self.station_code = station
        self.serial_port = port
        self.baud_rate = baud
        self.station_id = 0
        self.sample_error_file = sample_error_file
        self.auto_dst = auto_dst
        self.time_zone = time_zone

        self._station_config_json = None
        self._sample_interval = None

        self._mq_host = mq_host
        self._mq_port = mq_port
        self._mq_exchange = mq_exchange
        self._mq_username = mq_username
        self._mq_password = mq_password
        self._mq_vhost = mq_vhost
        self._mq_publisher = None

        if not os.path.exists(self.sample_error_file):
            # File doesn't exist. Try to create it.
            log.msg("Sample error file does not exist. Attempting to create...")
            with open(self.sample_error_file, 'a'):
                os.utime(self.sample_error_file, None)

        if not os.access(self.sample_error_file, os.W_OK):
            raise Exception(
                "Unable to write to failed sample output file {0}".format(
                    self.sample_error_file))

    def startService(self):
        """
        Starts the service
        """
        # This will kick off everything else.
        self._database_connect()
        service.Service.startService(self)

    def stopService(self):
        """
        Stops the service
        """
        service.Service.stopService(self)
        # We don't really need to do anything to shutdown the logger service.
        # All we could really do is disconnect from the DB and tell the
        # console to stop sending live data. Thats not really necessary though -
        # the console will stop on its own after a while and the the database
        # will disconnect when the application exits.

    def _store_latest_rec(self, result, station_id):

        if len(result) == 0:
            self._latest_time = 0
            self._latest_date = 0
            self._latest_ts = datetime.now()
        else:
            self._latest_time = result[0][0]
            self._latest_date = result[0][1]
            self._latest_ts = result[0][2]

        if self._mq_host is None:
            self._start_logging()
        else:
            self._mq_connect()

    @defer.inlineCallbacks
    def _mq_setup(self, connection):
        log.msg("Broker connected. Configuring...")

        channel = yield connection.channel()

        yield channel.exchange_declare(exchange=self._mq_exchange,
                                       exchange_type='topic')
        self._mq_publisher = MQPublisher(channel, self.station_code,
                                         self._mq_exchange)

        log.msg("Broker configured.")
        self._start_logging()

    def _mq_connect(self):
        try:
            log.msg("Attempting to connect to message broker...")
            import pika
            from pika.credentials import PlainCredentials
            from pika.adapters import twisted_connection

            credentials = PlainCredentials(self._mq_username, self._mq_password)

            parameters = pika.ConnectionParameters(virtual_host=self._mq_vhost,
                                                   credentials=credentials)
            cc = protocol.ClientCreator(
                reactor, twisted_connection.TwistedProtocolConnection,
                parameters)

            def _conn_ready(c):
                c.ready.addCallback(lambda _: c)
                return c.ready

            d = cc.connectTCP(self._mq_host, self._mq_port)
            d.addCallback(_conn_ready)
            d.addCallback(self._mq_setup)

        except ImportError:
            log.msg("*** WARNING: RabbitMQ Client library (pika) is not "
                    "available. Data will NOT be logged to RabbitMQ.")
            self._start_logging()

    def _start_logging(self):
        log.msg("Starting data logger...")
        logger = DavisLoggerProtocol(
            self.database_pool, self._station_id, self._latest_date,
            self._latest_time, self.sample_error_file, self.auto_dst,
            self.time_zone, self._latest_ts, self._live_available,
            self._mq_publisher, self._sample_interval,
            self._station_config_json)

        self.sp = SerialPort(logger,
                             self.serial_port,
                             reactor,
                             baudrate=self.baud_rate)

    def _store_station_id(self, result):
        self._station_id = result[0][0]
        self._live_available = result[0][1]
        log.msg('Station ID: {0}'.format(self._station_id))
        log.msg('Live data available: {0}'.format(self._live_available))

        query = """
                select ds.record_time, ds.record_date, s.time_stamp
                from sample s
                inner join davis_sample ds on ds.sample_id = s.sample_id
                where station_id = %s
                order by time_stamp desc
                fetch first 1 rows only
                """

        self.database_pool.runQuery(query, (self._station_id,)).addCallback(
            self._store_latest_rec, self._station_id)

    @defer.inlineCallbacks
    def _get_schema_version(self):
        result = yield self.database_pool.runQuery(
            "select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")

        if len(result) == 0:
            returnValue(1)

        result = yield self.database_pool.runQuery(
            "select v::integer from db_info where k = 'DB_VERSION'")

        returnValue(result[0][0])

    @defer.inlineCallbacks
    def _check_db(self):
        """
        Checks the database is compatible and gets a few other details
        """
        schema_version = yield self._get_schema_version()
        log.msg("Database schema version: {0}".format(schema_version))

        if schema_version < 3:
            log.msg("*** ERROR: davis-logger requires at least database "
                    "version 3 (zxweather 1.0.0). Please upgrade your "
                    "database.")
            try:
                reactor.stop()
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                pass
        else:
            result = yield self.database_pool.runQuery(
                "select version_check('DAVIS',1,0,0)")
            if not result[0][0]:
                result = yield self.database_pool.runQuery(
                    "select minimum_version_string('DAVIS')")

                log.msg("*** ERROR: This version of davis-logger is "
                        "incompatible with the configured database. The "
                        "minimum davis-logger (or zxweather) version supported "
                        "by this database is: {0}.".format(
                            result[0][0]))
                try:
                    reactor.stop()
                except ReactorNotRunning:
                    # Don't care. We wanted it stopped, turns out it already is
                    # that or its not yet started in which case there isn't
                    # anything much we can do to terminate yet.
                    pass

        result = yield self.database_pool.runQuery(
            "select archived, st.code as hw_type, sample_interval, station_config from station s "
            "inner join station_type st on s.station_type_id = st.station_type_id "
            "where upper(s.code) = upper(%s)",
            (self.station_code,))

        archived = result[0][0]
        hw_type = result[0][1].upper()
        self._sample_interval = result[0][2]
        self._station_config_json = result[0][3]

        fail = False
        if archived:
            log.msg("*** ERROR: unable to log to station {0} - station is archived".format(
                self.station_code))
            fail = True
        elif hw_type != 'DAVIS':
            log.msg("*** ERROR: Incorrect hardware type for station {0}: {1}".format(
                self.station_code, hw_type))
            fail = True

        if fail:
            log.msg("Critical error: Terminating")
            try:
                reactor.stop()
            except ReactorNotRunning:
                # Don't care. We wanted it stopped, turns out it already is
                # that or its not yet started in which case there isn't
                # anything much we can do to terminate yet.
                pass

        self.database_pool.runQuery(
            "select station_id, live_data_available "
            "from station where upper(code) = upper(%s)",
            (self.station_code,)).addCallback(self._store_station_id)

    def _database_connect(self, ):
        self.database_pool = adbapi.ConnectionPool("psycopg2", self.dbc)

        self._check_db()
