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
#   Database Configuration ###################################################
##############################################################################

# This is the database connection string. An example one might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
#   Message Broker Configuration #############################################
##############################################################################

# If your live data is broadcast by a RabbitMQ Message Broker, configure the
# message broker below:
enable_broker = False
broker_username = "guest"
broker_password = "guest"
broker_vhost = "/"
broker_hostname = "localhost"
broker_port = 5672
broker_exchange = "weather"

##############################################################################
#   SSH Protocol Configuration ###############################################
##############################################################################
# This protocol allows authenticated interactive read-write access to the
# server. It allows access to some administrative commands to list sessions,etc
# and also allows weather data to be uploaded via the weather-push service.

# If you want SSH Support, turn it on here:
enable_ssh = False

# The port number the SSH service listens on
ssh_port = 4222

# The filenames for the SSH public and private key files you generated.
ssh_private_key_file = 'id_rsa'
ssh_public_key_file = 'id_rsa.pub'

# The name of the file storing username:password pairs.
ssh_passwords_file = 'ssh-passwords'

##############################################################################
#   Telnet Protocol Configuration ############################################
##############################################################################
# This protocol allows interactive read-only access to the server to display
# current conditions and information about the weather station in a terminal
# window.

# If you want Telnet Support, turn it on here:
enable_telnet = True

# The port the Telnet service listens on
telnet_port = 4223

##############################################################################
#   Standard Protocol Configuration ##########################################
##############################################################################
# This protocol is used by the desktop client and anything else wanting access
# to weather data. In most cases it should be enabled.

# If this protocol should be enabled
enable_raw = True

# The port to listen on
raw_port = 4224

##############################################################################
#   WebSocket Protocol Configuration #########################################
##############################################################################
# This protocol is used by the web interface to access weather data.

# If this protocol should be enabled
enable_web_socket = True

# The port to listen on
web_socket_port = 81

# Hostname the websocket service will be available under
web_socket_hostname = 'server.example.com'


##############################################################################
#   WebSocket TLS Protocol Configuration #####################################
##############################################################################
# This a secure websocket endpoint using TLS. Its required if you're hosting
# the web UI over https:// (modern browsers won't allow an https site to connect
# to an http websocket).

# If this protocol should be enabled
enable_web_socket_tls = False

# The port to listen on
web_socket_tls_port = 443

# SSL Private Key
web_socket_tls_private_key_file = 'server.key'

# SSL Certificate
web_socket_tls_certificate_file = 'server.crt'

# SSL Certificate chain filename (file must be PEM encoded)
web_socket_tls_chain_file = None

# Hostname the websocket service will be available under
web_socket_tls_hostname = 'server.example.com'

# Set this to a long string that will be hard to guess or brute force if you
# want to enable online certificate reloading. Leave set to None to disable
# this feature (you'll have to restart the service if the certificates change)
web_socket_tls_certificate_reload_password = None

# With this password you can request the server reload its SSL certificate via
# a websocket connection with the ssl_reload command. For example, if your ssl
# reload password is "my-hard-to-guess-password" then the command would be:
#       ssl_reload my-hard-to-guess-password
# The easy way to do this is with the reload_certificates.py script:
#   reload_certificates.py wss://localhost:443/ my-hard-to-guess-password
# To make it work even if the certificate has expired you can add on this:
#   --no-ssl-validation
# To run this automatically whenever letsencrypt renews your certificate, create
# this file:
#    /etc/letsencrypt/renewal-hooks/deploy/reload_zxweatherd_cert.sh
# with the contents (assuming you've got zxweather in /opt/zxweather):
#   #!/bin/bash
#
#   cd /opt/zxweather/server
#   python2.7 reload_certificates.py wss://weather.zx.net.nz:444/ "41a9dc9e-ebe7-4b03-9f70-8e58076b522d" --no-ssl-validation
# And make it executable:
#   chmod +x /etc/letsencrypt/renewal-hooks/deploy/reload_zxweatherd_cert.sh


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
wss_config = None
rabbit_mq_config = None

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
    ws_config = {'port': web_socket_port, 'host': web_socket_hostname}

if enable_web_socket_tls:
    wss_config = {
        'port': web_socket_tls_port,
        'key': web_socket_tls_private_key_file,
        'certificate': web_socket_tls_certificate_file,
        'host': web_socket_tls_hostname,
        'chain': web_socket_tls_chain_file,
        'ssl_reload_password': web_socket_tls_certificate_reload_password
    }

if enable_broker:
    rabbit_mq_config = {
        'hostname': broker_hostname,
        'port': broker_port,
        'username': broker_username,
        'password': broker_password,
        'vhost': broker_vhost,
        'exchange': broker_exchange
    }


service = getServerService(
    dsn, ssh_config, telnet_config, raw_config, ws_config, wss_config,
    rabbit_mq_config)
service.setServiceParent(application)
