# coding=utf-8
"""
Handles production of charts using gnuplot
"""

import os

from twisted.internet import protocol
from twisted.python import log

from static_data_service.util import Event


class GnuplotSettings(object):
    def __init__(self, output_filename, title, width, height, output_format):
        self._output_filename = output_filename + \
                                GnuplotSettings._get_file_extension(
                                    output_format)
        self._output_filename = self._output_filename.replace("\\", "/")
        self._title = title
        self._width = width
        self._height = height
        self._output_format = output_format

    @staticmethod
    def _get_file_extension(output_format):
        """
        Tries to come up with a file extension for the supplied output format.
        Note that just because this function can come up with a file extension
        doesn't mean that the charts will come out ok.
        :param output_format: Output format (really the gnuplot terminal stuff)
        :return: File extension
        """
        # postscript enhanced
        # postscript eps
        formats = [
            ("png", ".png"),
            ("gif", ".gif"),
            ("jpeg", ".jpg"),
            ("pdf", ".pdf"),
            ("postscript", ".ps"),
            ("svg", ".svg"),
            ("canvas", ".js"),

        ]
        for fmt in formats:
            if output_format.startswith(fmt[0]):
                return fmt[1]
        return ""

    def to_script(self):
        pass


class StandardChartSeries(object):
    def __init__(self, data_file, x_column, y_column, title):
        self._file = data_file
        self._x_col = x_column
        self._y_col = y_column
        self._title = title

    @property
    def data_file(self):
        return self._file

    @property
    def x_column(self):
        return self._x_col

    @property
    def y_column(self):
        return self._y_col

    @property
    def title(self):
        return self._title


class StandardChart(GnuplotSettings):
    def __init__(self, output_filename, title, width, height,
                 #xdata_time=False,
                 value_axis_label=None, series=None, key=True,
                 key_axis_label=None, value_axis_range=None, key_axis_is_time=True,
                 time_includes_date=False, key_axis_format=None,# timefmt_is_time=False,
                 key_axis_range=None, output_format="pngcairo"):
        """
        
        :param output_filename: File to write the chart to. Extension will be added automatically 
        :param title: Title for the plot. None for no title
        :param width: Width for the plot
        :param height: Height for the plot
        :param value_axis_label: Label for value axis
        :param series: Series to include in the plot
        :param key: Include the key
        :param key_axis_label: Label for Key axis. 
        :param value_axis_range: Range for the value axis
        :param key_axis_is_time: Time is on the key axis
        :param key_axis_format: Format string for key axis values
        :param key_axis_range: Range of the key axis
        :param output_format: Gnuplot device to use
        :param key_axis_is_time: Time on the key axis (rather than some non-time value)
        """

        super(StandardChart, self).__init__(output_filename, title, width,
                                             height, output_format)

        self._x_range = key_axis_range
        #self._timefmt_is_time = timefmt_is_time
        self._x_format = key_axis_format
        self._time_has_date = time_includes_date
        self._key_axis_is_time = key_axis_is_time
        self._yrange = value_axis_range
        self._xlabel = key_axis_label
        self._key = key
        self._series = series
        self._ylabel = value_axis_label

        # _xdata_is_time

        #if xdata_time:
            #self._xdata_is_time = True
            #self._timefmt_is_time = True
            #self._time_has_date = False
            #self._x_format = '%H:%M'
