from __future__ import print_function
import subprocess

__author__ = 'David Goodwin'


gnuplot_binary = r'C:\Program Files (x86)\gnuplot\bin\gnuplot.exe'

def plot_graph(output_filename, title=None, xdata_time=False, ylabel=None,
               lines=None, key=True, width=None, height=None, xlabel=None,
               yrange=None, xdata_is_time=False, timefmt_is_date=False,
               x_format=None, timefmt_is_time=False):
    """
    Plots a graph using gnuplot.
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
        script += "set terminal png size {0}, {1}\n".format(width,height)
    else:
        script += "set terminal png\n"

    if title is not None:
        script += 'set title "{0}"\n'.format(title)


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

    script += "plot "

    if lines is not None:
        for line in lines:
            if not script.endswith("plot "):
                script += ", "

            # We format the second column as ($n) so we don't join dots in the
            # case of missing data.
            script += "'{0}' using {1}:(${2}) title \"{3}\"".format(line['filename'],
                                                                 line['xcol'], line['ycol'], line['title'])

        gnuplot = subprocess.Popen([gnuplot_binary], stdin=subprocess.PIPE)
        gnuplot.communicate(script)
