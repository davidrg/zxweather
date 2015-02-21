# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the davis weather
# station data logger service. You can start this service from within the
# davis-logger directory by executing a command such as:
#   twistd -y davis_logger.tac
#
# You should not make any changes to this file outside of the Configuration
# sections below.
#
#
##############################################################################
#   Database Configuration ###################################################
##############################################################################

# This is the database connection string. An example one might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
### Message Broker Configuration #############################################
##############################################################################

# Using this feature requires the pika library to be installed

# The logger can broadcast live data to RabbitMQ. This is useful if you're
# running on something like a Raspberry Pi where you don't want to be writing
# a database transaction to the SD card every 2 seconds. See the manual for
# more details.

#mq_host = "localhost"
#mq_port = 5672
#mq_username = "guest"
#mq_password = "guest"
#mq_vhost = "/"

# Name of the exchange to broadcast to. This must be a topic exchange. Routing
# keys will be station_code.datatype (eg, rua2.live)
#mq_exchange = "weather"

##############################################################################
#   Station Configuration ####################################################
##############################################################################

# This is the code for the station to log data for.
station_code = "rua2"

##############################################################################
#   Serial Port Configuration ################################################
##############################################################################

# Serial port name. Eg, /def/ttyS0 (UNIX) or COM1 (Windows)
serial_port = '/dev/ttyS0'

# Serial port baud rate. 19200 is the default for the Davis Vantage Vue but
# this can be changed on the station console.
baud_rate = 19200

##############################################################################
#   Logging Configuration ####################################################
##############################################################################

# File to dump bad samples into
sample_error_file = "errors.csv"


##############################################################################
#   Daylight Savings Configuration ###########################################
##############################################################################

# Automatically adjust for daylight savings:
#   This setting causes the data logger to automatically adjust the weather
#   stations clock and your data for Daylight Savings. When daylight savings
#   changes it will:
#     * Fix the timestamps on any data in the weather stations memory to account
#       for daylight savings
#     * Adjust the daylight savings setting on the weather station so data
#       produced by the weather station in the future doesn't need fixing.
#   Use this setting if your station does not support daylight savings in your
#   location or does not do it correctly.
#
#   If you turn this setting on you should have your weather stations daylight
#   savings set to "Manual". You can change this setting directly on your
#   weather stations console or by using the set-clock utility.
#
#   This setting is incompatible with archive intervals greater than one hour.

auto_dst = False

# Time zone for daylight savings:
#   Where in the world are you? You can see a list of valid options on Wikipedia
#   at http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
time_zone = "Pacific/Auckland"

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from davis_logger.logger import DavisService
from twisted.application.service import Application, IProcess

application = Application("davisd")
IProcess(application).processName = "davisd"

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

service = DavisService(dsn, station_code, serial_port, baud_rate,
                       sample_error_file, auto_dst, time_zone, x_mq_hostname,
                       x_mq_port, x_mq_exchange, x_mq_username, x_mq_password,
                       x_mq_vhost)

service.setServiceParent(application)