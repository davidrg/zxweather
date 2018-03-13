"""
Classes for dealing with tab-delimited data files
"""
import json
import os
from datetime import date, datetime, timedelta

from dateutil.relativedelta import relativedelta
from twisted.internet import defer
from twisted.python import log


class DataFile(object):
    TSV_HEADER = '# \n'

    TSV_ROW = '\n'

    SORT_COLUMN = 0

    FILENAME = ""

    def __init__(self, rows, dir_fragment, date, station_code):
        self._rows = rows
        self._sort_rows()
        self._dir_fragment = dir_fragment
        self._date = date
        self._station_code = station_code

    @property
    def file_date(self):
        """
        Date for this file
        :rtype: date
        """
        return self._date

    @property
    def range(self):
        if len(self._rows) == 0:
            return None

        return (self._rows[0][self.SORT_COLUMN],
                self._rows[-1][self.SORT_COLUMN])

    def _filename(self, base_directory):
        return os.path.join(base_directory, self._dir_fragment, self.FILENAME)

    def filename(self, base_directory):
        return self._filename(base_directory)

    def _sort_rows(self):
        """
        Sorts rows by timestamp
        """
        self._rows = sorted(self._rows,
                            key=lambda row: row[DataFile.SORT_COLUMN])

    def _record_to_string(self, sample):
        """
        Serialise the row to a tab-delimited record
        :param sample: Row to serialise
        :type sample: List
        :return: Tab delimited record
        :rtype: str
        """
        return self.TSV_ROW.format(*sample)

    def _write_file(self, filename):
        """
        Writes the data file to disk
        :param filename: File to write to
        :type filename: str
        """

        # Data must be sorted before writing to disk
        self._sort_rows()

        # Ensure the output directory exists
        path = os.path.dirname(filename)
        if not os.path.exists(path):
            os.makedirs(path)

        # Write out full sample data
        log.msg("Write file: {0}".format(filename))

        with open(filename, "w") as f:
            f.write(self.TSV_HEADER)
            for row in self._rows:
                f.write(self._record_to_string(row))
            f.flush()


class SampleDataFile(DataFile):
    TSV_HEADER = '# timestamp\ttemperature\tdew point\tapparent temperature\t' \
                 'wind chill\trelative humidity\tpressure\t' \
                 'indoor temperature\tindoor relative humidity\trainfall\t' \
                 'average wind speed\tgust wind speed\twind direction\t' \
                 'uv index\tsolar radiation\treception\thigh_temp\tlow_temp\t' \
                 'high_rain_rate\tgust_direction\tevapotranspiration\t' \
                 'high_solar_radiation\thigh_uv_index\tforecast_rule_id\n'

    TSV_ROW = '{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t{11}\t' \
              '{12}\t{13}\t{14}\t{15}\t{16}\t{17}\t{18}\t{19}\t{20}\t{21}\t' \
              '{22}\t{23}\n'

    # Full sample data file columns
    COL_TIMESTAMP = 0
    COL_TEMPERATURE = 1
    COL_DEW_POINT = 2
    COL_APPARENT_TEMP = 3
    COL_WIND_CHILL = 4
    COL_REL_HUMIDITY = 5
    COL_PRESSURE = 6
    COL_INDOOR_TEMP = 7
    COL_INDOOR_REL_HUMIDITY = 8
    COL_RAINFALL = 9
    COL_AVG_WIND_SPEED = 10
    COL_GUST_WIND_SPEED = 11
    COL_WIND_DIRECTION = 12
    COL_UV_INDEX = 13
    COL_SOLAR_RADIATION = 14
    COL_RECEPTION = 15
    COL_HIGH_TEMP = 16
    COL_LOW_TEMP = 17
    COL_HIGH_RAIN_RATE = 18
    COL_GUST_DIRECTION = 19
    COL_EVAPOTRANSPIRATION = 20
    COL_HIGH_SOLAR_RADIATION = 21
    COL_HIGH_UV_INDEX = 22
    COL_FORECAST_RULE_ID = 23

    FILENAME = ""
    SORT_COLUMN = COL_TIMESTAMP

    def __init__(self, station_code, rows, date, dir_fragment):
        super(SampleDataFile, self).__init__(rows, dir_fragment, date, station_code)

        self._sort_rows()

    def get_last_ts(self):
        if len(self._rows) == 0:
            return None
        return self._rows[-1][SampleDataFile.COL_TIMESTAMP]

    def write_file(self, base_directory):
        """
        Writes the data file to the appropriate place under the specified base
        directory returning the filename used
        :param base_directory: Directory to write file to
        :type base_directory: str
        :return: Filename written to
        :rtype: str
        """
        fn = self._filename(base_directory)
        self._write_file(fn)
        return fn

    def add_row(self, row, base_directory):
        """
        Adds a row and updates the on-disk file

        :param row: Row to add 
        :type row: List
        :param base_directory: Root dir where on-disk files belong
        :type base_directory: str
        """

        last_row_ts = self.get_last_ts()

        self._rows.append(row)

        if base_directory is None:
            # No base directory? No update file on disk
            return

        file_exists = os.path.exists(self._filename(base_directory))

        if last_row_ts is not None and \
                last_row_ts < row[SampleDataFile.COL_TIMESTAMP] and \
                file_exists:
            # Row is newer than all previous rows! Append to existing on-disk
            # file
            with open(self._filename(base_directory), "a") as f:
                f.write(self._record_to_string(row))

        else:
            # row is either out-of-order or file doesn't exist on disk. The file
            # must be created from scratch (which will cause self._rows to be
            # sorted
            self.write_file(base_directory)


