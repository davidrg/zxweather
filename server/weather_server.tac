# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the zxweatherd
# (weather server) service. You can start this service from within the
# server directory by executing a command such as:
#   twistd -y weather_server.tac
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
### SSH Protocol Configuration ###############################################
##############################################################################
# This protocol allows authenticated interactive read-write access to the
# server. It allows access to some administrative commands to list sessions,etc
# and also allows weather data to be uploaded. This protocol must be enabled
# for the weather-push service.

# If you want SSH Support, turn it on here:
enable_ssh = True

# The port number the SSH service listens on
ssh_port = 4222

# The filenames for the SSH public and private key files you generated.
ssh_private_key_file = 'id_rsa'
ssh_public_key_file = 'id_rsa.pub'

# The name of the file storing username:password pairs.
ssh_passwords_file = 'ssh-passwords'

##############################################################################
### Telnet Protocol Configuration ############################################
##############################################################################
# This protocol allows interactive read-only access to the server to display
# current conditions and information about the weather station in a terminal
# window.

# If you want Telnet Support, turn it on here:
enable_telnet = True

# The port the Telnet service listens on
telnet_port = 4223

##############################################################################
### Standard Protocol Configuration ##########################################
##############################################################################
# This protocol is used by the desktop client and anything else wanting access
# to weather data. In most cases it should be enabled.

# If this protocol should be enabled
enable_raw = True

# The port to listen on
raw_port = 4224

##############################################################################
### WebSocket Protocol Configuration #########################################
##############################################################################
# This protocol is used by the web interface to access weather data.

# If this protocol should be enabled
enable_web_socket = True

# The port to listen on
web_socket_port = 81

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from server.zxweatherd import getServerService
from twisted.application.service import Application, IProcess

application = Application("zxweatherd")
IProcess(application).processName = "zxweatherd"

ssh_config = None
telnet_config = None
raw_config = None
ws_config = None

if enable_ssh:
    ssh_config = {
        'port': ssh_port,
        'private_key_file': ssh_private_key_file,
        'public_key_file': ssh_public_key_file,
        'passwords_file': ssh_passwords_file
    }

if enable_telnet:
    telnet_config = {'port': telnet_port}

if enable_raw:
    raw_config = {'port': raw_port}

if enable_web_socket:
    ws_config = {'port': web_socket_port}

service = getServerService(
    dsn, ssh_config, telnet_config, raw_config, ws_config)
service.setServiceParent(application)