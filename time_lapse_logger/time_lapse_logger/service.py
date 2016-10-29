# coding=utf-8
import json
import os
from datetime import datetime, time, timedelta
import subprocess
from timeit import default_timer as timer

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


# TODO:
#  - Target file size option? Calculate a bit rate based on the number of frames and pass to the encoder
#  - Support calculated sunrise time with an optional offset (to, eg, start capturing 10 minutes before sunrise
#    and 10 minutes after sunset)
#  - option to store generated video on disk as well as database
#  - Come up with another encoder script that doesn't require a Pi
#  - Handle restarting the database or message broker
#  - Optionally generate subtitle overlay with weather data
#  - Adjust weatherpush client:
#       - Don't throw exceptions if the video type code doesn't exist on the server
#  - Adjust web UI:
#       - Use video tags for videos

# Stuff to test
#  - Run recovery
#  - Database storage
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
                 disable_cert_verification, video_script, working_dir):
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
        """

        self._mq_receiver = RabbitMqReceiver(mq_username, mq_password, mq_vhost,
                                             mq_hostname, mq_port, mq_exchange,
                                             station_code)

        self._db_receiver = DatabaseReceiver(dsn, station_code)

        self._database = Database(dsn, image_source_code)

        self._mq_receiver.LiveUpdate += self._live_data_received
        self._db_receiver.LiveUpdate += self._live_data_received

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

        self._camera_url = camera_url

        self._logging_start_time = None

        self._working_dir = working_dir

        if not os.path.exists(self._working_dir):
            os.makedirs(self._working_dir)

        self._current_image_number = 0

        self._mp4_script = video_script

    def startService(self):
        service.Service.startService(self)

        self._database.connect()

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
                    "date": datetime.now().date().isoformat(),
                    "started": self._logging_start_time.isoformat(),
                    "next_image": self._current_image_number
                }))
        except Exception as e:
            log.msg("Failed to capture or store image #{1:06}: {0}".format(e.message, self._current_image_number))

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
            return  # Nothing to do

        log.msg("Starting logger: {0}".format(trigger))
        self._logging = True

        self._logging_start_time = datetime.now()

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
            if t >= datetime.now():
                # The logger started less than an hour ago - this is probably
                # just a cloud passing across the sunrise. Ignore it.
                log.msg("Ignoring false sunset at {0}".format(datetime.now()))
                return

        log.msg("Stopping logger: {0}".format(trigger))
        self._logging = False

        if self._looper.running:
            self._looper.stop()

        if not self._daylight_trigger:
            self._schedule_logging_start()
        # else the logger will be started when the associated weather station
        # detects sunlight

        self._build_and_store_video()

    @inlineCallbacks
    def _build_and_store_video(self):
        finish_time = datetime.now()

        title = "Time-lapse for {0}".format(self._logging_start_time.date())
        description = "Time-lapse from {0} to {1}".format(
            self._logging_start_time.time().strftime("%H:%M"),
            finish_time.time().strftime("%H:%M")
        )

        input_size = sum(os.path.getsize(f) for f in os.listdir(self._working_dir) if os.path.isfile(f))

        dest_file = "output.mp4"

        start = timer()

        # generate the video file using the generator script
        result = subprocess.call(['{0} "{1}" "{2}" "{3}" "{4}"'.format(self._mp4_script, self._working_dir, dest_file, title, description)], shell=True)

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
            video_data = f.readall()

        # Store video in the database
        yield self._database.store_video(datetime.now(), video_data, 'video/mp4', json.dumps(metadata), title,
                                         description)

    def _recover_run(self):

        try:
            with open(os.path.join(self._working_dir, "info.json"), "r") as f:
                info = json.loads(f.readall())

            folder_date = dateutil.parser.parse(info["date"])
            folder_time = dateutil.parser.parse(info["started"])
            next_image = info["next_image"]

            if folder_date != self._logging_start_time.date():
                # The working directory contains data left over from a previous run
                self._empty_working_dir()
            else:
                # The previous run was interrupted before it was supposed to finish. We'll recover from it
                log.msg("Working directory contains data for current date. Recovering.")
                self._current_image_number = next_image
                self._logging_start_time = folder_time

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