class DailyRainDataFile(DataFile):
    TSV_HEADER = '# hour\trainfall\n'
    TSV_ROW = "{0}\t{1}\n"

    # Hourly rain data file columns
    COL_HOUR = 0
    COL_RAIN_TOTAL = 1

    SORT_COLUMN = COL_HOUR
    FILENAME = "hourly_rainfall.dat"

    def __init__(self, station_code, year, month, day, rows):
        """
        Creates a new day level hourly rain data file
        :param station_code: Station code the data file is for
        :type station_code: str
        :param year: Year the data covers
        :type year: int
        :param month: Month the data covers
        :type month: int
        :param day: Day the data covers or null if this is a month-level data set
        :type day: Optional[int]
        :param rows: Row data. Data will be sorted automatically
        :type rows: List[Tuple[timestamp,Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[int],float,float,int,
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float]]]
        """

        dir_fragment = os.path.join(str(year), str(month), str(day))
        super(self.__class__, self).__init__([], dir_fragment,
                                             date(year, month, day),
                                             station_code)

        self._rain_data = {}
        for h in range(0,24):
            ts = datetime(year=year, month=month, day=day, hour=h, minute=0)
            self._rain_data[ts] = 0

        for row in rows:
            row_time = row[SampleDataFile.COL_TIMESTAMP]
            row_rain = row[SampleDataFile.COL_RAINFALL]
            hour = datetime(year=row_time.year, month=row_time.month,
                            day=row_time.day, hour=row_time.hour, minute=0)

            if row_rain is not None:
                self._rain_data[hour] += row_rain

        self._rebuild_rows()

    def _record_to_string(self, sample):
        """
        Serialise the row to a tab-delimited record
        :param sample: Row to serialise
        :type sample: List
        :return: Tab delimited record
        :rtype: str
        """

        # Hourly rainfall includes only the hour number
        s = list(sample)
        s[DailyRainDataFile.COL_HOUR] = s[DailyRainDataFile.COL_HOUR].hour

        return super(self.__class__, self)._record_to_string(tuple(s))

    def _rebuild_rows(self):
        self._rows = []
        for hour in sorted(self._rain_data.keys()):
            self._rows.append((hour, self._rain_data[hour]))

    def write_file(self, base_directory):
        """
        Writes the data file to the appropriate place under the specified base
        directory returning the filename used
        :param base_directory: Directory to write file to
        :type base_directory: str
        :return: Filename written to
        :rtype: str
        """
        fn = self._filename(base_directory)
        self._rebuild_rows()
        self._write_file(fn)
        return fn

    def add_row(self, new_sample, base_directory):
        """
        Adds a row and updates the on-disk file

        :param new_sample: Row to add 
        :type new_sample: List
        :param base_directory: Root dir where on-disk files belong
        :type base_directory: str
        """

        sample_time = new_sample[SampleDataFile.COL_TIMESTAMP]
        sample_hour = datetime(year=sample_time.year, month=sample_time.month,
                                day=sample_time.day, hour=sample_time.hour,
                               minute=0)
        sample_rain = new_sample[SampleDataFile.COL_RAINFALL]

        if sample_hour in self._rain_data:
            if sample_rain is not None:
                self._rain_data[sample_hour] += sample_rain
        else:
            log.msg("Warning: Attempt to add row dated {0} to file dated {1} "
                    "ignored".format(sample_time.date(), self.file_date))
            return  # Sample not for this date.

        self.write_file(base_directory)

    def total_rain(self):
        t = 0
        for row in self._rows:
            t += row[DailyRainDataFile.COL_RAIN_TOTAL]
        return t


