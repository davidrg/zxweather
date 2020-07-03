# coding=utf-8
import errno
import json
import os
import shutil
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
from util import Event


# License: GPLv3 (Astral incompatible with GPLv2)


# TODO:
#  - Target file size option? Calculate a bit rate based on the number of
#    frames and pass to the encoder
#  - Handle restarting the message broker
#  - Optionally generate subtitle overlay with weather data
#  - Stuff to test
#    - Recovery from broker restart
#  - Allow for additional metadata to be stored from the config file?
#  - Any other metadata we might want to store beyond what we have now?
#  - Option to log weather data for duration of video and include in metadata?
#               This would have to be Davis Sample equivalents generated from live data
#               and aggregated into whatever the frame interval is:
#               15s - 6 live updates
#               30s - 12 live updates
#               60s - 24 live updates
#  - Include detected sunrise and sunset times in metadata?
#  - Include calculated sunrise and sunset times in metadata?

# noinspection PyClassicStyleClass
class NoVerifyWebClientContextFactory(ClientContextFactory):
    def __init__(self):
        pass

    def getContext(self, hostname=None, port=None):
        return ClientContextFactory.getContext(self)


class VideoProcessor(object):
    def __init__(self, working_directory, encoder_script, backup_location,
                 store_in_database, database, image_source_code, enabled,
                 variant_name, output_name, interval_multiplier, title,
                 description):

        self.stopService = Event()
        self.reconnectDatabase = Event()

        self._backup_location = backup_location
        self._database = None
        if store_in_database:
            self._database = database
        self._redirect_videos_to_disk = not store_in_database
        self._mp4_script = encoder_script
        self._working_dir = working_directory
        self._image_source_code = image_source_code
        self._enabled = enabled

        self._variant_name = variant_name
        self._output_name = output_name
        self._interval_multiplier = interval_multiplier
        self._title = title
        self._description = description

        # TODO: This produces no output because the log doesn't
        #   start until the service starts
        if enabled:
            log.msg("Output '{0}' enabled with interval multiplier {1}x ".format(
                    output_name, interval_multiplier))
        else:
            log.msg("Output '{0}' is disabled".format(output_name))

    @property
    def enabled(self):
        return self._enabled

    @property
    def output(self):
        return self._output_name

    @staticmethod
    def _select_inputs(working_dir, input_path, multiplier, frame_count):
        # Symlink (or copy if windows) every nth image into the input path
        # The resulting directory should be a smaller set of consecutively
        # numbered images
        # eg, with a multiplier of 2:
        #    working_dir            input_path
        #    001.jpg   --------->   001.jpg
        #    002.jpg       /---->   002.jpg
        #    003.jpg   ---/  /-->   003.jpg
        #    004.jpg        /
        #    005.jpg   ----/

        log.msg("Selecting inputs for multiplier {0}, output to {1}".format(multiplier, input_path))

        if not os.path.exists(input_path):
            os.makedirs(input_path)

        # First up, these are the frames we've got to work with
        available_images = ["{0:06}.jpg".format(frame_number)
                            for frame_number in range(0, frame_count+1)]

        selected_images = []
        i = 0
        next_id = 0
        while i < len(available_images):
            if i == next_id:
                selected_images.append(available_images[i])
                next_id += multiplier
            i += 1

        # Empty target directory
        for f in os.listdir(input_path):
            file_path = os.path.join(input_path, f)
            if os.path.isfile(file_path):
                os.unlink(file_path)

        # Now symlink (or copy) the selected images into the input path for
        # encoding.

        i = 0
        if hasattr(os, "symlink"):
            wd = os.getcwd()
            os.chdir(input_path)
            for image in selected_images:
                relative_path = os.path.join("../", image)
                os.symlink(relative_path, "{0:06}.jpg".format(i))
                i += 1
            os.chdir(wd)
        else:
            for image in selected_images:
                shutil.copy(os.path.join(working_dir, image),
                            os.path.join(input_path, "{0:06}.jpg".format(i)))
                i += 1
        return len(selected_images)

    def build_and_store_video(self, current_time, logging_start_time, interval,
                              frame_count):

        log.msg("Building video for output '{0}', multiplier {1}, store to db ".format(
                self._output_name, self._interval_multiplier, not self._redirect_videos_to_disk))

        finish_time = current_time
        mime_type = "video/mp4"

        metadata_parameters = {
            "date": logging_start_time.date(),
            "start_time": logging_start_time.time().strftime("%H:%M"),
            "end_time": finish_time.time().strftime("%H:%M")
        }

        title = self._title.format(**metadata_parameters)
        description = self._description.format(**metadata_parameters)

        input_path = self._working_dir
        if self._interval_multiplier > 1:
            if self._output_name is None or self._output_name == '':
                input_path = os.path.join(self._working_dir, 'default')
            else:
                input_path = os.path.join(self._working_dir, self._output_name)

            # Prepare a new encoder input directory with a reduced number of frames
            # and get a new frame count.
            frame_count = self._select_inputs(self._working_dir, input_path,
                                              self._interval_multiplier, frame_count)

        input_size = sum(os.path.getsize(os.path.join(input_path, f))
                         for f in os.listdir(input_path)
                         if os.path.isfile(os.path.join(input_path, f)))

        dest_file = "output{name}.mp4".format(name=self._output_name)

        start = timer()

        command = '{0} "{1}" "{2}" "{3}" "{4}"'.format(
            self._mp4_script, input_path, dest_file, title, description)
        log.msg("Encoding video {1} with command: {0}".format(command, dest_file))

        # generate the video file using the generator script
        result = subprocess.call(command, shell=True)

        processing_time = timer() - start

        if result != 0:
            log.msg("** ERROR: video script fails")
            self.stopService.fire()
            return timer() - start

        metadata = {
            "start": logging_start_time.isoformat(),
            "finish": finish_time.isoformat(),
            "processing_time": processing_time,
            "frame_count": frame_count + 1,  # +1 to account for number 0
            "base_interval": interval,
            "interval": interval * self._interval_multiplier,
            "total_size": input_size,
            "variant": self._variant_name
        }

        with open(os.path.join(input_path, dest_file), 'rb') as f:
            video_data = f.read()

        log.msg("Video is {0} bytes long".format(len(video_data)))

        if self._redirect_videos_to_disk:
            # Store video on disk in the backup location only
            self._store_video_in_backup_location(
                finish_time, metadata, video_data, mime_type, title,
                description)
        else:
            self._store_video_in_database(finish_time, metadata, video_data,
                                          mime_type, title, description,
                                          current_time)
        processing_time = timer() - start
        return processing_time

    @inlineCallbacks
    def _store_video_in_database(self, finish_time, metadata, video_data,
                                 mime_type, title, description, current_time):
        log.msg("Writing video to database.")
        try:
            yield self._database.store_video(current_time, video_data,
                                             'video/mp4', json.dumps(metadata),
                                             title, description)
            log.msg("Video stored.")
        except:
            log.msg("Possible database connection problem. "
                    "Attempting to reconnect...")
            try:
                self.reconnectDatabase.fire()

                yield self._database.store_video(finish_time, video_data,
                                                 mime_type,
                                                 json.dumps(metadata),
                                                 title, description)
                log.msg("Database reconnected and video stored.")
            except Exception as e:
                log.msg("Reconnect failed or other problem storing video.")

                self._store_video_in_backup_location(
                    finish_time, metadata, video_data, mime_type, title,
                    description)

                log.msg("Rethrowing exception...")
                raise e

    def _store_video_in_backup_location(self, finish_time, metadata, video_data,
                                        mime_type, title, description):
        log.msg("Storing video in backup location")
        if not os.path.exists(self._backup_location):
            os.makedirs(self._backup_location)

        filename_part = finish_time.strftime("%Y_%m_%d_%H_%M_%S")

        if self._output_name is not None and self._output_name != '':
            filename_part = self._output_name + '_' + filename_part

        metadata_file = os.path.join(self._backup_location,
                                     filename_part + ".json")
        db_info_file = os.path.join(self._backup_location,
                                    filename_part + "_info.json")
        video_file = os.path.join(self._backup_location,
                                  filename_part + ".mp4")
        with open(metadata_file, 'w') as f:
            f.write(json.dumps(metadata))
        with open(video_file, 'wb') as f:
            f.write(video_data)
        with open(db_info_file, 'w') as f:
            info = {
                "time": finish_time.isoformat(),
                "mime": mime_type,
                "title": title,
                "description": description,
                "source": self._image_source_code,
                "type": "TLVID",
                "metadata": metadata_file,
                "video": video_file
            }
            f.write(json.dumps(info))
            f.flush()

        log.msg("Video copied to backup location.\n"
                "Video file: {0}\nMetadata file: {1}\nTitle: {2}\n"
                "Description: {3}\nSize: {4} bytes".format(
                    video_file, metadata_file, title, description, len(video_data)))


