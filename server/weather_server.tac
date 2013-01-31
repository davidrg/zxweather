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
##############################################################################
### Application Configuration ################################################
##############################################################################


# The port number the SSH service listens on
ssh_port = 4222

# The filenames for the SSH public and private key files you generated.
ssh_private_key_file = 'id_rsa'
ssh_public_key_file = 'id_rsa.pub'

# The name of the file storing username:password pairs.
ssh_passwords_file = 'ssh-passwords'

# This is the database connection string. An example one might be:
# dsn = "host=localhost port=5432 user=zxweather password=password dbname=weather"
dsn = ""

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from server.zxweatherd import getServerSSHService
from twisted.application.service import Application, IProcess


application = Application("zxweatherd")
IProcess(application).processName = "zxweatherd"

# attach the service to its parent application
service = getServerSSHService(
    ssh_port,
    ssh_private_key_file,
    ssh_public_key_file,
    ssh_passwords_file,
    dsn
)
service.setServiceParent(application)