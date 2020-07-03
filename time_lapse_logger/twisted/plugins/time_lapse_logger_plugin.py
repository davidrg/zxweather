# coding=utf-8
import ConfigParser
from zope.interface import implements
from datetime import datetime

from twisted.python import usage
from twisted.plugin import IPlugin
from twisted.application.service import IServiceMaker

from time_lapse_logger.service import TSLoggerService

__author__ = 'david'


class Options(usage.Options):
    optParameters = [
        ["config-file", "f", None, "Configuration file."],
    ]


class TSLoggerServiceMaker(object):
    """
    Creates the Time lapse Logger service. Or, rather, prepares it from the
    supplied command-line arguments.
    """
    implements(IServiceMaker, IPlugin)
    tapname = "time_lapse_logger"
    description = "Service for logging time-lapse videos from an IP Camera to the " \
                  "weather database"
    options = Options

    def _readConfigFile(self, filename):
        S_DATABASE = 'database'
        S_RABBITMQ = 'rabbitmq'
        S_SCHEDULE = 'schedule'
        S_CAMERA = 'camera'
        S_PROCESSING = 'processing'

        config = ConfigParser.ConfigParser()
        config.read([filename])

        dsn = config.get(S_DATABASE, 'dsn')

        mq_host = None
        mq_port = None
        mq_exchange = None
        mq_user = None
        mq_password = None
        mq_vhost = "/"

        if config.has_section(S_RABBITMQ):
            if config.has_option(S_RABBITMQ, "host") \
                    and config.has_option(S_RABBITMQ, "port"):
                mq_host = config.get(S_RABBITMQ, "host")
                mq_port = config.getint(S_RABBITMQ, "port")
                mq_user = config.get(S_RABBITMQ, "user")
                mq_password = config.get(S_RABBITMQ, "password")
                mq_exchange = config.get(S_RABBITMQ, "exchange")

            if config.has_option(S_RABBITMQ, "virtual_host"):
                mq_vhost = config.get(S_RABBITMQ, "virtual_host")

        capture_interval = config.getint(S_SCHEDULE, "capture_interval")
        sunrise_time = config.get(S_SCHEDULE, "sunrise_time")
        sunset_time = config.get(S_SCHEDULE, "sunset_time")
        use_solar_sensors = config.getboolean(S_SCHEDULE, "use_solar_sensors")
        station_code = config.get(S_SCHEDULE, "station_code")
        calculate_schedule = config.getboolean(S_SCHEDULE, "calculate_schedule")
        latitude = config.getfloat(S_SCHEDULE, "latitude")
        longitude = config.getfloat(S_SCHEDULE, "longitude")
        timezone = config.get(S_SCHEDULE, "timezone")
        elevation = config.getfloat(S_SCHEDULE, "elevation")
        sunrise_offset = config.getint(S_SCHEDULE, "sunrise_offset")
        sunset_offset = config.getint(S_SCHEDULE, "sunset_offset")

        sunrise_time_t = datetime.strptime(sunrise_time, "%H:%M").time()
        sunset_time_t = datetime.strptime(sunset_time, "%H:%M").time()

        camera_url = config.get(S_CAMERA, "camera_url")
        image_source_code = config.get(S_CAMERA, "image_source")
        disable_cert_verification = config.getboolean(
                S_CAMERA, "disable_ssl_certificate_verification")

        working_dir = config.get(S_CAMERA, "working_directory")

        save_frame_information = config.get(S_CAMERA, "save_frame_information")

        outputs = [x for x in config.sections() if x.startswith(S_PROCESSING)]

        output_configurations = []

        for output in outputs:
            vp = {
                "script": config.get(output, "encoder_script"),
                "backup_location": config.get(output, "backup_location"),
                "store_in_db": config.getboolean(output, "store_in_database"),
                "variant_name": None,
                "interval_multiplier": config.getint(output, "interval_multiplier"),
                "output_name": output[len(S_PROCESSING):],
                "title": config.get(output, "title"),
                "description": config.get(output, "description")
            }
            if config.has_option(output, "variant_name"):
                vp["variant_name"] = config.get(output, "variant_name")

            output_configurations.append(vp)

        if config.has_option(S_CAMERA, "archive_script"):
            archive_script = config.get(S_CAMERA, "archive_script")
        else:
            archive_script = None

        return dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, capture_interval, sunrise_time_t, \
            sunset_time_t, use_solar_sensors, station_code, camera_url, \
            image_source_code, disable_cert_verification, working_dir, \
            calculate_schedule, latitude, longitude, timezone, \
            elevation, sunrise_offset, sunset_offset, output_configurations, \
            save_frame_information, archive_script


    def makeService(self, options):
        """
        Construct the Image Logger Service.
        :param options: Dictionary of command-line options
        :type options: dict
        """

        dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, capture_interval, sunrise_time, \
            sunset_time, use_solar_sensors, station_code, camera_url, \
            image_source_code, disable_cert_verification,\
            working_dir, calculate_schedule, latitude, \
            longitude, timezone, elevation, sunrise_offset, \
            sunset_offset, output_configurations, save_frame_information, \
            archive_script \
            = self._readConfigFile(options['config-file'])

        svc = TSLoggerService(dsn, station_code, mq_host, mq_port,
                              mq_exchange, mq_user, mq_password, mq_vhost,
                              capture_interval, sunrise_time,
                              sunset_time, use_solar_sensors, camera_url,
                              image_source_code, disable_cert_verification,
                              working_dir, calculate_schedule,
                              latitude, longitude, timezone, elevation,
                              sunrise_offset, sunset_offset,
                              output_configurations, save_frame_information,
                              archive_script)

        # All OK. Go get the service.
        return svc


serviceMaker = TSLoggerServiceMaker()