class TSLoggerService(service.Service):
    def __init__(self, dsn, station_code, mq_hostname, mq_port, mq_exchange,
                 mq_username, mq_password, mq_vhost, capture_interval,
                 sunrise_time, sunset_time,
                 use_solar_sensors, camera_url, image_source_code,
                 disable_cert_verification, working_dir,
                 calculate_schedule, latitude, longitude, timezone, elevation,
                 sunrise_offset, sunset_offset, output_configurations,
                 store_frame_info, archive_script):
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
        :param working_dir: Directory to store captured images and the generated
        video in
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
        :param output_configurations: Output configuration data
        """

        self._agent = None

        self._archive_script = archive_script

        store_in_database = False
        for c in output_configurations:
            store_in_database = c["store_in_db"]
            if store_in_database:
                break

        # Database connection for storing videos
        if store_in_database or use_solar_sensors:
            self._database = Database(dsn, image_source_code)
        else:
            self._database = None

        self._store_frame_info = store_frame_info

        # How often pictures should be taken
        self._interval = capture_interval

        # Fixed schedule
        self._fixed_sunrise = sunrise_time
        self._fixed_sunset = sunset_time

        self._daylight_trigger = use_solar_sensors
        self._calculated_schedule = calculate_schedule

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

        # Initialise other members
        self._logging = False
        self._looper = task.LoopingCall(self._get_image)
        self._logging_start_time = None
        self._current_image_number = 0

        def _stop_service():
            self.stopService()

        def _reconnect_db():
            self._database.reconnect()
            if self._db_receiver is not None:
                self._db_receiver.reconnect()

        self._image_source_code = image_source_code
        self._video_processors = []
        for c in output_configurations:
            self._video_processors.append(VideoProcessor(
                self._working_dir, c["script"], c["backup_location"],
                c["store_in_db"], self._database, image_source_code, True,
                c["variant_name"], c["output_name"], c["interval_multiplier"],
                c["title"], c["description"]))

        for vp in self._video_processors:
            vp.stopService += _stop_service
            vp.reconnectDatabase += _reconnect_db

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

        log.msg("Service start")

        if self._database is not None:
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
        self._stop_logging("service stop", False)

    @inlineCallbacks
    def _get_image(self):
        log.msg("Obtaining image #{0:06}...".format(self._current_image_number))

        try:
            # Share the agent to reduce log message spam
            # TODO: check this works
            if self._agent is None:
                if self._disable_cert_verification:
                    context_factory = NoVerifyWebClientContextFactory()
                else:
                    context_factory = WebClientContextFactory()
                self._agent = Agent(reactor, context_factory)

            response = yield self._agent.request(
                'GET',
                self._camera_url,
                Headers({'User-Agent': ['zxweather time-lapse-logger']}),
                None
            )

            response_data = yield readBody(response)
            response_time = self.current_time

            if response_data is None or not len(response_data):
                raise Exception("Empty repsonse from camera")

            fn = os.path.join(self._working_dir, "{0:06}.jpg".format(self._current_image_number))
            with open(fn, 'wb') as f:
                f.write(response_data)

            # Optionally store some metadata for the frame so it can be later
            # inserted into the database by the user if it has something
            # interesting in it.
            if self._store_frame_info:
                fi_fn = os.path.join(self._working_dir, "{0:06}.json".format(self._current_image_number))
                with open(fi_fn, "w") as f:
                    f.write(json.dumps({
                        "time": response_time.isoformat(),
                        "mime": "image/jpeg",
                        "title": None,
                        "description": "Time lapse frame {0:06}".format(self._current_image_number),
                        "source": self._image_source_code,
                        "type": "CAM",
                        "metadata": None,
                        "image": "{0:06}.jpg".format(self._current_image_number)
                    }))

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

    def _stop_logging(self, trigger, produce_outputs=True):

        if not self._logging:
            return

        # If we're triggered by daylight don't stop logging within 60 minutes
        # of starting logging. This should prevent clouds in the early morning
        # or late evening from triggering a stop before actual sunset.
        # For other scheduling modes (calculated or fixed) we'll follow the
        # schedule.
        if self._logging_start_time is not None and self._daylight_trigger:
            t = self._logging_start_time + timedelta(minutes=60)
            if t >= self.current_time:
                # The logger started less than an hour ago - this is probably
                # just a cloud passing across the sunrise. Ignore it.
                log.msg("Ignoring false sunset at {0}".format(
                    self.current_time))
                return

        log.msg("Stopping logger: {0}".format(trigger))
        self._logging = False

        if self._looper.running:
            self._looper.stop()

        if self._calculated_schedule or not self._daylight_trigger:
            self._schedule_logging_start()
        # else the logger will be started when the associated weather station
        # detects sunlight

        if produce_outputs:
            self._build_and_store_video()

    def _build_and_store_video(self):
        for vp in self._video_processors:
            if vp.enabled:
                log.msg("Processing for output: " + vp.output)
                pt = vp.build_and_store_video(self.current_time,
                                              self._logging_start_time,
                                              self._interval,
                                              self._current_image_number - 1)
                log.msg("Processed output in {0} seconds".format(pt))
            else:
                log.msg("Skipping output {0} - not enabled".format(vp.output))

        log.msg("All video processing completed.")

        # If the user wants to keep a copy of the input frames, take that copy
        # now.
        if self._archive_script is not None:
            command = '{0} "{1}" "{2}"'.format(
                self._archive_script,
                self._working_dir,
                self._logging_start_time.date())

            log.msg("Running archive script: {0}".format(command))

            start = timer()
            result = subprocess.call(command, shell=True)
            processing_time = timer() - start

            if result != 0:
                log.msg("** ERROR: archive script fails")
            else:
                log.msg("Archive script completed after {0}".format(
                    processing_time))

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
                log.msg("Working directory contains data for current date. "
                        "Recovering...")
                self._current_image_number = next_image
                self._logging_start_time = folder_time
                log.msg("Now at image {0}, logging from {1}".format(
                    self._current_image_number, self._logging_start_time))

        except Exception as e:
            # Something went wrong trying to recover the run. Abandon it and
            # start again.
            log.msg(e)
            self._empty_working_dir()

    @staticmethod
    def mkdir_p(path):
        try:
            os.makedirs(path)
        except OSError as exc:  # Python ≥ 2.5
            if exc.errno == errno.EEXIST and os.path.isdir(path):
                pass
            else:
                raise

    def _empty_working_dir(self):
        """
        Deletes all files within the working directory
        """

        # Ensure the working directory actually exists
        self.mkdir_p(self._working_dir)

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
