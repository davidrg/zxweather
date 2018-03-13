# coding=utf-8
import json
import os

import dateutil.parser
from twisted.internet import defer
from twisted.internet.defer import returnValue, log


class SysConfigJson(object):
    def __init__(self, db, data_dir):
        self._db = db
        self._data_dir = data_dir
        self._filename = os.path.join(data_dir, "sysconfig.json")

    @defer.inlineCallbacks
    def _create_sysconfig_station_row(self, station):
        min_ts = yield self._db.get_earliest_sample_timestamp(station[0])
        s = {
            "code": station[0].upper(),
            "msg_ts": None,
            "hw_config": json.loads(station[1]) if station[1] is not None
            else None,
            "hw_type": {
                "code": station[2],
                "name": station[3]
            },
            "name": station[4],
            "coordinates": {  # TODO: Privacy option?
                "latitude": station[5],
                "longitude": station[6]
            },
            "range": {  # The max value will be updated later
                "max": min_ts.isoformat(),
                "min": min_ts.isoformat()
            },
            "site_title": station[7],
            "desc": station[8],
            "order": station[9]
        }
        returnValue(s)

    @defer.inlineCallbacks
    def _create_sysconfig(self, stations):
        """
        Creates the sysconfig.json file
  
        :param stations: List of station info
        :return: 
        """
        doc = {
            "site_name": None,
            "stations": [],

            # These should be filled out by the user
            "image_type_sort": None,
            "wss_uri": None,
            "ws_uri": None,
            "zxweatherd_raw_port": None,
            "zxweatherd_host": None
        }

        for station in stations:
            s = yield self._create_sysconfig_station_row(station)
            doc["stations"].append(s)

        with open(self._filename, "w") as f:
            f.write(json.dumps(doc, sort_keys=True, indent=4,
                               separators=(',', ': ')))

    @defer.inlineCallbacks
    def _update_sysconfig(self, stations):

        with open(self._filename, "r") as f:
            doc = json.loads(f.read())

        for station in stations:
            station_updated = False
            new_row = yield self._create_sysconfig_station_row(station)

            for idx in range(0, len(doc["stations"])):
                if doc["stations"][idx]["code"].upper() == station[0].upper():
                    # Copy over current values from the database. We'll
                    # overwrite everything except range (which should be
                    # up-to-date already
                    doc["stations"][idx]["hw_config"] = new_row["hw_config"]
                    doc["stations"][idx]["hw_type"] = new_row["hw_type"]
                    doc["stations"][idx]["coordinates"] = new_row["coordinates"]
                    doc["stations"][idx]["site_title"] = new_row["site_title"]
                    doc["stations"][idx]["desc"] = new_row["desc"]
                    doc["stations"][idx]["order"] = new_row["order"]
                    station_updated = True

            if not station_updated:
                doc["stations"].append(new_row)

        with open(self._filename, "w") as f:
            f.write(json.dumps(doc, sort_keys=True, indent=4,
                               separators=(',', ': ')))

    @defer.inlineCallbacks
    def create_or_update(self):
        """
        Updates the sysconfig.json station list. If the file does not exist it
        is created. Other settings in the file are not touched.
        """
        station_info = yield self._db.get_station_info()

        if not os.path.exists(self._filename):
            yield self._create_sysconfig(station_info)
        else:
            yield self._update_sysconfig(station_info)

    def set_latest_sample_time(self, station, time):
        """
        Updates the last sample time for the specified file in sysconfig.json
        :param station: Station to update
         :type station: str
        :param time: Latest sample timestamp
         :type time: datetime.datetime
        :return: 
        """

        if time is None or station is None:
            return  # Nothing to do

        with open(self._filename, "r") as f:
            sysconfig = json.loads(f.read())

        for i in range(0, len(sysconfig["stations"])):
            code = sysconfig["stations"][i]["code"].lower()
            if code == station.lower():
                sysconfig["stations"][i]["range"]["max"] = time.isoformat()

        with open(self._filename, "w") as f:
            f.write(json.dumps(sysconfig, sort_keys=True, indent=4,
                               separators=(',', ': ')))

class SampleRangeJson(object):
    FILENAME = "samplerange.json"
    def __init__(self, db, data_dir):
        self._data_dir = data_dir
        #self._filename = os.path.join(station_data_dir, "samplerange.json")
        self._db = db

    @defer.inlineCallbacks
    def get_sample_range_end(self, station_code):
        """
        Reads the maximum timestamp for the specified station from
        samplerange.json. If the file does not exist it is created and set to
        have the value of the earliest timestamp in the database which is
        then returned.

        :param station_code: Station to get latest processed timestamp for
        :type station_code: str
        :return: latest timstamp
        :rtype: datetime
        """

        filename = os.path.join(self._data_dir, station_code.lower(),
                                SampleRangeJson.FILENAME)

        if os.path.exists(filename):
            with open(filename, "r") as f:
                j = json.loads(f.read())
                start_ts = dateutil.parser.parse(j["latest"])
        else:
            log.msg("samplerange.json not found. Rebuilding...")
            start_ts = yield self._db.get_earliest_sample_timestamp(
                station_code)
            doc = {
                "oldest": start_ts.isoformat(),
                "latest": start_ts.isoformat()
            }
            with open(filename, "w") as f:
                f.write(json.dumps(doc))
        returnValue(start_ts)

    def set_latest_sample_time(self, station, time):
        if time is None:
            return  # No time

        filename = os.path.join(self._data_dir, station.lower(),
                                SampleRangeJson.FILENAME)

        with open(filename, "r") as f:
            doc = json.loads(f.read())
        doc["latest"] = time.isoformat()
        with open(filename, "w") as f:
            f.write(json.dumps(doc, sort_keys=True, indent=4,
                               separators=(',', ': ')))

