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

        sunrise_time_t = datetime.strptime(sunrise_time, "%H:%M").time()
        sunset_time_t = datetime.strptime(sunset_time, "%H:%M").time()

        camera_url = config.get(S_CAMERA, "camera_url")
        image_source_code = config.get(S_CAMERA, "image_source")
        disable_cert_verification = config.getboolean(
                S_CAMERA, "disable_ssl_certificate_verification")

        working_dir = config.get(S_PROCESSING, "working_directory")
        encoder_script = config.get(S_PROCESSING, "encoder_script")

        return dsn, mq_host, mq_port, mq_exchange, mq_user, mq_password, \
            mq_vhost, capture_interval, sunrise_time_t, \
            sunset_time_t, use_solar_sensors, station_code, camera_url, \
            image_source_code, disable_cert_verification, working_dir, \
            encoder_script

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
            working_dir, encoder_script = self._readConfigFile(
                options['config-file'])

        svc = TSLoggerService(dsn, station_code, mq_host, mq_port,
                              mq_exchange, mq_user, mq_password, mq_vhost,
                              capture_interval, sunrise_time,
                              sunset_time, use_solar_sensors, camera_url,
                              image_source_code, disable_cert_verification,
                              encoder_script, working_dir)

        # All OK. Go get the service.
        return svc


serviceMaker = TSLoggerServiceMaker()
