# coding=utf-8
import json
import os
from datetime import datetime, time, timedelta
import subprocess
from timeit import default_timer as timer

import pytz
from astral import Location
from twisted.application import service
from twisted.internet import task, reactor
from twisted.internet.defer import inlineCallbacks
from twisted.internet.ssl import ClientContextFactory
from twisted.python import log
from twisted.web.client import Agent, WebClientContextFactory
from twisted.web.http_headers import Headers
import dateutil.parser

from database import DatabaseReceiver, Database
from mq_receiver import RabbitMqReceiver
from readbody import readBody


# License: GPLv3 (Astral incompatible with GPLv2)


# TODO:
#  - Target file size option? Calculate a bit rate based on the number of frames and pass to the encoder
#  - option to store generated video on disk as well as database
#  - Handle restarting the database or message broker
#  - Optionally generate subtitle overlay with weather data
#  - Adjust weatherpush client:
#       - Don't throw exceptions if the video type code doesn't exist on the server

# Stuff to test
#  - Recovery from broker restart
#  - Recovery from database restart


# noinspection PyClassicStyleClass
class NoVerifyWebClientContextFactory(ClientContextFactory):
    def __init__(self):
        pass

    def getContext(self, hostname=None, port=None):
        return ClientContextFactory.getContext(self)


class TSLoggerService(service.Service):
    def __init__(self, dsn, station_code, mq_hostname, mq_port, mq_exchange,
                 mq_username, mq_password, mq_vhost, capture_interval,
                 sunrise_time, sunset_time,
                 use_solar_sensors, camera_url, image_source_code,
                 disable_cert_verification, video_script, working_dir,
                 calculate_schedule, latitude, longitude, timezone, elevation,
                 sunrise_offset, sunset_offset):
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
        :param video_script: Script to generate the video file
        :type video_script: str
        :param working_dir: Directory to store captured images and the generated video in
        :type working_dir: str
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
        """

        # Database connection for storing videos
        self._database = Database(dsn, image_source_code)

        # How often pictures should be taken
        self._interval = capture_interval

        # Fixed schedule
        self._fixed_sunrise = sunrise_time
        self._fixed_sunset = sunset_time

        self._daylight_trigger = use_solar_sensors
        self._calculated_schedule = calculate_schedule

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
        elif use_solar_sensors:
            # Schedule trigger by weather station sensors
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

        # Location to store captured pictures until we can assemble them into
        # a video
        self._working_dir = working_dir
        if not os.path.exists(self._working_dir):
            os.makedirs(self._working_dir)

        # Script to build the video
        self._mp4_script = video_script

        # Initialise other members
        self._logging = False
        self._looper = task.LoopingCall(self._get_image)
        self._logging_start_time = None
        self._current_image_number = 0

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

        if self._daylight_trigger and not self._calculated_schedule:
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

            if self.sunrise <= self.current_time <= self.sunset:
                self._start_logging("service start during daytime")
            else:
                log.msg("Logger will start at scheduled sunrise time.")
                self._schedule_logging_start()

    def stopService(self):
        service.Service.stopService(self)
        self._stop_logging("service stop")

    @inlineCallbacks
    def _get_image(self):
        log.msg("Obtaining image #{0:06}...".format(self._current_image_number))

        try:
            # TODO: Try sharing the factory to reduce log message spam
            if self._disable_cert_verification:
                context_factory = NoVerifyWebClientContextFactory()
            else:
                context_factory = WebClientContextFactory()
            agent = Agent(reactor, context_factory)

            response = yield agent.request(
                'GET',
                self._camera_url,
                Headers({'User-Agent': ['zxweather time-lapse-logger']}),
                None
            )

            response_data = yield readBody(response)

            if response_data is None or not len(response_data):
                raise Exception("Empty repsonse from camera")

            fn = os.path.join(self._working_dir, "{0:06}.jpg".format(self._current_image_number))
            with open(fn, 'wb') as f:
                f.write(response_data)

            log.msg("Image {0:06} captured.".format(self._current_image_number))

            self._current_image_number += 1

            # Update the record so we can recover from where we left off should the logger restart
            with open(os.path.join(self._working_dir, "info.json"), "w") as f:
                f.write(json.dumps({
                    "date": self.current_time.date().isoformat(),
                    "started": self._logging_start_time.isoformat(),
                    "next_image": self._current_image_number
                }))
        except Exception as e:
            log.msg("Failed to capture or store image #{1:06}: {0}".format(e.message, self._current_image_number))

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

        self._current_image_number = 0

        self._recover_run()

        self._looper.start(self._interval, True)

        if not self._daylight_trigger:
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
                log.msg("Ignoring false sunset at {0}".format(self.current_time))
                return

        log.msg("Stopping logger: {0}".format(trigger))
        self._logging = False

        if self._looper.running:
            self._looper.stop()

        if self._calculated_schedule or not self._daylight_trigger:
            self._schedule_logging_start()
        # else the logger will be started when the associated weather station
        # detects sunlight

        self._build_and_store_video()

    @inlineCallbacks
    def _build_and_store_video(self):
        finish_time = self.current_time

        title = "Time-lapse for {0}".format(self._logging_start_time.date())
        description = "Time-lapse from {0} to {1}".format(
            self._logging_start_time.time().strftime("%H:%M"),
            finish_time.time().strftime("%H:%M")
        )

        input_size = sum(os.path.getsize(f) for f in os.listdir(self._working_dir) if os.path.isfile(f))

        dest_file = "output.mp4"

        start = timer()

        command = '{0} "{1}" "{2}" "{3}" "{4}"'.format(
            self._mp4_script, self._working_dir, dest_file, title, description)
        log.msg("Encoding video with command: {0}".format(command))

        # generate the video file using the generator script
        result = subprocess.call(command, shell=True)

        processing_time = timer() - start

        if result != 0:
            log.msg("** ERROR: video script fails")
            self.stopService()

        metadata = {
            "start": self._logging_start_time.isoformat(),
            "finish": finish_time.isoformat(),
            "processing_time": processing_time,
            "frame_count": self._current_image_number - 1,
            "interval": self._interval,
            "total_size": input_size
        }

        with open(os.path.join(self._working_dir, dest_file), 'rb') as f:
            video_data = f.read()

        # Store video in the database
        yield self._database.store_video(self.current_time, video_data,
                                         'video/mp4', json.dumps(metadata),
                                         title, description)
        log.msg("Video stored.")

    def _recover_run(self):

        try:
            with open(os.path.join(self._working_dir, "info.json"), "r") as f:
                info = json.loads(f.read())

            folder_date = dateutil.parser.parse(info["date"])
            folder_time = dateutil.parser.parse(info["started"])
            next_image = info["next_image"]

            log.msg("Found data for {0}".format(folder_date.date()))

            if folder_date.date() != self._logging_start_time.date():
                # The working directory contains data left over from a previous
                # run
                self._empty_working_dir()
            else:
                # The previous run was interrupted before it was supposed to
                # finish. We'll recover from it
                log.msg("Working directory contains data for current date. Recovering...")
                self._current_image_number = next_image
                self._logging_start_time = folder_time
                log.msg("Now at image {0}, logging from {1}".format(
                    self._current_image_number, self._logging_start_time))

        except:
            self._empty_working_dir()

    def _empty_working_dir(self):
        """
        Deletes all files within the working directory
        """

        for the_file in os.listdir(self._working_dir):
            file_path = os.path.join(self._working_dir, the_file)

            if os.path.isfile(file_path):
                os.unlink(file_path)

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
