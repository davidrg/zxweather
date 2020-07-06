# Installation
At the moment installation is not easy and the process is largely undocumented. Full installation documentation will come eventually and until its available its probably not worth attempting if you're not familiar with Linux and Python. If htat hasn't discouraged you below are some notes on what you'll need to get started.

## Requirements
zxweather is tested and run against the following:
   - PostgreSQL 9.1 or higher   
   - Python 2.7 for everything else
   - Qt 4.8 and higher for the desktop application
   - wxtoimg for processing satellite images

Hardware requirements:
   - A compatbile weather station:
      - Davis Vantage Vue (with a USB or Serial data logger)
      - Davis Vantage Pro2, Pro2 Plus (wired or wireless, extra sensors are supported, USB or Serial data logger required)
      - FineOffset WH1080 or 100% compatible
      - Anything else if you can get the data into the database yourself
   - A camera that provides a URL for getting snapshots if you want to take scheduled snapshots or time-lapse videos
   - An RTL-SDR dongle and suitable antenna for downloading APT-encoded images from NOAA satellites.
   
   
## Components
| Component | Location | Technology | Description |
| ------ | ------ | ------ | ------ |
| Admin Tool | admin_tool | Python2.7 | Tool for creating and managing the weather database |
| Autosat | autosat | Python2.7 | For receiving APT-encoded images from NOAA satelites |
| Davis logger | davis-logger | Python2.7, twisted | Data logger for Davis Vantage Vue and Pro2 weather stations |
| Desktop app | desktop | C++, Qt | The desktop interface |
| Image logger | image_logger | Python2.7, twisted | Takes snapshots on a schedule and saves them to the database |
| Weatherplot | plot | Python2.7 | Intended to be run from cron, generates static png charts for the web interfaces basic HTML mode |
| server | server | Python2.7, twisted | Supplies current conditions updates to the web and desktop interfaces |
| Time-lapse logger | time_lapse_logger | Python2.7, twisted | Like the Image Logger but makes time-lapse videos |
| Weather Push | weather_push | Python2.7, twisted | For efficiently streaming weather data from one database to another over the internet |
| wh1080 tools | wh1080 | ANSI C | Data logger and related tools for FineOffset WH1080 weather stations |
| Web interface | zxw_web | Python2.7, webpy | The web interface |
   
   
## Installation Notes
At the moment installation is not easy. Proper setup documentation will come eventually and its probably best to wait for that but if you really want to have a go the admin tool is the place to start. Start with the following to create a new weather database and configure a first weather station:

```
$ cd /opt
$ git clone https://github.com/davidrg/zxweather.git
$ cd zxweather
$ python2.7 admin_tool/admin_too.py
```

To get the web interface going, copy /opt/zxweather/zxw_web/config.cfg.sample to /etc/zxweather/web.cfg and edit it to suit your setup. Make sure all the requirements in /opt/zxweather/zxw_web/requirements.txt are installed. Apache can then be configured to serve up the web interface with something like the following:
```
WSGIScriptAlias / /opt/zxweather/zxw_web/zxweather.py
<Directory /opt/zxweather/zxw_web>
        Require all granted
</Directory>
<Location "/data/">
        # Allow cross-origin requests to the data section
        Header set Access-Control-Allow-Origin "*"
</Location>
```

To support WebSockets in the server application. Make sure the requirements listed in its requirements.txt file (/opt/zxweather/server/requirements.txt) are installed. Copy /opt/zxweather/server/config.cfg.sample to /etc/zxweather/server.cfg and customise it as necessary. Then install the systemd unit file (/opt/zxweather/server/systemd.service) to start and run the websocket endpoint server. Make sure you update the web interfaces config file to tell it the hostname and ports the server is listening on.

The procedure for installing the other backend servics built using twisted is much the same.

To build the desktop app you'll need the following installed:
   - A Qt SDK installed. Some recent release like 5.12 is probably a good choice but it should work with any release from 4.8 to 5.13. 
   - If you want to connect it to the database directly instead of via the web interface you'll need the postgres development libraries and ECPG installed
   - The Qt QPSQL database driver built (the qt docs should cover how to do this if you don't have it already)

Once all thats satisified you should be able to just `qmake desktop.pro` to build it. 

Lastly, you'll need something to get weather data into the database. This involves either writing your own data logger or setting up one of the supplied ones (below)

### FineOffset data logger
If you're using a FineOffset weather station, the wh1080 directory contains the bits required. To build it you'll need:
   
   - GCC
   - Postgres bits: ECPG, libecpg, libpgtypes, libpq
   - libusb-1.0

If you've got all that you should be able to just run make inside the wh1080 directory to build it. [This old PDF from 2013](http://ftp2.zx.net.nz/pub/DGS/zxweather/v0.x/v0.2.0/wh1080-utilities-users-guide.pdf) might be helpful too - the wh1080 tools haven't changed much since then.

### Davis data logger
For Davis stations things are a bit easier. You need python 2.7 plus the requirements listed in its requirements.txt file installed. From there just copy /opt/zxweather/davis-logger/config.cfg.sample to /etc/zxweather/davis_logger.cfg, adjust it as necessary and install the systemd unit file (/opt/zxweather/davis-logger/zxweather_davis_logger.service)