class DailySampleDataFile(SampleDataFile):
    FILENAME = "samples.dat"

    def __init__(self, station_code, year, month, day, rows):
        """
        Creates a new data file
        :param station_code: Station code the data file is for
        :type station_code: str
        :param year: Year the data covers
        :type year: int
        :param month: Month the data covers
        :type month: int
        :param day: Day the data covers or null if this is a month-level data set
        :type day: Optional[int]
        :param rows: Row data. Data will be sorted automatically
        :type rows: List[Tuple[timestamp,Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[int],float,float,int,
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float]]]
        """
        dir_fragment = os.path.join(str(year), str(month), str(day))
        super(self.__class__, self).__init__(station_code, rows,
                                             date(year, month, day),
                                             dir_fragment)

    def _record_to_string(self, sample):
        """
        Serialise the row to a tab-delimited record
        :param sample: Row to serialise
        :type sample: List
        :return: Tab delimited record
        :rtype: str
        """

        # Day level data files exclude the date
        s = list(sample)
        s[SampleDataFile.COL_TIMESTAMP] = s[SampleDataFile.COL_TIMESTAMP].time()

        return super(self.__class__, self)._record_to_string(tuple(s))

    def to_rain_file(self):
        return DailyRainDataFile(self._station_code, self.file_date.year,
                                 self.file_date.month, self.file_date.day,
                                 self._rows)

    def add_row(self, row, base_directory):
        row_date = row[SampleDataFile.COL_TIMESTAMP].date()

        if row_date != self.file_date:
            log.msg("Warning: Attempt to add row dated {0} to file dated {1} "
                    "ignored".format(row_date, self.file_date))
            return

        super(self.__class__, self).add_row(row, base_directory)


class WeeklySampleDataFile(SampleDataFile):
    FILENAME = "7-day_samples.dat"

    def __init__(self, station_code, year, month, day, rows):
        """
        Creates a new data file
        :param station_code: Station code the data file is for
        :type station_code: str
        :param year: Year the data covers
        :type year: int
        :param month: Month the data covers
        :type month: int
        :param day: Day the data covers or null if this is a month-level data set
        :type day: Optional[int]
        :param rows: Row data. Data will be sorted automatically
        :type rows: List[Tuple[timestamp,Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[int],float,float,int,
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float]]]
        """
        dir_fragment = os.path.join(str(year), str(month), str(day))
        super(self.__class__, self).__init__(station_code, rows,
                                             date(year, month, day),
                                             dir_fragment)
        self._trim()

    def _trim(self):
        self._sort_rows()
        min_ts = self.get_last_ts() - timedelta(days=7)

        # Remove any records that don't belong in this file because they fall
        # outside the 7 day time span defined by the most recent record
        x = self._rows
        self._rows = []
        for row in x:
            if row[SampleDataFile.COL_TIMESTAMP] > min_ts:
                self._rows.append(row)

    def add_row(self, row, base_directory):
        """
        Adds a row and updates the on-disk file

        :param row: Row to add 
        :type row: List
        :param base_directory: Root dir where on-disk files belong
        :type base_directory: str
        """
        self._rows.append(row)
        self._trim()

        # We can't update the on-disk file in place as we have likely removed
        # a line from near the start of the file in addition to adding a new one
        # at the end.
        self.write_file(base_directory)

    def rows(self):
        return self._rows


