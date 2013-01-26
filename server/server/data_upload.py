# coding=utf-8
"""
Implements the command for uploading data and all the stuff for processing
the incoming CSV records.
"""
from twisted.internet import defer
from server.command import Command
from server.database import get_station_hw_type, BaseSampleRecord, WH1080SampleRecord, insert_wh1080_sample, BaseLiveRecord, insert_base_live, insert_generic_sample

__author__ = 'david'

def _float_or_none(val):
    if val == "Null" or val == "None":
        return None
    return float(val)

def _int_or_none(val):
    if val == "Null" or val == "None":
        return None
    return int(val)

def _get_base_sample_record(values):
    """
    Builds a BaseSampleRecord instance from the supplied values.
    :param values: Full list of sample values.
    :return: A new populated BaseSampleRecord
    :rtype: BaseSampleRecord
    """

    rec = BaseSampleRecord(
        # s for sample
        station_code = values[1],
        temperature = _float_or_none(values[2]),
        humidity = _int_or_none(values[3]),
        indoor_temperature = _float_or_none(values[4]),
        indoor_humidity = _int_or_none(values[5]),
        pressure = _float_or_none(values[6]),
        average_wind_speed = _float_or_none(values[7]),
        gust_wind_speed = _float_or_none(values[8]),
        wind_direction = _int_or_none(values[9]),
        rainfall = _float_or_none(values[10]),
        download_timestamp = values[11], # Postgres can handle this
        time_stamp = values[12], # And this.
    )

    return rec

def _get_wh1080_sample_record(values):
    """
    Builds a WH1080SampleRecord instance from the supplied values.
    :param values: Full list of sample values.
    :return: A new populated WH1080SampleRecord
    :rtype: WH1080SampleRecord
    """

    # TODO: Handle nulls

    if values[16] == 'True':
        invalid_data = True
    else:
        invalid_data = False

    if values[19] == 'True':
        rain_overflow = True
    else:
        rain_overflow = False

    if values[15] == 'True':
        last_in_batch = True
    else:
        last_in_batch = False

    rec = WH1080SampleRecord(
        sample_interval = int(values[13]),
        record_number = int(values[14]),
        last_in_batch = last_in_batch,
        invalid_data = invalid_data,
        wind_direction = values[17],
        total_rain = float(values[18]),
        rain_overflow = rain_overflow,
    )

    return rec


def insert_csv_sample(values):
    """
    Inserts CSV sample data.
    :type values: list
    :Returns: a failure message or None all wrapped in a Deferred
    :rtype: Deferred
    """
    base = _get_base_sample_record(values)
    hw_type = get_station_hw_type(base.station_code)

    if hw_type == 'FOWH1080':
        if len(values) != 20:
            msg = "# ERR-003: Invalid FOWH1080 sample record - " \
                  "column count not 20. Rejecting."
            # Yeah, this is stupid - succeeding with a value is failure,
            # succeeding with nothing is success.
            return defer.succeed(msg)

        wh1080 = _get_wh1080_sample_record(values)

        return insert_wh1080_sample(base, wh1080)
    elif hw_type == 'GENERIC':
        if len(values) != 13:
            msg = "# ERR-003: Invalid GENERIC sample record - "\
                  "column count not 13. Rejecting."
            # Yeah, this is stupid - succeeding with a value is failure,
            # succeeding with nothing is success.
            return defer.succeed(msg)

        return insert_generic_sample(base)
    else:
        msg = "# ERR-004: Unsupported hardware type {0}. Record " \
              "rejected.".format(hw_type)
        return defer.succeed(msg)

def _get_base_live_record(values):
    """
    Builds a BaseLiveRecord instance from the supplied values.
    :param values: Full list of live values.
    :return: A new populated BaseLiveRecord
    :rtype: BaseLiveRecord
    """

    rec = BaseLiveRecord(
        # l for live
        station_code = values[1],
        download_timestamp = values[2], # We'll let postgres parse this.
        indoor_humidity = _int_or_none(values[3]),
        indoor_temperature = _float_or_none(values[4]),
        temperature = _float_or_none(values[5]),
        humidity = _int_or_none(values[6]),
        pressure = _float_or_none(values[7]),
        average_wind_speed = _float_or_none(values[8]),
        gust_wind_speed = _float_or_none(values[9]),
        wind_direction = _int_or_none(values[10]),
    )
    return rec

def insert_csv_live(values):
    """
    Inserts CSV live data
    :param values: Live data values
    :Returns: a failure message or None all wrapped in a Deferred
    :rtype: Deferred
    """

    base = _get_base_live_record(values)
    hw_type = get_station_hw_type(base.station_code)

    # there is only one format for the live data at the moment.
    if hw_type in ['FOWH1080','GENERIC']:
        if len(values) != 11:
            msg = "# ERR-005: Invalid FOWH1080 live record - column count not" \
                  " 11. Rejecting."
            return defer.succeed(msg)

        return insert_base_live(base)
    else:
        msg = "# ERR-004: Unsupported hardware type {0}. Record "\
              "rejected.".format(hw_type)
        return defer.succeed(msg)

class UploadCommand(Command):
    """
    Implements a command that allows new samples and live data to be streamed
    _to_ the database for storage.
    """

    def _result_handler(self, message):
        if message is None: return
        self.writeLine(message)

    def _error_handler(self, failure):
        # Something went wrong - probably a database error related to bad
        # data being pushed. Send it back to the client so they know what
        # went wrong. Might give some idea of how to gix it.
        failure.trap(Exception)
        return "# ERR-006: " + failure.getErrorMessage()

    def _processSample(self, values):
        """
        Processes one sample.
        :param values: The sample values.
        :type values: list
        """
        if len(values) < 14:
            self.writeLine("# ERR-001: Invalid sample record - column count "
                           "less than 14. Record rejected.")
            return

        try:
            insert_csv_sample(values).addErrback(
                self._error_handler).addCallback(self._result_handler)
        except Exception as e:
            self.writeLine("# ERR-006: " + e.message)

    def _processLive(self, values):
        """
        Processes one live data record
        :param values: The live data values
        :type values: list
        :return:
        """
        if len(values) < 11:
            self.writeLine("# ERR-002: Invalid live record - column count "
                           "less than 11. Record rejected.")
            return

        try:
            insert_csv_live(values).addErrback(
                self._error_handler).addCallback(self._result_handler)
        except Exception as e:
            self.writeLine("# ERR-006: " + e.message)

    def _processLine(self, line):
        """
        Processes a line sent from the client.
        :param line: Line data
        :type line: str
        """

        values = line.split(",")
        if values[0] == 's':
            self._processSample(values)
        elif values[0] == 'l':
            self._processLive(values)
        else:
            self.writeLine("# ERR-000: Unrecognised record type '{0}'".format(
                values[0]))

        self._getLine()

    def _getLine(self):
        self.readLine().addCallback(self._processLine)

    def cleanUp(self):
        """  Clean up """
        self.writeLine("# Finished")

    def main(self):
        """Entry """
        self.auto_exit = False
        self.writeLine("# Waiting for data. Send ^C when finished.")
        self._getLine()