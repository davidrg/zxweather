from __future__ import print_function
import subprocess

__author__ = 'David Goodwin'


gnuplot_binary = r'gnuplot'

# TODO: rewrite this garbage to be less garbage-y

gnuplot_instance = None
gnuplot_count = 1
next_gnuplot = 0


def run_plot_script(script):
    global gnuplot_instance, gnuplot_binary, next_gnuplot, gnuplot_count
    if gnuplot_instance is None:

        gnuplot_instance = []

        while len(gnuplot_instance) < gnuplot_count:
            ins = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
            gnuplot_instance.append(ins)


    script = "reset\n{0}\n".format(script)

    gnuplot_instance[next_gnuplot].stdin.write(script)

    next_gnuplot += 1
    if next_gnuplot >= gnuplot_count:
        next_gnuplot = 0


def plot_graph(output_filename, title=None, xdata_time=False, ylabel=None,
               lines=None, key=True, width=None, height=None, xlabel=None,
               yrange=None, xdata_is_time=False, timefmt_is_date=False,
               x_format=None, timefmt_is_time=False, x_range=None):
    """
    Plots a graph using gnuplot.
    :param timefmt_is_time:
    :param x_format:
    :param timefmt_is_date:
    :param xdata_is_time:
    :param yrange:
    :param output_filename:
    :param title:
    :param xdata_time: Formats the X axis to display time. Turns on
                       xdata_is_time, timefmt_is_time, sets the xlabel to
                       'Time' and sets the x_format to '%H:%M'.
    :param ylabel: Label for the Y axis
    :param xlabel: Label for the X axis
    :param lines:
    :param key:
    :param width:
    :param height:
    :param x_range: The minimum and maximum
    :return:
    """

    global gnuplot_binary

    print("Plot {0}".format(output_filename))

    # Line graph with a grid written to output_filename.
    # 'set datafile missing "?"' sets the ? character to mean missing data.
    script = """set style data lines
set grid
set output "{0}"
set datafile missing "?"
""".format(output_filename)

    if width is not None and height is not None:
        script += "set terminal pngcairo size {0}, {1}\n".format(width,height)
    else:
        script += "set terminal pngcairo\n"

    if title is not None:
        script += 'set title "{0}"\n'.format(title)

    #script += 'set xtics rotate by -45\n'

    if xdata_time:
        xdata_is_time = True
        timefmt_is_time = True
        timefmt_is_date = False
        x_format = '%H:%M'
        xlabel = 'Time'

    if xdata_is_time:
        script += "set xdata time\n"

    # "2012-04-02 15:14:35+12"
    if timefmt_is_date:
        script += 'set timefmt "%Y-%m-%d %H:%M:%S"\n'
    elif timefmt_is_time:
        script += 'set timefmt "%H:%M:%S"\n'

    if x_format is not None:
        script += 'set format x "{0}"\n'.format(x_format)

    if xlabel is not None:
        script += 'set xlabel "{0}"\n'.format(xlabel)

    if ylabel is not None:
        script += 'set ylabel "{0}"\n'.format(ylabel)

    if not key:
        script += "set key off\n"

    if yrange is not None:
        script += "set yrange [{0}:{1}]\n".format(yrange[0], yrange[1])

    if x_range is not None:
        script += 'set xrange ["{0}":"{1}"]\n'.format(x_range[0],x_range[1])


    # Override x_format if x data is time and doesn't include the date.
    if xdata_time and not timefmt_is_date:
        script += 'set format x "%H"\n'

    script += "plot "

    if lines is not None:
        for line in lines:
            if not script.endswith("plot "):
                script += ", "

            # We format the second column as ($n) so we don't join dots in the
            # case of missing data.
            script += "'{0}' using {1}:(${2}) title \"{3}\"".format(line['filename'],
                                                                 line['xcol'], line['ycol'], line['title'])
        file = open(output_filename + '.plt', 'w+')
        file.writelines(script)
        file.close()
        run_plot_script(script)
        #gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
        #gnuplot.communicate(script)


def plot_rainfall(data_file_name, output_filename, title, width, height, empty,
                  columns='1:2:xtic(1)'):
    """
    Plots a rainfall chart
    :param data_file_name: Name of the data file
    :param output_filename: Output name of the PNG file
    :param title: Chart title
    :param width: Chart width
    :param height: Chart height
    :param columns: Column specification.
    :return:
    """

    global gnuplot_binary

    print("Plot {0}".format(output_filename))

    xlabel = 'Hour'
    ylabel = 'Rainfall (mm)'
    colour = 'blue'

    script = ""

    script += 'set output "{0}"\n'.format(output_filename)
    script += 'set terminal pngcairo size {0}, {1}\n'.format(width, height)

    script += "set style fill solid border -1\n"
    script += "set boxwidth 0.8\n"

    script += 'set title "{0}"\n'.format(title)
    script += 'set xlabel "{0}"\n'.format(xlabel)
    script += 'set ylabel "{0}"\n'.format(ylabel)
    script += "set style fill solid\n"
    script += "set grid\n"
    script += "set key off\n"
    script += 'set format x "%s"\n'

    if empty:
        script += "set yrange [0:1]\n"

    script += "plot '{file}' using {columns} linecolor rgb '{colour}' with " \
              "boxes\n".format(file=data_file_name, colour=colour,
                               columns=columns)

    script_file = open(output_filename + '.plt', 'w+')
    script_file.writelines(script)
    script_file.close()
    run_plot_script(script)
    #gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
    #gnuplot.communicate(script)