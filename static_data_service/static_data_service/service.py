# coding=utf-8
import os
from datetime import date, datetime

from dateutil.relativedelta import relativedelta
from twisted.application import service
from twisted.internet import defer, reactor
from twisted.internet.defer import returnValue
from twisted.python import log

from static_data_service.database import DatabaseReceiver, Database

# Note: on windows this application needs pywin32 installed. If it still fails
# to run with pywin32 installed make sure pythoncom27.dll and pywintypes27.dll
# are in \Lib\site-packages\win32
#
# Approximate Storage requirements:
#   Tab-delimited data files: 125MB per station per year
#   Static charts (no solar): 100MB per station per year
#   Static charts (with solar): 130MB per station per year
#
#
# TODO: Trash files only used for generating charts once charts are generated?
#
# TODO: two output sets (which can be combined together as desired)
#   -> Desktop support (generate files required by desktop client only
#   -> Web UI basic support (generate static charts required by basic UI)
# additional slow-to-generate files could also be manually enabled such as the
# cumulus ones.
#
# TODO: don't leave gnuplot running forever. Start and stop it to run batches
# of charts
#
# These files are required by the desktop client:
# Done  /data/sysconfig.json
# TODO  /data/<station>/live.json
# Done  /data/<station>/samplerange.json
# TODO  /data/<station>/image_sources.json
# TODO  /data/<station>/image_sources_by_date.json
# TODO  /data/<station>/rain_summary.json  <----
# Done  /data/<station>/<year>/<month>/samples.dat
# TODO  /data/<station>/<year>/<month>/<day>/images/index.json
# TODO  /data/<station>/<year>/<month>/<day>/images/<source>/<time-HH_MM_SS>/<type>_<format>.<extension>
# where image formats are: full, thumbnail, description, metadata
#
# These are to support cumulus-compatible software:
# TODO  /data/<station>/realtime.txt
# TODO  /data/<station>/dayfile.txt
#
# The following files are generated for use by the Web UI only:
# Done  /data/<station>/<year>/<month>/<day>/7-day_samples.dat
# Done  /data/<station>/<year>/<month>/<day>/hourly_rainfall.dat
# Done  /<station>/<year>/<month>/<day>/*.png
# The data files aren't strictly necessary long-term and they could be deleted
# later
#
# Other misc files:
# TODO  /data/<station>/current_day_records.json
# TODO  /data/<station>/current_day_rainfall_totals.json  <-----
# TODO  /data/<station>/24hr_rainfall_totals.json   <------
#
# JSON data for current day charts (both rolling and static)
# TODO  /data/<station>/current_day_samples.json
# TODO  /data/<station>/24hr_samples.json
# TODO  /data/<station>/current_day_7day_30m_avg_samples.json
# TODO  /data/<station>/168hr_30m_avg_samples.json
# TODO  /data/<station>/current_day_rainfall.json
# TODO  /data/<station>/24hr_rainfall.json
# TODO  /data/<station>/24hr_hourly_rainfall.json
# TODO  /data/<station>/current_day_7day_rainfall.json
# TODO  /data/<station>/168hr_rainfall.json
#
# More charts and other stuff
# TODO  /data/<station>/<year>/daily_records.json
# TODO  /data/<station>/<year>/<month>/samples.json
# TODO  /data/<station>/<year>/<month>/30m_avg_samples.json
# TODO  /data/<station>/<year>/<month>/daily_records.json
# TODO  /data/<station>/<year>/<month>/<day>/indoor_samples.json
# TODO  /data/<station>/<year>/<month>/<day>/records.json
# TODO  /data/<station>/<year>/<month>/<day>/7day_30m_avg_indoor_samples.json
# TODO  /data/<station>/<year>/<month>/<day>/samples.json
# TODO  /data/<station>/<year>/<month>/<day>/rainfall.json
# TODO  /data/<station>/<year>/<month>/<day>/hourly_rainfall.json
# TODO  /data/<station>/<year>/<month>/<day>/7day_30m_avg_samples.json
# TODO  /data/<station>/<year>/<month>/<day>/7day_samples.json
# TODO  /data/<station>/<year>/<month>/<day>/7day_indoor_samples.json
# TODO  /data/<station>/<year>/<month>/<day>/7day_hourly_rainfall.json
#
#
# Regeneration behaviour:
#   sysconfig.json
#       File missing - recreate
#       Station missing - add
#       Else - update hw_config, hw_type, coordinates, title, desc, order
#   samplerange.json
#       File missing - recreate setting both oldest and latest to minimum TS
#       latest is updated whenever a new sample comes
#
#   On startup, all .dat files and associated charts are regenerated starting
#   from the month specified in samplerange.json and up to (but excluding) the
#   current month. If the latest ts is for the current month nothing is
#   regenerated. If samplerange.json does not exist all files are regenerated
#   except those for this month.
#
#   When the first new sample arrives for a station the entire current month
#   is regenerated. After the first sample, the current day and month files are
#   updated in place while the rainfall, 7-day and chart files are regenerated
#   whenever a new sample arrives.
#
#   Station-level rain total files (rain_summary.json,
#   current_day_rainfall_totals.json, 24hr_rainfall_totals.json) are rewritten
#   on each new sample and on startup.
#
#  TODO: init rainfall whenever month file changes? This should result in it
#  being reset whenever the month or year changes limiting how long glitches
#  could survive.
#
#
# Currently working on:
# 24hr_rainfall_totals.json
# current_day_rainfall_totals.json
# rain_summary.json
# in datafile.py::RainFiles
#
#