class MonthlySampleDataFile(SampleDataFile):
    FILENAME = "samples.dat"

    def __init__(self, station_code, year, month, rows, previous):
        """
        Creates a new data file
        :param station_code: Station code the data file is for
        :type station_code: str
        :param year: Year the data covers
        :type year: int
        :param month: Month the data covers
        :type month: int
        :param rows: Row data. Data will be sorted automatically
        :type rows:  List[Tuple[timestamp,Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[int],float,float,int,
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float],Optional[float],
            Optional[float],Optional[float],Optional[float]]]
        :param previous: Previous data file (only used at the month level)
        :type previous: Optional[MonthlySampleDataFile]
        """
        super(self.__class__, self).__init__(
            station_code, rows, date(year, month, 1),
            os.path.join(str(year), str(month)))

        if previous is not None:
            self._previous_7days = previous.latest_weekly_file()
        else:
            self._previous_7days = None

    def to_day_files(self, day=None):
        """
        Generates a list of day-level data files from a month-level data file
        :return: 
        """

        # Split the rows out by date
        file_rows = {}
        for row in self._rows:
            row_date = row[SampleDataFile.COL_TIMESTAMP].date()

            if day is not None and row_date != day:
                continue  # Not interested in rows for this date

            if row_date not in file_rows:
                file_rows[row_date] = []

            file_rows[row_date].append(row)

        # Then wrap them up
        files = {}
        for file_date in file_rows.keys():
            rows = file_rows[file_date]
            f = DailySampleDataFile(self._station_code, file_date.year,
                                    file_date.month, file_date.day, rows)
            files[file_date] = f

        return files

    def _dates(self):
        dates = set()

        for r in self._rows:
            dates.add(r[SampleDataFile.COL_TIMESTAMP].date())

        return dates

    def latest_weekly_file(self):
        """
        Returns the data for the final 7 days (or 168h) of the month.
        
        :return: Last week of data
        :rtype: WeeklySampleDataFile
        """
        max_ts = self.get_last_ts()
        if max_ts is None:
            return None

        min_ts = self.get_last_ts() - timedelta(days=7)

        dt = max_ts.date()

        if self._previous_7days is not None:
            source = list(self._rows) + list(self._previous_7days.rows())
        else:
            source = list(self._rows)

        log.msg("Selecting latest weekly set from {0} to {1} "
                "from set of {2} rows".format(min_ts, max_ts, len(source)))

        week_rows = [r for r in source
                     if min_ts < r[SampleDataFile.COL_TIMESTAMP] <= max_ts]
        log.msg("Selected {0} rows".format(len(week_rows)))

        # We can just hand all our rows to the weekly file - it will trim as
        # necessary
        return WeeklySampleDataFile(self._station_code, dt.year, dt.month,
                                    dt.day, week_rows)

    def to_weekly_files(self, day=None):
        """
        Produces a list of weekly data files. If day is supplied the list will
        contain only the file for that day
        :param day: Day to generate the weekly data file for or None for all
                    days in the month
        :return: List of weekly data files
        :rtype: List[WeeklySampleDataFile]
        """
        if day is None:
            files = []
            for dt in self._dates():
                files += self.to_weekly_files(dt)
            return files

        # Prepare the source rows. If we have access to the previous months
        # data we'll add in the last week of data from that month too so we can
        # build the full files for the first six days of this month.
        if self._previous_7days is not None:
            source = self._rows + self._previous_7days.rows()
        else:
            source = self._rows

        rows = []
        for r in source:
            rts = r[SampleDataFile.COL_TIMESTAMP].date()
            if rts <= day:
                rows.append(r)

        return [WeeklySampleDataFile(self._station_code, day.year, day.month,
                                     day.day, rows), ]

    def add_row(self, row, base_directory):
        row_date = row[SampleDataFile.COL_TIMESTAMP].date()
        row_month = date(year=row_date.year, month=row_date.month, day=1)

        if row_month != self.file_date:
            log.msg("Warning: Attempt to add row dated {0} to monthly file "
                    "dated {1} ignored".format(row_date, self.file_date))
            return

        super(self.__class__, self).add_row(row, base_directory)


