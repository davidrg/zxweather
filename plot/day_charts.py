from gnuplot import plot_graph

__author__ = 'David Goodwin'


def charts_1_day(cur, dest_dir, day, month, year):
    """
    Charts detailing weather for a single day (24 hours max)
    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param day: Day to chart for
    :param month: Month to chart for
    :param year: Year to chart for
    """

    date = '{0}-{1}-{2}'.format(day, month, year)
    cur.execute("""select time_stamp::time, temperature, dew_point,
        apparent_temperature, wind_chill, relative_humidity, absolute_pressure,
        indoor_temperature, indoor_relative_humidity
        from sample where date(time_stamp) = %s::date
        order by time_stamp asc""", (date,))
    temperature_data = cur.fetchall()
    # Columns in the query
    COL_TIMESTAMP = 0
    COL_TEMPERATURE = 1
    COL_DEW_POINT = 2
    COL_APPARENT_TEMP = 3
    COL_WIND_CHILL = 4
    COL_REL_HUMIDITY = 5
    COL_ABS_PRESSURE = 6
    COL_INDOOR_TEMP = 7
    COL_INDOOR_REL_HUMIDITY = 8
    # Fields in the data file for gnuplot
    FIELD_TIMESTAMP = COL_TIMESTAMP + 1
    FIELD_TEMPERATURE = COL_TEMPERATURE + 1
    FIELD_DEW_POINT = COL_DEW_POINT + 1
    FIELD_APPARENT_TEMP = COL_APPARENT_TEMP + 1
    FIELD_WIND_CHILL = COL_WIND_CHILL + 1
    FIELD_REL_HUMIDITY = COL_REL_HUMIDITY + 1
    FIELD_ABS_PRESSURE = COL_ABS_PRESSURE + 1
    FIELD_INDOOR_TEMP = COL_INDOOR_TEMP + 1
    FIELD_INDOOR_REL_HUMIDITY = COL_INDOOR_REL_HUMIDITY + 1
    # Write the data file for gnuplot
    file_data = [
        '# timestamp  temperature  dew point  apparent temperature  wind chill  relative humidity  absolute pressure  indoor temperature  indoor relative humidity\n']
    for record in temperature_data:
        file_data.append(
            '{0}        {1}        {2}        {3}        {4}        {5}        {6}        {7}        {8}\n'
            .format(str(record[COL_TIMESTAMP]),
                    str(record[COL_TEMPERATURE]),
                    str(record[COL_DEW_POINT]),
                    str(record[COL_APPARENT_TEMP]),
                    str(record[COL_WIND_CHILL]),
                    str(record[COL_REL_HUMIDITY]),
                    str(record[COL_ABS_PRESSURE]),
                    str(record[COL_INDOOR_TEMP]),
                    str(record[COL_INDOOR_REL_HUMIDITY])))
    data_filename = dest_dir + 'gnuplot_data.dat'
    file = open(data_filename, 'w+')
    file.writelines(file_data)
    file.close()
    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 1280
            height = 960
        else:
            width = None    # Default width and height
            height = None

        # Plot Temperature and Dew Point
        output_filename = dest_dir + 'temperature_tdp.png'
        if large:
            output_filename = dest_dir + 'temperature_tdp_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Temperature and Dew Point",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_TEMPERATURE, # Temperature
                           'title': "Temperature"},
                           {'filename': data_filename,
                            'xcol': FIELD_TIMESTAMP, # Time
                            'ycol': FIELD_DEW_POINT, # Dew Point
                            'title': "Dew Point"}])

        # Plot Apparent Temperature and Wind Chill
        output_filename = dest_dir + 'temperature_awc.png'
        if large:
            output_filename = dest_dir + 'temperature_awc_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Apparent Temperature and Wind Chill",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_APPARENT_TEMP, # Apparent temperature
                           'title': "Apparent Temperature"},
                           {'filename': data_filename,
                            'xcol': FIELD_TIMESTAMP, # Time
                            'ycol': FIELD_WIND_CHILL, # Wind Chill
                            'title': "Wind Chill"}])

        # Humidity
        output_filename = dest_dir + 'humidity.png'
        if large:
            output_filename = dest_dir + 'humidity_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_REL_HUMIDITY, # Humidity
                           'title': "Relative Humidity"}])

        # Indoor Humidity
        output_filename = dest_dir + 'indoor_humidity.png'
        if large:
            output_filename = dest_dir + 'indoor_humidity_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Indoor Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_REL_HUMIDITY, # Indoor Humidity
                           'title': "Relative Humidity"}])

        # Absolute Pressure
        output_filename = dest_dir + 'pressure.png'
        if large:
            output_filename = dest_dir + 'pressure_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Absolute Pressure",
                   ylabel="Absolute Pressure (hPa)",
                   key=False,
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_ABS_PRESSURE, # Absolute Pressure
                           'title': "Absolute Pressure"}])

        # Indoor Temperature
        output_filename = dest_dir + 'indoor_temperature.png'
        if large:
            output_filename = dest_dir + 'indoor_temperature_large.png'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Indoor Temperature",
                   ylabel="Temperature (C)",
                   key=False,
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_TEMP, # Indoor Temperature
                           'title': "Temperature"}])

