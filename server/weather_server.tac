# coding=utf-8
"""
Launches the weather server daemon. This allows clients to remotely access
data as well as data to be remotely loaded into the database.
"""

from server.zxweatherd import getServerService
from twisted.application.service import Application, IProcess


# this is the core part of any tac file, the creation of the root-level
# application object
application = Application("zxweatherd")
IProcess(application).processName = "zxweatherd"

# attach the service to its parent application
service = getServerService()
service.setServiceParent(application)