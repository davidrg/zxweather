# coding=utf-8
"""
Autosat configuration data

zxweather autosat
(C) Copyright David Goodwin 2017

License: GNU GPL v2+
"""

import os
from ConfigParser import ConfigParser


class Satellite(object):
    def __init__(self, name, frequency, bandwidth, decoder):
        self.name = name
        self.frequency = frequency
        self.bandwidth = bandwidth
        self.decoder = decoder


class Settings(object):

    APT = "apt_decoder"
    OUTPUT = "output"
    STATION = "station"
    TLE = "tle"
    LOGGING = "logging"
    RECORDING = "recording"
    ZXWEATHER = "zxweather"

    def __init__(self):
        parser = ConfigParser()
        parser.read(["config.cfg", "/etc/zxweather/autosat.cfg"])

        #
        # APT Settings
        #
        self.wxtoimg_path = parser.get(Settings.APT, "wxtoimg_path")
        self.quiet = parser.getboolean(Settings.APT, "quiet")
        self.text_overlay_enabled = parser.getboolean(Settings.APT,
                                                      "text_overlay_enabled")
        self.text_overlay = parser.get(Settings.APT, "overlay_text")
        self.jpeg_quality = parser.getint(Settings.APT, "jpeg_quality")
        self.decode_all = parser.getboolean(Settings.APT, "decode_all")
        self.map_overlay_offset_x = parser.getint(Settings.APT,
                                                  "map_overlay_offset_x")
        self.map_overlay_offset_y = parser.getint(Settings.APT,
                                                  "map_overlay_offset_y")
        self.map_dir = parser.get(Settings.APT, "map_output_dir")
        self.map_enabled = parser.getboolean(Settings.APT, "map_enabled")
        self.enhancements_enabled = parser.getboolean(Settings.APT,
                                                      "enable_enhancements")
        self.enhancements = parser.get(Settings.APT,
                                       "enhancements").split(",")
        self.enhancements = [x for x in map(str.strip, self.enhancements) if x <> ""]

        #
        # Output settings
        #
        self.img_dir = parser.get(self.OUTPUT, "images")
        self.rec_dir = parser.get(self.OUTPUT, "recordings")
        self.spectrogram_dir = parser.get(self.OUTPUT,
                                          "spectrogram")

        #
        # Station settings
        #
        self.latitude = parser.getfloat(Settings.STATION, "latitude")
        self.longitude = parser.getfloat(Settings.STATION, "longitude")
        self.altitude = parser.getfloat(Settings.STATION, "altitude")
        self.longitude_negative = float(self.longitude)*-1
        self.minimum_elevation = parser.getint(Settings.STATION,
                                               "minimum_elevation")

        #
        # TLE Settings
        #
        self.tle_dir = parser.get(Settings.TLE, "directory")
        self.tle_file_name = parser.get(Settings.TLE, "file")
        self.tle_file = os.path.join(self.tle_dir, self.tle_file_name)
        self.tle_update_url = parser.get(Settings.TLE, "update_url")

        #
        # Logging
        #
        self.enable_logging = parser.getboolean(Settings.LOGGING,
                                                "enable")
        self.log_file = parser.get(Settings.LOGGING,
                                   "filename")
        self.pid_file = parser.get(Settings.LOGGING,
                                   "pid_file")
        self.status_file = parser.get(Settings.LOGGING, "status_file")

        #
        # Recording
        #
        self.gain = parser.getfloat(Settings.RECORDING, "gain")
        self.ppm_error = parser.getint(Settings.RECORDING, "ppm_error")
        self.wav_sample_rate = parser.getint(Settings.RECORDING,
                                             "wav_sample_rate")
        self.keep_raw_recording = parser.getboolean(Settings.RECORDING,
                                                    "keep_raw_recording")
        self.make_spectrogram = parser.getboolean(Settings.RECORDING,
                                                  "create_spectrogram")

        #
        # zxweather config
        #
        self.zxw_store_images = parser.getboolean(Settings.ZXWEATHER,
                                                  "store_images")
        self.zxw_store_enhancements = parser.get(
            Settings.ZXWEATHER, "store_enhancements").split(",")
        self.zxw_store_enhancements = [
            x.lower() for x in map(str.strip, self.zxw_store_enhancements)
            if x <> ""]
        self.zxw_store_spectrogram = parser.getboolean(Settings.ZXWEATHER,
                                                       "store_spectrogram")
        self.zxw_store_recording = parser.getboolean(Settings.ZXWEATHER,
                                                     "store_recording")
        self.zxw_image_source_code = parser.get(Settings.ZXWEATHER,
                                                "image_source_code")
        self.zxw_dsn = parser.get(Settings.ZXWEATHER,
                                  "dsn")

        #
        # Find satellite config
        #
        self.satellites = []
        for section in parser.sections():
            if section.startswith("satellite "):
                x = Satellite(
                    parser.get(section, "name"),
                    parser.getint(section, "frequency"),
                    parser.getint(section, "bandwidth"),
                    parser.get(section, "decode")
                )
                self.satellites.append(x)

    def get_satellite(self, name):
        """
        :param name: Name of the satellite to get
        :rtype: Satellite
        """
        for s in self.satellites:
            if s.name == name:
                return s
        return None
