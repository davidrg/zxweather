# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the time lapse logger
# service. You can start this service from within the image_logger directory
# by executing a command such as:
#   twistd -y time_lapse_logger.tac
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

# How often a new frame should be captured in seconds
capture_interval = 60

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
#   Data Processing Configuration ############################################
##############################################################################

# The directory where captured images will be stored and where the output video
# file should be placed
working_directory="/tmp/tlvlogger"

# A script that should turn the numbered images in the working directory into
# an MP4 video file. The default, omx_mp4.sh, will generate the video using
# hardware acceleration on a Raspberry Pi. A different script will be required
# on other platforms.
encoder_script="/opt/zxweather/time_lapse_logger/encoders/omx_mp4.sh"

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
                             x_mq_vhost, capture_interval, sunrise_time_t,
                             sunset_time_t, use_solar_sensors, camera_url,
                             image_source_code,
                             disable_ssl_certificate_verification,
                             encoder_script, working_directory)

service.setServiceParent(application)