#            self._xlabel = 'Time'

        # if key_axis_is_time and not time_includes_date:
        #     self._x_format = '%H'

    def __str__(self):
        return "Standard Line Chart - {0} {1}".format(self._title,
                                                      self._x_range)

    def to_script(self):
        # Line graph with a grid written to output_filename.
        # 'set datafile missing "?"' sets the ? character to mean missing data.
        script = """set style data lines

set style line 1 lc rgb '#8b1a0e' pt 1 ps 1 lt 1 lw 2 # --- red
set style line 2 lc rgb '#5e9c36' pt 6 ps 1 lt 1 lw 2 # --- green
set style line 11 lc rgb '#808080' lt 1
set border 3 back ls 11
set tics nomirror
set style line 12 lc rgb '#808080' lt 0 lw 1
set grid back ls 12

set grid
set output "{0}"
set datafile missing "None"
""".format(self._output_filename)

        if self._width is not None and self._height is not None:
            script += "set terminal {2} size {0}, {1}\n".format(self._width,
                                                                self._height,
                                                                self._output_format)
        else:
            script += "set terminal {0}\n".format(self._output_format)

        if self._title is not None:
            script += 'set title "{0}"\n'.format(self._title)

        # script += 'set xtics rotate by -45\n'

        if self._key_axis_is_time:
            script += "set xdata time\n"

            # "2012-04-02 15:14:35+12"
            if self._time_has_date:
                script += 'set timefmt "%Y-%m-%d %H:%M:%S"\n'
            else:
                script += 'set timefmt "%H:%M:%S"\n'

        if self._x_format is not None:
            script += 'set format x "{0}"\n'.format(self._x_format)

        if self._xlabel is not None:
            script += 'set xlabel "{0}"\n'.format(self._xlabel)

        if self._ylabel is not None:
            script += 'set ylabel "{0}"\n'.format(self._ylabel)

        if not self._key:
            script += "set key off\n"

        if self._yrange is not None:
            script += "set yrange [{0}:{1}]\n".format(self._yrange[0],
                                                      self._yrange[1])

        if self._x_range is not None:
            script += 'set xrange ["{0}":"{1}"]\n'.format(self._x_range[0],
                                                          self._x_range[1])

        script += "plot "

        if self._series is not None:
            for series in self._series:
                if not script.endswith("plot "):
                    script += ", "

                # We format the second column as ($n) so we don't join dots in
                # the case of missing data.
                script += "'{0}' using {1}:(${2}) title \"{3}\"".format(
                    series.data_file.replace("\\", "/"),
                    series.x_column, series.y_column, series.title)

        # Flush the output file to disk and close it
        script += '\nset output\n'
        #script += 'print "--plot complete--"\n'

        return script


class DayLineChart(StandardChart):
    def __init__(self, filename, output_format, large, title, value_axis_label,
                 series, key_axis_range, key=True):
        if large:
            width = 1140
            height = 600
            time_format = '%H:%M'
        else:
            width = 570
            height = 300

            # Only have space for the hour in the small version
            time_format = '%H'

        super(DayLineChart, self).__init__(
            filename,
            title=title,
            value_axis_label=value_axis_label,
            width=width,
            height=height,
            key_axis_range=key_axis_range,
            key_axis_label="Time",
            key_axis_format=time_format,
            series=series,
            output_format=output_format,
            key=key)


class WeekLineChart(StandardChart):
    def __init__(self, filename, output_format, large, title, value_axis_label,
                 series, key_axis_range, key=True):
        if large:
            width = 2340
            height = 600
        else:
            width = 1170
            height = 300

        super(WeekLineChart, self).__init__(
            filename,
            title=title,
            value_axis_label=value_axis_label,
            width=width,
            height=height,
            key_axis_range=key_axis_range,
            key_axis_label="Date",
            key_axis_format='%d-%b',
            time_includes_date=True,
            series=series,
            output_format=output_format,
            key=key)