def charts_7_days(cur, dest_dir, day, month, year):
    date = '{0}-{1}-{2}'.format(day, month, year)
    cur.execute("""select s.time_stamp,
       s.temperature,
       s.dew_point,
       s.apparent_temperature,
       s.wind_chill,
       s.relative_humidity,
       s.absolute_pressure,
       s.indoor_temperature,
       s.indoor_relative_humidity
from sample s,
     (select max(time_stamp) as ts from sample where date(time_stamp) = %s::date::date) as max_ts
where s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
  and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
order by s.time_stamp asc
""", (date,))
    temperature_data = cur.fetchall()
    # Columns in the query
    COL_TIMESTAMP = 0
    COL_TEMPERATURE = 1
    COL_DEW_POINT = 2
    COL_APPARENT_TEMP = 3
    COL_WIND_CHILL = 4
    COL_REL_HUMIDITY = 5
    COL_ABS_PRESSURE = 6
    COL_INDOOR_TEMP = 7
    COL_INDOOR_REL_HUMIDITY = 8
    # Fields in the data file for gnuplot
    FIELD_TIMESTAMP = COL_TIMESTAMP + 1
    FIELD_TEMPERATURE = COL_TEMPERATURE + 2
    FIELD_DEW_POINT = COL_DEW_POINT + 2
    FIELD_APPARENT_TEMP = COL_APPARENT_TEMP + 2
    FIELD_WIND_CHILL = COL_WIND_CHILL + 2
    FIELD_REL_HUMIDITY = COL_REL_HUMIDITY + 2
    FIELD_ABS_PRESSURE = COL_ABS_PRESSURE + 2
    FIELD_INDOOR_TEMP = COL_INDOOR_TEMP + 2
    FIELD_INDOOR_REL_HUMIDITY = COL_INDOOR_REL_HUMIDITY + 2
    # Write the data file for gnuplot
    file_data = [
        '# timestamp  temperature  dew point  apparent temperature  wind chill  relative humidity  absolute pressure  indoor temperature  indoor relative humidity\n']
    for record in temperature_data:
        file_data.append(
            '{0}        {1}        {2}        {3}        {4}        {5}        {6}        {7}        {8}\n'
            .format(str(record[COL_TIMESTAMP]),
                    str(record[COL_TEMPERATURE]),
                    str(record[COL_DEW_POINT]),
                    str(record[COL_APPARENT_TEMP]),
                    str(record[COL_WIND_CHILL]),
                    str(record[COL_REL_HUMIDITY]),
                    str(record[COL_ABS_PRESSURE]),
                    str(record[COL_INDOOR_TEMP]),
                    str(record[COL_INDOOR_REL_HUMIDITY])))
    data_filename = dest_dir + '7-day_gnuplot_data.dat'
    file = open(data_filename, 'w+')
    file.writelines(file_data)
    file.close()
    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 1280
            height = 960
        else:
            width = None    # Default width and height
            height = None

        # Plot Temperature and Dew Point
        output_filename = dest_dir + '7-day_temperature_tdp.png'
        if large:
            output_filename = dest_dir + '7-day_temperature_tdp_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Temperature and Dew Point",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_TEMPERATURE, # Temperature
                           'title': "Temperature"},
                           {'filename': data_filename,
                            'xcol': FIELD_TIMESTAMP, # Time
                            'ycol': FIELD_DEW_POINT, # Dew Point
                            'title': "Dew Point"}])

        # Plot Apparent Temperature and Wind Chill
        output_filename = dest_dir + '7-day_temperature_awc.png'
        if large:
            output_filename = dest_dir + '7-day_temperature_awc_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Apparent Temperature and Wind Chill",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_APPARENT_TEMP, # Apparent temperature
                           'title': "Apparent Temperature"},
                           {'filename': data_filename,
                            'xcol': FIELD_TIMESTAMP, # Time
                            'ycol': FIELD_WIND_CHILL, # Wind Chill
                            'title': "Wind Chill"}])

        # Humidity
        output_filename = dest_dir + '7-day_humidity.png'
        if large:
            output_filename = dest_dir + '7-day_humidity_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_REL_HUMIDITY, # Humidity
                           'title': "Relative Humidity"}])

        # Indoor Humidity
        output_filename = dest_dir + '7-day_indoor_humidity.png'
        if large:
            output_filename = dest_dir + '7-day_indoor_humidity_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Indoor Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_REL_HUMIDITY, # Indoor Humidity
                           'title': "Relative Humidity"}])

        # Absolute Pressure
        output_filename = dest_dir + '7-day_pressure.png'
        if large:
            output_filename = dest_dir + '7-day_pressure_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Absolute Pressure",
                   ylabel="Absolute Pressure (hPa)",
                   key=False,
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_ABS_PRESSURE, # Absolute Pressure
                           'title': "Absolute Pressure"}])

        # Indoor Temperature
        output_filename = dest_dir + '7-day_indoor_temperature.png'
        if large:
            output_filename = dest_dir + '7-day_indoor_temperature_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Indoor Temperature",
                   ylabel="Temperature (C)",
                   key=False,
                   width=width,
                   height=height,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_TEMP, # Indoor Temperature
                           'title': "Temperature"}])
