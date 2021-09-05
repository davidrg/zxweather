# coding=utf-8
from datetime import datetime, time, timedelta

import pytz
from astral import Location
from twisted.application import service
from twisted.internet import task, reactor
from twisted.internet.defer import inlineCallbacks
from twisted.internet.ssl import ClientContextFactory
from twisted.python import log
from twisted.web.client import Agent, WebClientContextFactory
from twisted.web.http_headers import Headers

from .database import DatabaseReceiver, Database
from .mq_receiver import RabbitMqReceiver
from .readbody import readBody

# License: GPLv3 (Astral incompatible with GPLv2)

# noinspection PyClassicStyleClass
class NoVerifyWebClientContextFactory(ClientContextFactory):
    def __init__(self):
        pass

    def getContext(self, hostname=None, port=None):
        return ClientContextFactory.getContext(self)


class ImageLoggerService(service.Service):
    def __init__(self, dsn, station_code, mq_hostname, mq_port, mq_exchange,
                 mq_username, mq_password, mq_vhost, capture_interval,
                 capture_during_daylight_only,  sunrise_time, sunset_time,
                 use_solar_sensors, camera_url, image_source_code,
                 disable_cert_verification, calculate_schedule, latitude,
                 longitude, timezone, elevation, sunrise_offset, sunset_offset,
                 take_detected_sunrise_picture):
        """

        :param dsn: Database connection string
        :type dsn: str
        :param station_code: Weather station to monitor for sunlight
        :type station_code: str
        :param mq_hostname: RabbitMQ hostname
        :type mq_username: str or None
        :param mq_port: RabbitMQ port
        :type mq_port: int or None
        :param mq_exchange: Name of a topic exchange on the RabbitMQ server
                            where live weather data is broadcast.
        :type mq_exchange: str or None
        :param mq_username: RabbitMQ username
        :type mq_username: str or None
        :param mq_password: Password for the specified RabbitMQ user
        :type mq_password: str or None
        :param mq_vhost: RabbitMQ vhost.
        :type mq_vhost: str or None
        :param capture_interval: How often to capture images (in seconds)
        :type capture_interval: int
        :param sunrise_time: Time the sun rises
        :type sunrise_time: time
        :param sunset_time: Time the sun sets
        :type sunset_time: time
        :param use_solar_sensors: If the weather stations solar sensors should
                                  be used to automatically detect sunrise/sunset
                                  instead of using sunrise_time/sunset_time
        :type use_solar_sensors: bool
        :param camera_url: URL where images can be obtained from the IP Camera
        :type camera_url: str
        :param image_source_code: Code of the camera
        :type image_source_code: str
        :param disable_cert_verification: Disable SSL certificate verification
        :type disable_cert_verification: bool
        :param calculate_schedule: If sunrise and sunset times should be
            calculated based on location instead of using the solar sensors or
            a fixed schedule
        :type calculate_schedule: bool
        :param latitude: Latitude used for sunrise & sunset calculation
        :type latitude: float
        :param longitude: Longitude used for sunrise & sunset calculation
        :type longitude: float
        :param timezone: Timezone used for sunrise & sunset calculation
        :type timezone: str
        :param elevation: Elevation used for sunrise & sunset calculation
        :type elevation: float
        :param sunrise_offset: Time offset in minutes for calculated sunrise
        :type sunrise_offset: int
        :param sunset_offset: Time offset in minutes for calculated sunset
        :type sunset_offset: int
        :param take_detected_sunrise_picture: If an extra picture should be
        taken when the configured stations solar sensors first detect sunlight
        in the morning when running on a fixed or calculated schedule
        :type take_detected_sunrise_picture: bool
        """

        # Database connection for storing pictures
        self._database = Database(dsn, image_source_code)

        # How often pictures should be taken
        self._interval = capture_interval

        # No schedule - capture all the time
        self._daylight_only = capture_during_daylight_only

        # Fixed schedule
        self._fixed_sunrise = sunrise_time
        self._fixed_sunset = sunset_time

        self._daylight_trigger = use_solar_sensors
        self._calculated_schedule = calculate_schedule

        self._take_detected_sunrise_picture = take_detected_sunrise_picture
        self._db_receiver = None

        if calculate_schedule:
            # Schedule based on location
            self._location = Location()
            self._location.latitude = latitude
            self._location.longitude = longitude
            self._location.elevation = elevation
            self._location.timezone = timezone
            self._sunrise_offset = timedelta(minutes=sunrise_offset)
            self._sunset_offset = timedelta(minutes=sunset_offset)
            self._time_zone = pytz.timezone(timezone)

        # Schedule trigger by weather station sensors if use_solar_sensors is
        # true. Otherwise this is only used to capture the sunrise picture
        if self._daylight_trigger or take_detected_sunrise_picture:
            if mq_username is None:
                self._enable_mq = False
                self._db_receiver = DatabaseReceiver(dsn, station_code)
                self._db_receiver.LiveUpdate += self._live_data_received
            else:
                self._enable_mq = True
                self._mq_receiver = RabbitMqReceiver(mq_username, mq_password,
                                                     mq_vhost,
                                                     mq_hostname, mq_port,
                                                     mq_exchange,
                                                     station_code)
                self._mq_receiver.LiveUpdate += self._live_data_received

        # Camera settings
        self._camera_url = camera_url
        self._disable_cert_verification = disable_cert_verification

        # Initialise other members
        self._logging = False
        self._looper = task.LoopingCall(self._get_image)
        self._sunrise_image_taken = False
        self._logging_start_time = None

        log.msg("Current time: {0}".format(self.current_time))
        log.msg("Sunrise: {0}".format(self.sunrise))
        log.msg("Sunrise tomorrow: {0}".format(self.sunrise_tomorrow))
        log.msg("Sunset: {0}".format(self.sunset))

    @property
    def current_time(self):
        # When we're running on a calculated schedule all timestamps need
        # timezone information so we can compare them with the calculated
        # sunrise and sunset (which also include timezone information)
        if self._calculated_schedule:
            return self._time_zone.localize(datetime.now())

        # When running on a fixed schedule or using daylight sensor triggering
        # we don't need timezone info.
        return datetime.now()

    @property
    def sunrise(self):
        if self._calculated_schedule:
            return self._location.sunrise() + self._sunrise_offset

        current_time = self.current_time

        return datetime(current_time.year,
                        current_time.month,
                        current_time.day,
                        self._fixed_sunrise.hour,
                        self._fixed_sunrise.minute,
                        self._fixed_sunrise.second)

    @property
    def sunrise_tomorrow(self):
        if self._calculated_schedule:
            tomorrow = (self.current_time + timedelta(days=1)).date()
            return self._location.sunrise(tomorrow) + self._sunrise_offset

        return self.sunrise + timedelta(days=1)

    @property
    def sunset(self):
        if self._calculated_schedule:
            return self._location.sunset() + self._sunset_offset

        current_time = self.current_time

        return datetime(current_time.year,
                        current_time.month,
                        current_time.day,
                        self._fixed_sunset.hour,
                        self._fixed_sunset.minute,
                        self._fixed_sunset.second)

    def startService(self):
        service.Service.startService(self)

        self._database.connect()

        if self._daylight_only:
            # We're set to log only during daylight hours.

            if self._take_detected_sunrise_picture or self._daylight_trigger:
                if self._enable_mq:
                    self._mq_receiver.connect()
                else:
                    self._db_receiver.connect()

            if self._calculated_schedule or not self._daylight_trigger:
                # We're using a configured start time and end time for the
                # logging period. If we're currently in daylight hours start
                # logging immediately, otherwise schedule the logger to start
                # at the next sunrise.

                if self.sunrise <= self.current_time <= self.sunset:
                    self._start_logging("service start during daytime")
                else:
                    log.msg("Logger will start at sunrise: {0}".format(
                        self.sunrise))
                    self._schedule_logging_start()
            else:
                # The schedule will be started and stopped by the presence or
                # absence of UV or Solar Radiation received by the linked
                # station.
                log.msg("Logger start will be triggered by UV or Solar "
                        "Radiation from weather station")
        else:
            # We're set to log always. Start immediately.
            self._start_logging("service start")

    def stopService(self):
        service.Service.stopService(self)
        self._stop_logging("service stop")

    @inlineCallbacks
    def _get_image(self):
        ts = datetime.now()
        log.msg("Obtaining image...")

        try:
            if self._disable_cert_verification:
                context_factory = NoVerifyWebClientContextFactory()
            else:
                context_factory = WebClientContextFactory()
            agent = Agent(reactor, context_factory)

            response = yield agent.request(
                b'GET',
                self._camera_url.encode('latin1'),
                Headers({'User-Agent': ['zxweather image-logger']}),
                None
            )

            response_data = yield readBody(response)
            content_type = response.headers.getRawHeaders(
                    "Content-Type", ["application/octet-stream"])[0]

            if response_data is None or not len(response_data):
                raise Exception("Empty repsonse from camera")

            # noinspection PyBroadException
            try:
                yield self._database.store_image(ts, response_data, content_type)
                log.msg("Image stored.")
            except:
                # Lost database connection perhaps? Try reconnecting.
                log.msg("Possible database connection problem. "
                        "Attempting reconnect...")
                self._database.reconnect()

                if self._db_receiver is not None:
                    self._db_receiver.reconnect()

                yield self._database.store_image(ts, response_data,
                                                 content_type)
                log.msg("Database reconnected and image stored")
        except Exception as e:
            log.msg("Failed to capture or store image: {0}".format(e))
            raise e

    def _schedule_logging_start(self):
        """
        Schedules the logger to start at the next configured sunrise time.
        """
        current_time = self.current_time

        # The sunrise time for the current *date*
        sunrise_time = self.sunrise

        if current_time < sunrise_time:
            # And the sunrise for the current date is in the future. That means
            # we're some time between midnight and sunrise_time.
            seconds_until_sunrise = (sunrise_time - current_time).seconds
        else:
            # The sunrise for the current date has already been. Move onto the
            # one tomorrow

            sunrise_time = self.sunrise_tomorrow

            seconds_until_sunrise = (sunrise_time - current_time).seconds

        reactor.callLater(seconds_until_sunrise,
                          self._start_logging,
                          "scheduled sunrise")

        log.msg("Sunrise scheduled in {0} seconds time (at {1})".format(
                seconds_until_sunrise, sunrise_time))

    def _schedule_logging_stop(self):
        """
        Schedules the logger to stop at the next scheduled sunset time.
        """

        current_time = self.current_time

        sunset_time = self.sunset

        seconds_until_sunset = (sunset_time - current_time).seconds

        reactor.callLater(seconds_until_sunset,
                          self._stop_logging,
                          "scheduled sunset")

        log.msg("Sunset scheduled in {0} seconds time (at {1})".format(
                seconds_until_sunset, sunset_time))

    def _start_logging(self, trigger):

        if self._logging:
            return  # Nothing to do

        log.msg("Starting logger: {0}".format(trigger))
        self._logging = True

        self._logging_start_time = self.current_time

        # Interval in minutes (unlike time-lapse logger which is in seconds)
        self._looper.start(self._interval * 60, True)

        if self._daylight_only and not self._daylight_trigger:
            self._schedule_logging_stop()
        # else the logger will be stopped when the associated weather station
        # stops detecting sunlight

    def _stop_logging(self, trigger):

        if not self._logging:
            return

        if self._logging_start_time is not None:
            t = self._logging_start_time + timedelta(minutes=60)
            if t >= self.current_time:
                # The logger started less than an hour ago - this is probably
                # just a cloud passing across the sunrise. Ignore it.
                log.msg("Ignoring false sunset at {0}".format(
                    self.current_time))
                return

        # Fetch one last image before we stop for the night
        self._get_image()

        log.msg("Stopping logger: {0}".format(trigger))
        self._logging = False
        self._sunrise_image_taken = False

        if self._looper.running:
            self._looper.stop()

        if self._daylight_only and (self._calculated_schedule or
                                    not self._daylight_trigger):
            self._schedule_logging_start()
        # else the logger will be started when the associated weather station
        # detects sunlight or we weren't supposed to stop.

    def _live_data_received(self, uv_index, solar_radiation):
        """
        Called by either the RabbitMqReceiver or the DatabaseReceiver whenever
        live data is updated. If the supplied live-data is non-zero the logger
        will be started. If its zero (or negative I guess) the logger will stop.
        If its None it will be ignored.

        :param uv_index: Current UV index calculated by the weather station
        :type uv_index: float
        :param solar_radiation: Current solar radiation measured by the weather
                                station
        :type solar_radiation: float
        """

        # This function is only called when a live data subscription is enabled.
        # And this is only enabled when:
        #   - Daylight triggering is enabled. In which case this function needs
        #     to start and stop the logger
        #   - Sunrise image is enabled, in which case this function needs to
        #     take a picture on first detected light for the day.

        if uv_index is None and solar_radiation is None:
            # Lost communication with the ISS? Ignore it.
            return
        elif uv_index > 0 or solar_radiation > 0:
            if self._daylight_trigger:
                # Sunrise - start logging.
                self._start_logging("sunrise detected")
            elif self._daylight_only:
                # We're running on a fixed or calculated schedule. Take a
                # sunrise picture.

                if not self._sunrise_image_taken:
                    log.msg("Fetching picture of detected sunrise...")
                    self._sunrise_image_taken = True
                    self._get_image()
        elif self._daylight_only and self._daylight_trigger:
            # Daylight trigger - stop logging at sunset.
            self._stop_logging("sunset detected")