class RainfallChart(GnuplotSettings):
    def __init__(self, output_filename, title, width, height,
                 empty, data_file, columns='1:2:xtic(1)',
                 output_format="pngcairo"):
        super(self.__class__, self).__init__(output_filename, title, width,
                                             height, output_format)

        self._data_file = data_file.replace("\\", "/")
        self._columns = columns
        self._empty = empty

    def __str__(self):
        return "Rainfall Chart - {0} {1}".format(self._title,
                                                      self._data_file)

    def to_script(self):
        xlabel = 'Hour'
        ylabel = 'Rainfall (mm)'
        colour = 'blue'

        script = """set style line 11 lc rgb '#808080' lt 1
set border 3 back ls 11
set tics nomirror
set style line 12 lc rgb '#808080' lt 0 lw 1
set grid back ls 12
"""

        script += 'set output "{0}"\n'.format(self._output_filename)
        script += 'set terminal {2} size {0}, {1}\n'.format(self._width,
                                                            self._height,
                                                            self._output_format)

        script += "set style fill solid border -1\n"
        script += "set boxwidth 0.8\n"

        script += 'set title "{0}"\n'.format(self._title)
        script += 'set xlabel "{0}"\n'.format(xlabel)
        script += 'set ylabel "{0}"\n'.format(ylabel)
        script += "set style fill solid\n"
        script += "set grid\n"
        script += "set key off\n"
        script += 'set format x "%s"\n'

        if self._empty:
            script += "set yrange [0:1]\n"

        script += "plot '{file}' using {columns} linecolor rgb '{colour}' with " \
                  "boxes\n".format(file=self._data_file, colour=colour,
                                   columns=self._columns)

        return script


# Fields in the standard tab-delimited data files
FIELD_TIMESTAMP = 1
FIELD_TEMPERATURE = 2
FIELD_DEW_POINT = 3
FIELD_APPARENT_TEMP = 4
FIELD_WIND_CHILL = 5
FIELD_REL_HUMIDITY = 6
FIELD_ABS_PRESSURE = 7
FIELD_INDOOR_TEMP = 8
FIELD_INDOOR_REL_HUMIDITY = 9
FIELD_RAINFALL = 10
FIELD_AVG_WIND_SPEED = 11
FIELD_GUST_WIND_SPEED = 12
FIELD_WIND_DIRECTION = 13
FIELD_UV_INDEX = 14
FIELD_SOLAR_RADIATION = 15


def make_line_chart_settings(dest_dir, data_filename, x_range, output_format,
                             solar, large, single_day):
    """
    Produces chart configuration for day-level charts.
    
    :param dest_dir: Directory the charts will be saved to 
    :param data_filename: Data file to generate charts from
    :param x_range: Minimum and maximum timestamps the data file covers
    :param output_format: Output image format
    :param solar: If solar sensors are available in the data file
    :param large: If large versions should be generated instead of small
    :return: List of charts
    """
    charts = []

    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    def chart(filename, title, value_axis_label, series, key=True):
        if large:
            filename += "_large"

        if not single_day:
            filename = "7day_" + filename

        filename = os.path.join(dest_dir, filename)

        if single_day:
            return DayLineChart(filename, output_format, large, title,
                                value_axis_label, series, x_range, key=key)
        else:
            return WeekLineChart(filename, output_format, large, title,
                                 value_axis_label, series, x_range, key=key)

    def field(f):
        if f == FIELD_TIMESTAMP:
            return f

        if single_day:
            return f

        # 7-day data sets have both date and time columns which are handled
        # separately by gnuplot. So for these datasets we need to shift all the
        # non-timestamp columns along by one
        return f+1

    charts.append(chart("temperature_tdp",
                        "Temperature and Dew Point",
                        "Temperature (C)",
                        [
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_TEMPERATURE),
                                                "Temperature"),
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_DEW_POINT),
                                                "Dew Point")
                        ]))
    charts.append(chart("temperature_awc",
                        "Apparent Temperature and Wind Chill",
                        "Temperature (C)",
                        [
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_APPARENT_TEMP),
                                                "Apparent Temperature"),
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_WIND_CHILL),
                                                "Wind Chill"),
                        ]))
    charts.append(chart("humidity",
                        "Humidity",
                        "Relative Humidity (%)",
                        [
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_REL_HUMIDITY),
                                                "Relative Humidity"),
                        ],
                        key=False))
    charts.append(
        chart("indoor_humidity",
              "Indoor Humidity",
              "Relative Humidity (%)",
              [
                  StandardChartSeries(data_filename,
                                      field(FIELD_TIMESTAMP),
                                      field(FIELD_INDOOR_REL_HUMIDITY),
                                      "Relative Humidity")
              ],
              key=False))
    charts.append(chart("pressure",
                        "Pressure",
                        "Pressure (hPa)",
                        [
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_ABS_PRESSURE),
                                                "Pressure")
                        ],
                        key=False))
    charts.append(chart("indoor_temperature",
                        "Indoor Temperature",
                        "Temperature (C)",
                        [
                            StandardChartSeries(data_filename,
                                                field(FIELD_TIMESTAMP),
                                                field(FIELD_INDOOR_TEMP),
                                                "Temperature")
                        ],
                        key=False))
    if solar:
        charts.append(
            chart("solar_radiation",
                  "Solar Radiation",
                  "W/m^2",
                  [
                      StandardChartSeries(data_filename,
                                          field(FIELD_TIMESTAMP),
                                          field(FIELD_SOLAR_RADIATION),
                                          "Solar Radiation")
                  ],
                  key=False))
        charts.append(chart("uv_index",
                            "UV Index",
                            "",
                            [
                                StandardChartSeries(data_filename,
                                                    field(FIELD_TIMESTAMP),
                                                    field(FIELD_UV_INDEX),
                                                    "UV Index")
                            ],
                            key=False))
    return charts


