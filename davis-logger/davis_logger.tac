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
### Database Configuration ###################################################
##############################################################################

# This is the database connection string. An example one might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
### Station Configuration ####################################################
##############################################################################

# This is the code for the station to log data for.
station_code = "rua2"

##############################################################################
### Serial Port Configuration ################################################
##############################################################################

# Serial port name. Eg, /def/ttyS0 (UNIX) or COM1 (Windows)
serial_port = '/dev/ttyS0'

# Serial port baud rate. 19200 is the default for the Davis Vantage Vue but
# this can be changed on the station console.
baud_rate = 19200

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from davis_logger.logger import DavisService
from twisted.application.service import Application, IProcess

application = Application("davisd")
IProcess(application).processName = "davisd"

service = DavisService(dsn, station_code, serial_port, baud_rate)

service.setServiceParent(application)