from static_data_service.datafile import MonthlySampleDataFile
from static_data_service.gnuplot import make_day_chart_settings, \
    make_7day_chart_settings, GnuplotProcessProtocol
from static_data_service.metadatafile import SysConfigJson, SampleRangeJson

class StaticDataService(service.Service):
    def __init__(self, dsn, output_directory, build_charts, chart_formats,
                 chart_interval, gnuplot):
        """
        Constructs the Static Data Service
        
        :param dsn: Database connection string
        :type dsn: str
        :param output_directory: Directory to write generated data to
        :type output_directory: str
        :param build_charts: If static charts should be produced
        :type build_charts: bool
        :param chart_formats: Which formats static charts should be produced in
        :type chart_formats: List[str]
        :param chart_interval: How often to produce charts (in samples)
        :type chart_interval: int
        :param gnuplot: Gnuplot binary to use
        :type gnuplot: str
        """

        # Root directory for all output (both data files and charts).
        # data files will go into output_directory/data/, charts will go
        # into output_directory/station-code/
        self._output_directory = output_directory

        self._db_receiver = DatabaseReceiver(dsn)
        self._db_receiver.NewSample += self._new_sample
        self._db = Database(dsn)
        self._db.DatabaseReady += self._database_ready

        # Current day and month data files by station code
        self._monthly = {}
        self._weekly = {}
        self._daily = {}
        self._daily_rain = {}

        # metadata files we maintain
        self._sysconfig = SysConfigJson(self._db, self._data_dir())
        self._samplerange = SampleRangeJson(self._db, self._data_dir())

        # Queue of gnuplot tasks
        self._gnuplot_queue = []

        # Charting settings
        self._build_charts = build_charts
        self._chart_formats = chart_formats
        self._gnuplot_binary = gnuplot

        # Dictionary of broadcast IDs keyed by lowercase station code
        self._broadcast_ids = {}
        self._has_solar = {}

    def startService(self):
        log.msg("Static Data Service startup...")
        service.Service.startService(self)

        if self._build_charts:
            self._gnuplot = GnuplotProcessProtocol()
            self._gnuplot.Ready += self._gnuplot_ready
            reactor.spawnProcess(self._gnuplot, self._gnuplot_binary,
                                 path=os.path.dirname(self._gnuplot_binary))

        # Connect to the database. This will eventually fire the ready event
        # resulting in self._database_ready being called
        self._db.connect()

        #self._db_receiver.connect()

    def stopService(self):
        service.Service.stopService(self)

    @defer.inlineCallbacks
    def _new_sample(self, station_code, sample):

        row_date = sample[0].date()

        data_dir = self._station_data_dir(station_code)

        # Update the data files!
        month = date(row_date.year, row_date.month, 1)
        log.msg("New sample for {0}/{1} station {2}".format(
            row_date.month, row_date.year, station_code))

        if station_code in self._monthly:
            latest = self._monthly[station_code].get_last_ts()
            if sample[0] < latest:
                # Sample is from the past! Force regeneration of the entire
                # month
                self._monthly[station_code] = None

        if station_code in self._monthly \
                and self._monthly[station_code] is not None \
                and self._monthly[station_code].file_date == month:

            log.msg("Add to month file")
            # We have the current data file in memory; append the row!
            self._monthly[station_code].add_row(sample, data_dir)

            # Now deal with the day-level file
            if station_code in self._daily \
                    and self._daily[station_code].file_date == row_date:
                log.msg("Add to day and week files")
                # We have the current day-level file; append!
                self._daily[station_code].add_row(sample, data_dir)
                self._weekly[station_code].add_row(sample, data_dir)
                self._daily_rain[station_code].add_row(sample, data_dir)
            else:
                log.msg("Day file missing or wrong date. "
                        "Recreating day and week")
                # The day file doesn't exist or is for a previous date. Create
                # a new one for the current date as well as a new weekly file.
                day_files = self._monthly[station_code].to_day_files(row_date)
                # There should only be one
                self._daily[station_code] = day_files[day_files.keys()[0]]
                self._weekly[station_code] = \
                    self._monthly[station_code].latest_weekly_file()
                self._daily_rain[station_code] = \
                    self._daily[station_code].to_rain_file()

            # Update sysconfig.json and samplerange.json
            self._set_latest_sample_time(station_code, sample[0])
        else:
            log.msg("Month file missing or wrong month. Recreating.")

            if station_code not in self._monthly:
                self._monthly[station_code] = None

            self._monthly[station_code] = yield self._month_datafile(
                station_code, month, self._monthly[station_code])

            if month.day > 1:
                # We're picking up part way through the month. Make sure all the
                # outputs for previous days exist.
                self._build_archival_monthly_outputs(
                    station_code,
                    self._monthly[station_code],
                    self._has_solar[station_code])

            day_files = self._monthly[station_code].to_day_files(row_date)

            # There should only be one
            self._daily[station_code] = day_files[day_files.keys()[0]]
            self._weekly[station_code] = self._monthly[
                station_code].latest_weekly_file()
            self._daily_rain[station_code] = self._daily[
                station_code].to_rain_file()

            self._monthly[station_code].write_file(data_dir)
            self._daily[station_code].write_file(data_dir)
            self._weekly[station_code].write_file(data_dir)
            self._daily_rain[station_code].write_file(data_dir)

            self._set_latest_sample_time(
                station_code, self._monthly[station_code].get_last_ts())

            self._build_current_charts(station_code)

    def _build_current_charts(self, station_code):
        if self._build_charts:
            dt = self._daily[station_code].file_date
            station_charts_dir = self._station_dir(station_code)
            charts_dir = os.path.join(station_charts_dir,
                                      str(dt.year),
                                      dt.strftime("%B").lower(),
                                      str(dt.day))

            dr = self._daily[station_code].range
            if dr is not None:
                day_range = (dr[0], dr[1])

                for fmt in self._chart_formats:
                    chart_specs = make_day_chart_settings(
                        charts_dir,
                        self._daily[station_code].filename(
                            self._station_data_dir(station_code)),
                        self._daily_rain[station_code].filename(
                            self._station_data_dir(station_code)),
                        day_range,
                        fmt,  # Chart format
                        self._has_solar[station_code],
                        self._daily_rain[station_code].total_rain()
                    )

                    self.queue_charts(chart_specs)

            dr = self._weekly[station_code].range
            if dr is not None:
                week_range = (str(dr[0]), str(dr[1]))

                for fmt in self._chart_formats:
                    chart_specs = make_7day_chart_settings(
                        charts_dir,
                        self._monthly[station_code].filename(
                            self._station_data_dir(station_code)),
                        week_range,
                        fmt,  # Chart format
                        self._has_solar[station_code]
                    )

                    self.queue_charts(chart_specs)

    @defer.inlineCallbacks
    def _get_station_codes(self):
        stations = yield self._db.get_station_codes()

        station_codes = []

        # Grab a list of all station codes the database knows about along with
        # their broadcast IDs and if solar data is available
        for s in stations:
            station = s[0]
            broadcast_id = s[1]
            solar = s[2]
            station_codes.append(station)
            self._broadcast_ids[station] = broadcast_id

            self._has_solar[station] = solar

        returnValue(station_codes)

    @defer.inlineCallbacks
    def _database_ready(self):
        log.msg("Database connected!")

        yield self._sysconfig.create_or_update()

        stations = yield self._get_station_codes()

        yield self.prepare_output_directory(stations)

        log.msg("Setup completed.")
        log.msg("Connecting to live data...")
        self._db_receiver.connect()

    def _data_dir(self):
        d = os.path.join(self._output_directory, "data")

        if not os.path.exists(d):
            os.makedirs(d)

        return d

    def _station_data_dir(self, station):
        d = os.path.join(self._output_directory, "data", station.lower())

        if not os.path.exists(d):
            os.makedirs(d)

        return d

    def _station_dir(self, station):
        d = os.path.join(self._output_directory, station.lower())

        if not os.path.exists(d):
            os.makedirs(d)

        return d

    def _set_latest_sample_time(self, station, time):
        """
        Updates the most recent sample timestamp stored in samplerange.json
        and sysconfig.json
        
        :param station: Station whose maximum sample timestamp is being updated
        :type station: str
        :param time: Latest sample timestamp
        :type time: datetime
        """
        if time is None:
            return  # No time

        # Update samplerange.json
        self._samplerange.set_latest_sample_time(station, time)

        # Update sysconfig.json
        self._sysconfig.set_latest_sample_time(station, time)

    def queue_charts(self, chart_specs):
        """
        Queues a list of chart specs
        :param chart_specs: 
         :type chart_specs: list
        :return: 
        """
        self._gnuplot_queue += chart_specs

        # If gnuplot is currently idle, start processing
        if self._gnuplot.idle:
            self._gnuplot_ready()

    def _gnuplot_ready(self):
        if len(self._gnuplot_queue) > 0:
            specs = self._gnuplot_queue.pop(0)
            #log.msg("Plotting: {0}".format(specs))
            self._gnuplot.plot(specs.to_script())

    @defer.inlineCallbacks
    def _month_datafile(self, station_code, month, previous_month=None):
        month_data = yield self._db.get_month_samples(
            station_code, self._broadcast_ids[station_code],
            month.year, month.month)
        df = MonthlySampleDataFile(station_code, month.year, month.month,
                                   month_data, previous_month)
        returnValue(df)

    @defer.inlineCallbacks
    def prepare_output_directory(self, station_codes):
        """
        Prepares the output directory. 
        :param station_codes: List of station codes to generate for
        """

        log.msg("Preparing output directory...")

        # Generate archival data for all stations
        for station in station_codes:
            log.msg("Station: {0}".format(station))

            start_ts = yield self._samplerange.get_sample_range_end(station)

            # Find all the months between start_ts and now.
            dt = start_ts.date()
            today = datetime.now().date()
            this_month = date(year=today.year, month=today.month, day=1)

            if dt >= this_month:
                log.msg("Station up-to-date")
                # On-disk stuff for this station is up-to-date prior to this
                # month.
                continue

            log.msg("Building datasets from: {0}".format(dt))

            # We don't want to generate the data file for this month/today -
            # we'll do that when the first sample for the station comes in to
            # reduce the chances of missing a new sample
            data_file = None
            while dt < this_month:
                log.msg("Month: {0}".format(dt.strftime("%b-%Y").upper()))

                data_file = yield self._month_datafile(station, dt,
                                                      data_file)

                if data_file.get_last_ts() is not None:
                    # Only bother creating the month data file if there is data
                    # to put in it

                    self._build_monthly_outputs(station, data_file)

                    self._set_latest_sample_time(station, data_file.get_last_ts())

                dt += relativedelta(months=1)
        print("Completed data file generation")

    def _build_monthly_outputs(self, station_code, data_file):
        """
        Takes a month-level data file and produces the daily and 7-daily 
        outputs from it (both data files and charts where enabled).
         
        :param station_code: Station to plot for
         :type station_code: str
        :param data_file: Month level data file to generate from
         :type data_file: MonthlySampleDataFile
        """

        station_data_dir = self._station_data_dir(station_code)
        station_charts_dir = self._station_dir(station_code)
        solar_enabled = self._has_solar[station_code]

        data_file.write_file(station_data_dir)

        day_files = data_file.to_day_files()
        for file_date in sorted(day_files.keys()):
            charts_dir = os.path.join(station_charts_dir,
                                      str(file_date.year),
                                      file_date.strftime("%B").lower(),
                                      str(file_date.day))

            f = day_files[file_date]
            day_filename = f.write_file(station_data_dir)
            rain_file = f.to_rain_file()
            rain_filename = rain_file.write_file(station_data_dir)

            if self._build_charts:
                dr = f.range
                if dr is None:
                    continue  # No range - no chart!

                day_range = (dr[0], dr[1])

                for fmt in self._chart_formats:
                    chart_specs = make_day_chart_settings(
                        charts_dir,
                        day_filename,
                        rain_filename,
                        day_range,
                        fmt,  # Chart format
                        solar_enabled,
                        rain_file.total_rain()
                    )

                    self.queue_charts(chart_specs)

        week_files = data_file.to_weekly_files()
        for f in week_files:
            charts_dir = os.path.join(station_charts_dir,
                                      str(f.file_date.year),
                                      f.file_date.strftime("%B").lower(),
                                      str(f.file_date.day))
            weekly_filename = f.write_file(station_data_dir)

            if self._build_charts:
                dr = f.range
                if dr is None:
                    continue  # No range - no chart!

                week_range = (dr[0], dr[1])

                for fmt in self._chart_formats:
                    chart_specs = make_7day_chart_settings(
                        charts_dir,
                        weekly_filename,
                        week_range,
                        fmt,  # Chart format
                        solar_enabled
                    )

                    self.queue_charts(chart_specs)