def make_day_chart_settings(dest_dir, data_filename, rain_data_filename,
                            x_range, output_format, solar, rain_total):

    day_range = (str(x_range[0].time()), str(x_range[1].time()))

    small = make_line_chart_settings(dest_dir, data_filename, day_range,
                                     output_format, solar, False, True)
    large = make_line_chart_settings(dest_dir, data_filename, day_range,
                                     output_format,
                                     solar, True, True)

    charts = small + large

    empty = rain_total == 0
    rain_small = RainfallChart(os.path.join(dest_dir, "hourly_rainfall"),
                               "Hourly Rainfall", 570, 300, empty,
                               rain_data_filename, output_format=output_format)
    rain_large = RainfallChart(os.path.join(dest_dir, "hourly_rainfall_large"),
                               "Hourly Rainfall", 1140, 600, empty,
                               rain_data_filename, output_format=output_format)
    charts.append(rain_small)
    charts.append(rain_large)

    return charts


def make_7day_chart_settings(dest_dir, data_filename, x_range, output_format,
                             solar):

    date_range = (str(x_range[0]), str(x_range[1]))

    small = make_line_chart_settings(dest_dir, data_filename, date_range,
                                     output_format, solar, False, False)
    large = make_line_chart_settings(dest_dir, data_filename, date_range,
                                     output_format,
                                     solar, True, False)

    return small + large


class GnuplotProcessProtocol(protocol.ProcessProtocol):

    def __init__(self):
        self._idle = False
        self._received_output = ""
        self.Ready = Event()

    def connectionMade(self):
        log.msg("Gnuplot started!")
        self._idle = True
        self.Ready.fire()
        pass

    def errReceived(self, data):
        # self._received_output += data.strip()
        log.msg(data)

        # if self._received_output.endswith("--plot complete--"):
        #     self._received_output = ""
        #     self._idle = True
        #     log.msg("gnuplot ready")
        #     self.Ready.fire()


    def outReceived(self, data):
        #self._received_output += data
        log.msg(data)

        # if self._received_output.endswith("gnuplot>"):
        #     self._received_output = ""
        #     self._idle = True
        #     log.msg("gnuplot ready")
        #     self.Ready.fire()

    def plot(self, script):
        #log.msg(script)
        self._idle = False
        s = "reset\n{0}\n".format(script)
        self.transport.write(s)
        self._idle = True
        self.Ready.fire()

    @property
    def idle(self):
        return self._idle

