# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the weather_push
# service. You can start this service from within the weather_push directory
# by executing a command such as:
#   twistd -y weather_push.tac
#
# You should not make any changes to this file outside of the Application
# Configuration section below.
#
# Alternatively, you can launch the weather service by executing a command
# such as the following supplying all settings on the command-line:
#   twistd weather_push -h hostname -p 22 -u admin -a password
#
#
##############################################################################
### Application Configuration ################################################
##############################################################################

# This is the remote hostname or IP Address to connect to
hostname = "localhost"

# The port number the remote weather servers SSH service is listening on
port = 4222


# The username and password to login with
username = "admin"
password = ""

# The remote servers SSH Host Key. You would look something like:
# host_key = '03:ab:56:b4:92:66:01:3d:0d:53:3f:fe:0a:80:5e:35'
# If you don't specify one (leaving it as the default None below) then the
# remote hosts key will not be validated.
host_key = None

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.

from zxw_push.zxw_push import getPushService
from twisted.application.service import Application, IProcess

# this is the core part of any tac file, the creation of the root-level
# application object
application = Application("weather_push")
IProcess(application).processName = "weather_push"

# attach the service to its parent application
service = getPushService(
    hostname,
    port,
    username,
    password,
    host_key)
service.setServiceParent(application)