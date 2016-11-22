# coding=utf-8
from datetime import datetime, time, timedelta

from twisted.application import service
from twisted.internet import task, reactor
from twisted.internet.defer import inlineCallbacks
from twisted.internet.ssl import ClientContextFactory
from twisted.python import log
from twisted.web.client import Agent, WebClientContextFactory
from twisted.web.http_headers import Headers

from database import DatabaseReceiver, Database
from mq_receiver import RabbitMqReceiver
from readbody import readBody


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
                 disable_cert_verification):
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
        :param capture_during_daylight_only: If images should only be captured
                                             during daylight hours
        :type capture_during_daylight_only: bool
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
        """

        self._mq_receiver = RabbitMqReceiver(mq_username, mq_password, mq_vhost,
                                             mq_hostname, mq_port, mq_exchange,
                                             station_code)

        self._db_receiver = DatabaseReceiver(dsn, station_code)

        self._database = Database(dsn, image_source_code)

        self._mq_receiver.LiveUpdate += self._live_data_received
        self._db_receiver.LiveUpdate += self._live_data_received

        self._daylight_only = capture_during_daylight_only
        self._sunrise = sunrise_time
        self._sunset = sunset_time
        self._daylight_trigger = use_solar_sensors
        self._interval = capture_interval
        self._disable_cert_verification = disable_cert_verification

        self._enable_mq = True
        if mq_username is None:
            self._enable_mq = False

        self._logging = False

        self._looper = task.LoopingCall(self._get_image)

        self._sunrise_image_taken = False

        self._camera_url = camera_url

    def startService(self):
        service.Service.startService(self)

        self._database.connect()

        if self._daylight_only:
            # We're set to log only during daylight hours.

            if self._daylight_trigger:
                # The schedule will be started and stopped by the presence or
                # absence of UV or Solar Radiation received by the linked
                # station.
                log.msg("Logger start will be triggered by UV or Solar "
                        "Radiation from weather station")

                if self._enable_mq:
                    self._mq_receiver.connect()
                else:
                    self._db_receiver.connect()
            else:
                # We're using a configured start time and end time for the
                # logging period. If we're currently in daylight hours start
                # logging immediately, otherwise schedule the logger to start
                # at the next sunrise.

                current_time = datetime.now().time()
                if self._sunrise <= current_time <= self._sunset:
                    self._start_logging(
                            "service start during configured daytime")
                else:
                    log.msg("Logger will start at scheduled sunrise time.")
                    self._schedule_logging_start()
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
                'GET',
                self._camera_url,
                Headers({'User-Agent': ['zxweather image-logger']}),
                None
            )

            response_data = yield readBody(response)
            content_type = response.headers.getRawHeaders(
                    "Content-Type", ["application/octet-stream"])[0]

            if response_data is None or not len(response_data):
                raise Exception("Empty repsonse from camera")

            yield self._database.store_image(ts, response_data, content_type)

            log.msg("Image stored.")
        except Exception as e:
            log.msg("Failed to capture or store image: {0}".format(e.message))

    def _schedule_logging_start(self):
        """
        Schedules the logger to start at the next configured sunrise time.
        """
        current_time = datetime.now()

        # The sunrise time for the current *date*
        sunrise_time = datetime(current_time.year,
                                current_time.month,
                                current_time.day,
                                self._sunrise.hour,
                                self._sunrise.minute,
                                self._sunrise.second)

        if current_time < sunrise_time:
            # And the sunrise for the current date is in the future. That means
            # we're some time between midnight and sunrise_time.
            seconds_until_sunrise = (sunrise_time - current_time).seconds
        else:
            # The sunrise for the current date has already been. Move onto the
            # one tomorrow
            sunrise_time += timedelta(days=1)
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

        current_time = datetime.now()

        sunset_time = datetime(current_time.year,
                               current_time.month,
                               current_time.day,
                               self._sunset.hour,
                               self._sunset.minute,
                               self._sunset.second)

        seconds_until_sunset = (sunset_time - current_time).seconds

        reactor.callLater(seconds_until_sunset,
                          self._stop_logging,
                          "scheduled sunset")

        log.msg("Sunset scheduled in {0} seconds time (at {1})".format(
                seconds_until_sunset, sunset_time))

    def _start_logging(self, trigger):

        if self._logging:
            # If we're not using daylight to start the logger we want to take
            # one picture when daylight is first detected by the weather
            # stations solar sensors:
            if not self._sunrise_image_taken and not self._daylight_trigger:
                self._sunrise_image_taken = True
                self._get_image()

            return  # Nothing to do

        log.msg("Starting logger: {0}".format(trigger))
        self._logging = True

        # TODO: option to clip interval to specific time-past-the-hour?
        # this would mean taking a snapshot now, calculating time between now
        # and when the next snapshot should occur, asking twisted to call us
        # again then and only then actually starting the logger proper.

        self._looper.start(self._interval * 60, True)

        if self._daylight_only and not self._daylight_trigger:
            self._schedule_logging_stop()
        # else the logger will be stopped when the associated weather station
        # stops detecting sunlight

    def _stop_logging(self, trigger):

        if not self._logging:
            return

        # Fetch one last image before we stop for the night
        self._get_image()

        log.msg("Stopping logger: {0}".format(trigger))
        self._logging = False
        self._sunrise_image_taken = False

        if self._looper.running:
            self._looper.stop()

        if self._daylight_only and not self._daylight_trigger:
            self._schedule_logging_start()
        # else the logger will be started when the associated weather station
        # detects sunlight

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

        if uv_index is None and solar_radiation is None:
            # Lost communication with the ISS? Ignore it.
            return
        elif uv_index > 0 or solar_radiation > 0:
            self._start_logging("sunrise detected")
        else:
            self._stop_logging("sunset detected")
