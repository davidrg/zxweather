# coding=utf-8
##############################################################################
#   Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the weather push server
# service. You can start this service from within the weather_push directory
# by executing a command such as:
#   twistd -y weather_push_server.tac
#
# You should not make any changes to this file outside of the Application
# Configuration sections below.
#

##############################################################################
#   Database Configuration ###################################################
##############################################################################

# This is the connection string for the database containing data you wish
# to push out to the remote server. An example might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
#   Transport Configuration ##################################################
##############################################################################


# This is the network interface to listen on. Empty string is all interfaces.
interface = ""

# The UDP port to listen on
port = 4295

# The TCP port to listen on
tcp_port = 4296

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.

from zxw_push.zxw_push import getServerService
from twisted.application.service import Application, IProcess

# this is the core part of any tac file, the creation of the root-level
# application object
application = Application("weather_push_server")
IProcess(application).processName = "weather_push_server"



# attach the service to its parent application
service = getServerService(
    dsn, interface, port, tcp_port)
service.setServiceParent(application)
