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

##############################################################################
### Database Configuration ###################################################
##############################################################################

# This is the connection string for the database containing data you wish
# to push out to the remote server. An example might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
### RabbitMQ Configuration ###################################################
##############################################################################

# Live data can be received via RabbitMQ instead of the database. This is only
# supported by the Davis data logger and requires the pika library to be
# installed. The main advantage of using RabbitMQ is it reduces the number of
# database writes from one every 2.5 seconds to one every five minutes or so.
# This is useful in cases where the database resides on flash storage which
# may wear out quickly (eg, an SD Card)

#mq_host="localhost"
#mq_port=5672

#mq_username="guest"
#mq_password="guest"

#mq_virtual_host="/"

#mq_exchange="weather"

##############################################################################
### Transport Configuration ##################################################
##############################################################################

# Transport type. Options are:
#  ssh  - connects to the weather server via the SSH protocol using a username
#         and password
#  udp  - Connects to the remote receiving service via UDP. This uses less
#         bandwidth (good for expensive internet connections such as 3G) but
#         may result in some live data updates getting lost.
transport_type = "ssh"

# Data encoding. Options are:
# standard      - less efficient but more reliable.
# diff          - Only includes fields in each update that have a different
#                 value from the previous update. This uses less data (good for
#                 expensive links) but when used over UDP it may increase the
#                 number of discarded live data updates.
encoding = "standard"

# This is the remote hostname or IP Address to connect to.
hostname = "localhost"

# The remote port to connect to.
port = 4222

##############################################################################
### SSH Configuration ########################################################
##############################################################################

# This only applies if you're using the ssh transport.

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

from zxw_push.zxw_push import getClientService
from twisted.application.service import Application, IProcess

# this is the core part of any tac file, the creation of the root-level
# application object
application = Application("weather_push")
IProcess(application).processName = "weather_push"

try:
    x_mq_hostname = mq_host
    x_mq_port = mq_port
    x_mq_exchange = mq_exchange
    x_mq_username = mq_username
    x_mq_password = mq_password
    x_mq_vhost = mq_virtual_host
except NameError:
    pass

# attach the service to its parent application
service = getClientService(
    hostname,
    port,
    username,
    password,
    host_key,
    dsn, transport_type, encoding, x_mq_host, x_mq_port, x_mq_exchange,
    x_mq_user, x_mq_password, x_mq_vhost)
service.setServiceParent(application)