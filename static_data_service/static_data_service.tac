# coding=utf-8
##############################################################################
### Instructions #############################################################
##############################################################################
#
# This is a twisted application configuration file for the static data service
# You can start this service from within the static_data_service directory
# by executing a command such as:
#   twistd -y static_data_service.tac
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
#   Output Configuration #####################################################
##############################################################################

output_directory = "/opt/zxweather/zxw_web/static/"

# Should we build static charts? (used by the Basic HTML interface)
build_charts = True

# What formats should they be generated in?
chart_formats = [
    'png',
    # 'gif'
]

# How often should the charts be updated? unit is samples - eg, every 6th sample
# rebuild the charts. With a 5 minute sample interval this would result in the
# charts being updated every 30 minutes
chart_interval = 6

# Gnuplot binary to use
gnuplot = "gnuplot"

##############################################################################
##############################################################################
##############################################################################
# Don't change anything below this point.
from static_data_service.service import StaticDataService
from twisted.application.service import Application, IProcess





application = Application("static-data-service")
IProcess(application).processName = "static-data-service"

service = StaticDataService(dsn, output_directory, build_charts, chart_formats,
                            chart_interval, gnuplot)

service.setServiceParent(application)