class RainFiles(object):
    def __init__(self, directory):
        self._summary_file = os.path.join(directory, "rain_summary.json")
        self._day_file = os.path.join(directory,
                                      "current_day_rainfall_totals.json")
        self._rolling_file = os.path.join(directory,
                                          "24hr_rainfall_totals.json")

        self._year_start = None
        self._year_total = 0.0
        self._year_end = None

        self._week_start = None
        self._week_total = 0.0
        self._week_end = None

        self._yesterday_start = None
        self._yesterday_total = 0.0
        self._yesterday_end = None

        self._today_start = None
        self._today_total = 0.0
        self._today_end = None

        self._month_start = None
        self._month_total = 0.0
        self._month_end = None

        self._24h_samples = []
        self._168h_samples = []
        self._7day_samples = []

    def new_sample(self, sample):
        ts = sample[SampleDataFile.COL_TIMESTAMP]
        rain = sample[SampleDataFile.COL_RAINFALL]

        new_sample_date = ts.date()

        last_ts = self._today_end
        if last_ts is None:
            last_ts = new_sample_date.date() - relativedelta(days=1)
        today = last_ts.date()

        if new_sample_date < today:
            # Samples from the past! Ignore.
            return

        if new_sample_date > today:
            # New day! We need to figure out what we need to reset.

            # At the very least, today and yesterday are resset
            self._yesterday_start = self._today_start
            self._yesterday_total = self._today_total
            self._yesterday_end = self._today_end
            self._today_start = ts
            self._today_total = 0
            self._today_end = ts
            # Note the above may glitch if we skip over multiple days. If this
            # happens, just delete the summary file and restart the service

            if today.weekday() == 6 and new_sample_date.weekday() == 0:
                # New week!
                self._week_start = ts
                self._week_end = ts
                self._week_total = 0

            if today.month != new_sample_date.month:
                # Month has also changed. Reset that too
                self._month_start = ts
                self._month_end = ts
                self._month_total = 0

            if today.year != new_sample_date.year:
                # New year! Reset..
                self._year_start = ts
                self._year_end = ts
                self._year_total = 0

        self._24h_samples.append((ts,rain))
        self._168h_samples.append((ts,rain))
        self._7day_samples.append((ts,rain))

        min_24h = ts - relativedelta(hours=24)
        min_168h = ts - relativedelta(hours=168)
        min_7day = ts.date() - relativedelta(days=7)

        # Trim samples outside the acceptable range
        self._24h_samples = [s for s in self._24h_samples if s[0] > min_24h]
        self._168h_samples = [s for s in self._168h_samples if s[0] > min_168h]
        self._7day_samples = [s for s in self._7day_samples if s[0] > min_7day]

        # Update totals and timespans
        self._today_start = min(self._today_start, ts)
        self._today_end = max(self._today_end, ts)
        self._today_total += rain

        self._week_start = min(self._today_start, ts)
        self._week_end = max(self._today_end, ts)
        self._week_total += rain

        self._month_start = min(self._month_start, ts)
        self._month_end = max(self._month_end, ts)
        self._month_total += rain

        self._year_start = min(self._year_start, ts)
        self._year_end = max(self._year_end, ts)
        self._year_total += rain

        self._write_file()

    @defer.inlineCallbacks
    def create(self, db):


        # TODO: these queries need to return ts and rain, not a total
        day_query = """select sum(rainfall) as day_rainfall
            from sample
            where time_stamp::date = $time
            and station_id = $station"""
        week_query = """select sum(rainfall) as sevenday_rainfall
        from sample s,
          (
             select max(time_stamp) as ts from sample
             where date(time_stamp) = $time::date
             and station_id = $station
          ) as max_ts
        where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
          and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
          and s.station_id = $station"""
        rolling_24h_query = """select sum(rainfall) as day_rainfall
            from sample
            where (time_stamp < $time and time_stamp > $time - '1 hour'::interval * 24)
            and station_id = $station"""
        rolling_168h_query = """select sum(rainfall) as sevenday_rainfall
        from sample s
        where s.time_stamp <= $time     -- 604800 seconds in a week.
          and s.time_stamp >= ($time - (604800 * '1 second'::interval))
          and s.station_id = $station"""

    def _write_file(self):
        summary = {
            "this_year": {
                "start": self._year_start.isoformat(),
                "total": self._year_total,
                "end": self._year_end.isoformat()
            },
            "this_week": {
                "start": self._week_start.isoformat(),
                "total": self._week_total,
                "end": self._week_end.isoformat()
            },
            "yesterday": {
                "start": self._yesterday_start.isoformat(),
                "total": self._yesterday_total,
                "end": self._yesterday_end.isoformat()
            },
            "today": {
                "start": self._today_start.isoformat(),
                "total": self._today_total,
                "end": self._today_end.isoformat()
            },
            "this_month": {
                "start": self._month_start.isoformat(),
                "total": self._month_total,
                "end": self._month_end.isoformat()
            }
        }

        day_totals = {
            "rainfall": self._today_total,
            "7day_rainfall": sum([r[1] for r in self._7day_samples])
        }

        rolling_totals = {
            "rainfall": sum([r[1] for r in self._24h_samples]),
            "7day_rainfall": sum([r[1] for r in self._168h_samples])
        }

        with open(self._summary_file, "w") as f:
            f.write(json.dumps(summary))

        with open(self._day_file, "w") as f:
            f.write(json.dumps(day_totals))

        with open(self._rolling_file, "w") as f:
            f.write(json.dumps(rolling_totals))