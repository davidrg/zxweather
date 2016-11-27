# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the image logger service.
# You can start this service from within the image_logger directory by
# executing a command such as:
#   twistd -y image_logger.tac
#
# You should not make any changes to this file outside of the Configuration
# sections below.
#
#
##############################################################################
#   Database Configuration ###################################################
##############################################################################

# This is the database connection string. An example one might be:
dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"

##############################################################################
#   Message Broker Configuration #############################################
##############################################################################

# If you have a Davis Vantage Pro2 Plus weather station, the schedule can be
# started and stopped by your weather stations solar sensors. If you broadcast
# weather data via a RabbitMQ message broker rather than the database you can
# configure the message broker below. When enabled, live data will be received
# from RabbitMQ only - any live data broadcast via the database will be ignored.

# Using this feature requires the pika library to be installed
#mq_host = "localhost"
#mq_port = 5672
#mq_username = "guest"
#mq_password = "guest"
#mq_vhost = "/"

# Name of the exchange to bind to. This must be a topic exchange.
#mq_exchange = "weather"

##############################################################################
#   Schedule Configuration ###################################################
##############################################################################

# Here you need to configure two things:
#   -> How often photos should be taken (the capture interval).
#   -> The schedule: what time capturing should start, and what time capturing
#      should finish.
#
# There are three schedule options:
#  -> Fixed: you specify the sunrise and sunset times. They do not vary during
#     the year unless you change them.
#  -> Automatic based on solar sensors: If you've got a Davis Vantage Pro 2 Plus
#     weather station its Solar Radiation sensor can be used to detect sunrise
#     and sunset. In practice the schedule will likely start a little while
#     after civil dawn and a little while before civil dusk.
#  -> Automatic based on location: Calculate sunrise and sunset based on your
#     coordinates (longitude, latitude, elevation) and time zone. The schedule
#     will begin at civil dawn and end at civil dusk.

# How often images should be captured in minutes
capture_interval = 60

# Set this to true if you'd rather not capture images of the night sky. If your
# camera just produces black images at night you may as well turn this on to
# prevent those from being captured. When turned off images will be captured
# through the night and the other settings in this section will be ignored.
capture_during_daylight_only = True

# What time the sun rises and sets. If the above setting is turned on images
# will only be captured between these times. The first image will be taken
# at sunrise.
sunrise_time = "06:00"
sunset_time = "20:00"

# If your weather station is a Davis Vantage Pro2 Plus you can turn this setting
# on and use the weather stations solar sensors to automatically detect sunrise
# and sunset. This overrides the sunrise_time/sunset_time settings above.
use_solar_sensors = False

# The station that should be used for sunrise detection
station_code = "abc"

# When running off a fixed or calculated schedule (rather than triggered by
# solar sensors) set this option to take an extra picture when the specified
# weather stations detect sunlight for the first time in the morning. This
# setting has no effect when use_solar_sensors is True
take_detected_sunrise_picture = False

# Alternatively, sunrise and sunset can be calculated based on your location.
# This setting overrides solar sensors and the fixed sunrise/sunset time:
calculate_schedule = True

# These values are required when calculating sunrise and sunset based on your
# location.
latitude = -36.860790
longitude = 174.777887
timezone = "Pacific/Auckland"
elevation = 0

# You can optionally apply an offset (in minutes) to the calculated sunrise
# and sunset times.
sunrise_offset = 0
sunset_offset = 0


##############################################################################
#   Camera Configuration #####################################################
##############################################################################

# Only IP cameras which provide an HTTP interface are supported. Enter the URL
# for your camera that produces an image below:
camera_url = "https://10.0.1.206/snapshot.cgi?chan=0"

# The Image Source that images should be logged against
image_source_code = "cam01"

# You can turn this on if security isn't a concern and your camera has an SSL
# certificate that can't be verified.
disable_ssl_certificate_verification = True

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from image_logger.service import ImageLoggerService
from twisted.application.service import Application, IProcess
from datetime import datetime

x_mq_hostname = None
x_mq_port = None
x_mq_exchange = None
x_mq_username = None
x_mq_password = None
x_mq_vhost = None

try:
    x_mq_hostname = mq_host
    x_mq_port = mq_port
    x_mq_exchange = mq_exchange
    x_mq_username = mq_username
    x_mq_password = mq_password
    x_mq_vhost = mq_vhost
except NameError:
    pass

sunrise_time_t = datetime.strptime(sunrise_time, "%H:%M").time()
sunset_time_t = datetime.strptime(sunset_time, "%H:%M").time()

application = Application("imagelogger")
IProcess(application).processName = "imagelogger"

service = ImageLoggerService(dsn, station_code, x_mq_hostname, x_mq_port,
                             x_mq_exchange, x_mq_username, x_mq_password,
                             x_mq_vhost, capture_interval,
                             capture_during_daylight_only,
                             sunrise_time_t,
                             sunset_time_t, use_solar_sensors, camera_url,
                             image_source_code,
                             disable_ssl_certificate_verification,
                             calculate_schedule, latitude,
                             longitude, timezone, elevation, sunrise_offset,
                             sunset_offset, take_detected_sunrise_picture)

service.setServiceParent(application)
