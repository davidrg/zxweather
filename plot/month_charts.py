from gnuplot import plot_graph

__author__ = 'David Goodwin'

def month_charts(cur, dest_dir, month, year):
    """
    Charts detailing weather for a single month.
    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param month: Month to chart for
    :param year: Year to chart for
    :return:
    """

    date = '01-{0}-{1}'.format(month,year)
    cur.execute("""select cur.time_stamp,
       cur.temperature,
       cur.dew_point,
       cur.apparent_temperature,
       cur.wind_chill,
       cur.relative_humidity,
       cur.absolute_pressure,
       cur.indoor_temperature,
       cur.indoor_relative_humidity,
       cur.time_stamp::time - (cur.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((cur.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap
from sample cur, sample prev
where date(date_trunc('month',cur.time_stamp)) = %s::date
  and prev.time_stamp = (select max(time_stamp) from sample where time_stamp < cur.time_stamp)
order by cur.time_stamp asc""", (date,))

    weather_data = cur.fetchall()

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
    COL_PREV_TIMESTAMP = 9
    COL_PREV_SAMPLE_MISSING = 10

    # Fields in the data file for gnuplot. Field numbers start at 1.
    FIELD_TIMESTAMP = COL_TIMESTAMP + 1

    # The timestamp field actually counts as two fields as far as gnuplot is
    # concerned. So we must skip over that too.
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

    FORMAT_STRING = '{0}        {1}        {2}        {3}        {4}        {5}        {6}        {7}        {8}\n'
    for record in weather_data:
        # Handle missing data.
        if record[COL_PREV_SAMPLE_MISSING]:
            file_data.append(FORMAT_STRING.format(str(record[COL_PREV_TIMESTAMP]),
                                                  '?','?','?','?','?','?','?','?'))

        file_data.append(FORMAT_STRING.format(str(record[COL_TIMESTAMP]),
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
            width = 2560
            height = 960
        else:
            width = 1280
            height = 480

        # Plot Temperature and Dew Point
        output_filename = dest_dir + 'temperature_tdp.png'
        if large:
            output_filename = dest_dir + 'temperature_tdp_large.png'

        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'temperature_awc.png'
        if large:
            output_filename = dest_dir + 'temperature_awc_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'humidity.png'
        if large:
            output_filename = dest_dir + 'humidity_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'indoor_humidity.png'
        if large:
            output_filename = dest_dir + 'indoor_humidity_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'pressure.png'
        if large:
            output_filename = dest_dir + 'pressure_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'indoor_temperature.png'
        if large:
            output_filename = dest_dir + 'indoor_temperature_large.png'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
    pass