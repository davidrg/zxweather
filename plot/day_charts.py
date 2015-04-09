from datetime import date
from gnuplot import plot_graph, plot_rainfall

__author__ = 'David Goodwin'


def charts_1_day(cur, dest_dir, plot_date, station_code, output_format,
                 hw_config):
    """
    Charts detailing weather for a single day (24 hours max)
    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param plot_date: Date to chart for
    :type plot_date: date
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param output_format: Output format (eg, "pngcairo")
    :type output_format: str
    :param hw_config: station hardware configuration details
    :type hw_config: weatherplot.StationConfig
    """

    cur.execute("""select cur.time_stamp::time,
       round(cur.temperature::numeric,2),
       round(cur.dew_point::numeric, 1),
       round(cur.apparent_temperature::numeric, 1),
       round(cur.wind_chill::numeric,1),
       cur.relative_humidity,
       round(cur.absolute_pressure::numeric,2),
       round(cur.indoor_temperature::numeric,2),
       cur.indoor_relative_humidity,
       round(cur.rainfall::numeric, 1),
       round(cur.average_wind_speed::numeric,2),
       round(cur.gust_wind_speed::numeric,2),
       cur.wind_direction,
       cur.time_stamp::time - (s.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (cur.time_stamp - prev.time_stamp) > ((s.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap,
       ds.average_uv_index as uv_index,
       ds.solar_radiation
from sample cur
inner join sample prev on prev.time_stamp = (
        select max(x.time_stamp)
        from sample x
        where x.time_stamp < cur.time_stamp
        and x.station_id = cur.station_id) and prev.station_id = cur.station_id
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(cur.time_stamp) = %s
  and s.code = %s
order by cur.time_stamp asc""", (plot_date, station_code,))
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
    COL_RAINFALL = 9
    COL_AVG_WIND_SPEED = 10
    COL_GUST_WIND_SPEED = 11
    COL_WIND_DIRECTION = 12
    COL_PREV_TIMESTAMP = 13
    COL_PREV_SAMPLE_MISSING = 14
    COL_UV_INDEX = 15
    COL_SOLAR_RADIATION = 16

    # Fields in the data file for gnuplot. Field numbers start at 1.
    FIELD_TIMESTAMP = COL_TIMESTAMP + 1
    FIELD_TEMPERATURE = COL_TEMPERATURE + 1
    FIELD_DEW_POINT = COL_DEW_POINT + 1
    FIELD_APPARENT_TEMP = COL_APPARENT_TEMP + 1
    FIELD_WIND_CHILL = COL_WIND_CHILL + 1
    FIELD_REL_HUMIDITY = COL_REL_HUMIDITY + 1
    FIELD_ABS_PRESSURE = COL_ABS_PRESSURE + 1
    FIELD_INDOOR_TEMP = COL_INDOOR_TEMP + 1
    FIELD_INDOOR_REL_HUMIDITY = COL_INDOOR_REL_HUMIDITY + 1
    FIELD_RAINFALL = COL_RAINFALL + 1
    FIELD_AVG_WIND_SPEED = COL_AVG_WIND_SPEED + 1
    FIELD_GUST_WIND_SPEED = COL_GUST_WIND_SPEED + 1
    FIELD_WIND_DIRECTION = COL_WIND_DIRECTION + 1
    FIELD_UV_INDEX = 14
    FIELD_SOLAR_RADIATION = 15

    # Write the data file for gnuplot
    file_data = [
        '# timestamp\ttemperature\tdew point\tapparent temperature\twind chill'
        '\trelative humidity\tabsolute pressure\tindoor temperature\t'
        'indoor relative humidity\trainfall\taverage wind speed\t'
        'gust wind speed\twind direction\tuv index\tsolar radiation\n']

    FORMAT_STRING = '{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t{11}\t{12}\t{13}\t{14}\n'
    for record in weather_data:
        # Handle missing data.
        if record[COL_PREV_SAMPLE_MISSING]:
            file_data.append(
                FORMAT_STRING.format(str(record[COL_PREV_TIMESTAMP]),
                                     '?', '?', '?', '?', '?', '?', '?', '?',
                                     '?', '?', '?', '?', '?', '?'))

        file_data.append(FORMAT_STRING.format(str(record[COL_TIMESTAMP]),
                                              str(record[COL_TEMPERATURE]),
                                              str(record[COL_DEW_POINT]),
                                              str(record[COL_APPARENT_TEMP]),
                                              str(record[COL_WIND_CHILL]),
                                              str(record[COL_REL_HUMIDITY]),
                                              str(record[COL_ABS_PRESSURE]),
                                              str(record[COL_INDOOR_TEMP]),
                                              str(record[
                                                  COL_INDOOR_REL_HUMIDITY]),
                                              str(record[COL_RAINFALL]),
                                              str(record[COL_AVG_WIND_SPEED]),
                                              str(record[COL_GUST_WIND_SPEED]),
                                              str(record[COL_WIND_DIRECTION]),
                                              str(record[COL_UV_INDEX]),
                                              str(record[COL_SOLAR_RADIATION])
        ))
    x_range = (str(weather_data[0][COL_TIMESTAMP]),
               str(weather_data[len(weather_data) - 1][COL_TIMESTAMP]))
    data_filename = dest_dir + 'gnuplot_data.dat'
    file = open(data_filename, 'w+')
    file.writelines(file_data)
    file.close()
    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 1140
            height = 600
        else:
            width = 570    # Default width and height
            height = 300

        # Plot Temperature and Dew Point
        output_filename = dest_dir + 'temperature_tdp'
        if large:
            output_filename = dest_dir + 'temperature_tdp_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Temperature and Dew Point",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_TEMPERATURE, # Temperature
                           'title': "Temperature"},
                          {'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_DEW_POINT, # Dew Point
                           'title': "Dew Point"}],
                   output_format=output_format)

        # Plot Apparent Temperature and Wind Chill
        output_filename = dest_dir + 'temperature_awc'
        if large:
            output_filename = dest_dir + 'temperature_awc_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Apparent Temperature and Wind Chill",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_APPARENT_TEMP, # Apparent temperature
                           'title': "Apparent Temperature"},
                          {'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_WIND_CHILL, # Wind Chill
                           'title': "Wind Chill"}],
                   output_format=output_format)

        # Humidity
        output_filename = dest_dir + 'humidity'
        if large:
            output_filename = dest_dir + 'humidity_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_REL_HUMIDITY, # Humidity
                           'title': "Relative Humidity"}],
                   output_format=output_format)

        # Indoor Humidity
        output_filename = dest_dir + 'indoor_humidity'
        if large:
            output_filename = dest_dir + 'indoor_humidity_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Indoor Humidity",
                   ylabel="Relative Humidity (%)",
                   key=False,
                   width=width,
                   height=height,
                   yrange=(0, 100),
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_REL_HUMIDITY, # Indoor Humidity
                           'title': "Relative Humidity"}],
                   output_format=output_format)

        # Absolute Pressure
        output_filename = dest_dir + 'pressure'
        if large:
            output_filename = dest_dir + 'pressure_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Absolute Pressure",
                   ylabel="Absolute Pressure (hPa)",
                   key=False,
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_ABS_PRESSURE, # Absolute Pressure
                           'title': "Absolute Pressure"}],
                   output_format=output_format)

        # Indoor Temperature
        output_filename = dest_dir + 'indoor_temperature'
        if large:
            output_filename = dest_dir + 'indoor_temperature_large'
        plot_graph(output_filename,
                   xdata_time=True,
                   title="Indoor Temperature",
                   ylabel="Temperature (C)",
                   key=False,
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_TEMP, # Indoor Temperature
                           'title': "Temperature"}],
                   output_format=output_format)

        # Solar Radiation
        if hw_config.has_solar_sensor:
            output_filename = dest_dir + 'solar_radiation'
            if large:
                output_filename = dest_dir + 'solar_radiation_large'
            plot_graph(output_filename,
                       xdata_time=True,
                       title="Solar Radiation",
                       ylabel="W/m^2",
                       key=False,
                       width=width,
                       height=height,
                       x_range=x_range,
                       lines=[{'filename': data_filename,
                               'xcol': FIELD_TIMESTAMP, # Time
                               'ycol': FIELD_SOLAR_RADIATION,
                               'title': "Solar Radiation"}],
                       output_format=output_format)

        # UV Index
        if hw_config.has_uv_sensor:
            output_filename = dest_dir + 'uv_index'
            if large:
                output_filename = dest_dir + 'uv_index_large'
            plot_graph(output_filename,
                       xdata_time=True,
                       title="UV Index",
                       ylabel="",
                       key=False,
                       width=width,
                       height=height,
                       x_range=x_range,
                       lines=[{'filename': data_filename,
                               'xcol': FIELD_TIMESTAMP, # Time
                               'ycol': FIELD_UV_INDEX,
                               'title': "UV Index"}],
                       output_format=output_format)


def rainfall_1_day(cur, dest_dir, plot_date, station_code, output_format):
    """
    Plots 1-day hourly rainfall charts
    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param plot_date: Date to chart for
    :type plot_date: date
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param output_format: Output format (eg, "pngcairo")
    :type output_format: str
    """

    cur.execute("""
    select extract(hour from s.time_stamp)::integer  as time_stamp,
       coalesce(sum(s.rainfall),0)
    from sample s
    inner join station st on st.station_id = s.station_id
    where date(s.time_stamp) = %s
      and st.code = %s
    group by extract(hour from s.time_stamp)
    order by extract(hour from s.time_stamp) asc
    """, (plot_date, station_code,))
    rainfall_data = cur.fetchall()

    # Columns in the query
    COL_HOUR = 0
    COL_RAINFALL = 1

    # Write the data file for gnuplot
    file_data = ['# hour\trainfall\n']
    FORMAT_STRING = '{0}\t{1}\n'
    rain_total = 0
    for record in rainfall_data:
        # TODO: Handle missing data
        file_data.append(FORMAT_STRING.format(str(record[COL_HOUR]),
                                              str(record[COL_RAINFALL])))
        rain_total += record[COL_RAINFALL]

    data_filename = dest_dir + 'hourly_rainfall.dat'
    data_file = open(data_filename, 'w+')
    data_file.writelines(file_data)
    data_file.close()

    empty = rain_total == 0.0

    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 1140
            height = 600
        else:
            width = 570    # Default width and height
            height = 300

        # Plot Temperature and Dew Point
        output_filename = dest_dir + 'hourly_rainfall'
        if large:
            output_filename = dest_dir + 'hourly_rainfall_large'

        plot_rainfall(data_filename,
                      output_filename,
                      "Hourly Rainfall",
                      width,
                      height,
                      empty,
                      output_format=output_format)


def charts_7_days(cur, dest_dir, plot_date, station_code, output_format,
                 hw_config):
    """
    Creates 7-day charts for the specified day
    :param cur: Database cursor
    :param dest_dir: Destination directory
    :param plot_date: Date to plot for
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param output_format: The output format (eg, "pngcairo")
    :type output_format: str
    :param hw_config: station hardware configuration details
    :type hw_config: weatherplot.StationConfig
    """
    cur.execute("""select s.time_stamp,
       round(s.temperature::numeric,2),
       round(s.dew_point::numeric, 1),
       round(s.apparent_temperature::numeric, 1),
       round(s.wind_chill::numeric,1),
       s.relative_humidity,
       round(s.absolute_pressure::numeric,2),
       round(s.indoor_temperature::numeric,2),
       s.indoor_relative_humidity,
       round(s.rainfall::numeric, 1),
       round(s.average_wind_speed::numeric,2),
       round(s.gust_wind_speed::numeric,2),
       s.wind_direction,
       s.time_stamp::time - (st.sample_interval * '1 minute'::interval) as prev_sample_time,
       CASE WHEN (s.time_stamp - prev.time_stamp) > ((st.sample_interval * 2) * '1 minute'::interval) THEN
          true
       else
          false
       end as gap,
       ds.average_uv_index as uv_index,
       ds.solar_radiation
from (select max(time_stamp) as ts, station_id from sample where time_stamp::date = %s group by station_id) as max_ts
inner join sample s on s.time_stamp <= max_ts.ts
        and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
inner join sample prev on prev.station_id = s.station_id
        and prev.time_stamp = (
            select max(ps.time_stamp)
            from sample ps
            where ps.time_stamp < s.time_stamp
            and ps.station_id = s.station_id)
inner join station st on st.station_id = s.station_id
left outer join davis_sample ds on ds.sample_id = s.sample_id
where st.code = %s
order by s.time_stamp asc
""", (plot_date, station_code,))
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
    COL_RAINFALL = 9
    COL_AVG_WIND_SPEED = 10
    COL_GUST_WIND_SPEED = 11
    COL_WIND_DIRECTION = 12
    COL_PREV_TIMESTAMP = 13
    COL_PREV_SAMPLE_MISSING = 14
    COL_UV_INDEX = 15
    COL_SOLAR_RADIATION = 16

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
    FIELD_RAINFALL = COL_RAINFALL + 2
    FIELD_AVG_WIND_SPEED = COL_AVG_WIND_SPEED + 2
    FIELD_GUST_WIND_SPEED = COL_GUST_WIND_SPEED + 2
    FIELD_WIND_DIRECTION = COL_WIND_DIRECTION + 2
    FIELD_UV_INDEX = 15
    FIELD_SOLAR_RADIATION = 16

    # Write the data file for gnuplot
    file_data = [
        '# timestamp\ttemperature\tdew point\tapparent temperature\twind chill'
        '\trelative humidity\tabsolute pressure\tindoor temperature'
        '\tindoor relative humidity\trainfall\taverage wind speed\tgust wind speed\twind direction\tuv index\tsolar radiation\n']
    FORMAT_STRING = '{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t{11}\t{12}\t{13}\t{14}\n'
    for record in temperature_data:
        # Handle missing data.
        if record[COL_PREV_SAMPLE_MISSING]:
            file_data.append(
                FORMAT_STRING.format(str(record[COL_PREV_TIMESTAMP]),
                                     '?', '?', '?', '?', '?', '?', '?', '?',
                                     '?', '?', '?', '?', '?', '?'))

        file_data.append(FORMAT_STRING.format(str(record[COL_TIMESTAMP]),
                                              str(record[COL_TEMPERATURE]),
                                              str(record[COL_DEW_POINT]),
                                              str(record[COL_APPARENT_TEMP]),
                                              str(record[COL_WIND_CHILL]),
                                              str(record[COL_REL_HUMIDITY]),
                                              str(record[COL_ABS_PRESSURE]),
                                              str(record[COL_INDOOR_TEMP]),
                                              str(record[
                                                  COL_INDOOR_REL_HUMIDITY]),
                                              str(record[COL_RAINFALL]),
                                              str(record[COL_AVG_WIND_SPEED]),
                                              str(record[COL_GUST_WIND_SPEED]),
                                              str(record[COL_WIND_DIRECTION]),
                                              str(record[COL_UV_INDEX]),
                                              str(record[COL_SOLAR_RADIATION])
        ))
    x_range = (str(temperature_data[0][COL_TIMESTAMP]),
               str(temperature_data[len(temperature_data) - 1][COL_TIMESTAMP]))
    data_filename = dest_dir + '7-day_gnuplot_data.dat'
    file = open(data_filename, 'w+')
    file.writelines(file_data)
    file.close()
    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 2340
            height = 600
        else:
            width = 1170    # Default width and height
            height = 300

        # Plot Temperature and Dew Point
        output_filename = dest_dir + '7-day_temperature_tdp'
        if large:
            output_filename = dest_dir + '7-day_temperature_tdp_large'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Temperature and Dew Point",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_TEMPERATURE, # Temperature
                           'title': "Temperature"},
                          {'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_DEW_POINT, # Dew Point
                           'title': "Dew Point"}],
                   output_format=output_format)

        # Plot Apparent Temperature and Wind Chill
        output_filename = dest_dir + '7-day_temperature_awc'
        if large:
            output_filename = dest_dir + '7-day_temperature_awc_large'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Date',
                   x_format='%d-%b',
                   timefmt_is_date=True,
                   title="Apparent Temperature and Wind Chill",
                   ylabel="Temperature (C)",
                   width=width,
                   height=height,
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_APPARENT_TEMP, # Apparent temperature
                           'title': "Apparent Temperature"},
                          {'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_WIND_CHILL, # Wind Chill
                           'title': "Wind Chill"}],
                   output_format=output_format)

        # Humidity
        output_filename = dest_dir + '7-day_humidity'
        if large:
            output_filename = dest_dir + '7-day_humidity_large'
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
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_REL_HUMIDITY, # Humidity
                           'title': "Relative Humidity"}],
                   output_format=output_format)

        # Indoor Humidity
        output_filename = dest_dir + '7-day_indoor_humidity'
        if large:
            output_filename = dest_dir + '7-day_indoor_humidity_large'
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
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_REL_HUMIDITY, # Indoor Humidity
                           'title': "Relative Humidity"}],
                   output_format=output_format)

        # Absolute Pressure
        output_filename = dest_dir + '7-day_pressure'
        if large:
            output_filename = dest_dir + '7-day_pressure_large'
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
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_ABS_PRESSURE, # Absolute Pressure
                           'title': "Absolute Pressure"}],
                   output_format=output_format)

        # Indoor Temperature
        output_filename = dest_dir + '7-day_indoor_temperature'
        if large:
            output_filename = dest_dir + '7-day_indoor_temperature_large'
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
                   x_range=x_range,
                   lines=[{'filename': data_filename,
                           'xcol': FIELD_TIMESTAMP, # Time
                           'ycol': FIELD_INDOOR_TEMP, # Indoor Temperature
                           'title': "Temperature"}],
                   output_format=output_format)

        # Solar Radiation
        if hw_config.has_solar_sensor:
            output_filename = dest_dir + '7-day_solar_radiation'
            if large:
                output_filename = dest_dir + '7-day_solar_radiation_large'
            plot_graph(output_filename,
                       xdata_is_time=True,
                       xlabel='Date',
                       x_format='%d-%b',
                       timefmt_is_date=True,
                       title="Solar Radiation",
                       ylabel="W/m^2",
                       key=False,
                       width=width,
                       height=height,
                       x_range=x_range,
                       lines=[{'filename': data_filename,
                               'xcol': FIELD_TIMESTAMP,  # Time
                               'ycol': FIELD_SOLAR_RADIATION,
                               'title': "Solar Radiation"}],
                       output_format=output_format)

        # UV Index
        if hw_config.has_uv_sensor:
            output_filename = dest_dir + '7-day_uv_index'
            if large:
                output_filename = dest_dir + '7-day_uv_index_large'
            plot_graph(output_filename,
                       xdata_is_time=True,
                       xlabel='Date',
                       x_format='%d-%b',
                       timefmt_is_date=True,
                       title="UV Index",
                       ylabel="",
                       key=False,
                       width=width,
                       height=height,
                       x_range=x_range,
                       lines=[{'filename': data_filename,
                               'xcol': FIELD_TIMESTAMP,  # Time
                               'ycol': FIELD_UV_INDEX,
                               'title': "UV Index"}],
                       output_format=output_format)


def rainfall_7_day(cur, dest_dir, plot_date, station_code, output_format):
    """
    Plots 7-day hourly rainfall charts

    The charts this generates are fairly unreadable at the moment. It needs to
    be 2-hourly rainfall or maybe even lower resolution than that. X labels
    probably can't be included at all.

    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param day: Day to chart for
    :param month: Month to chart for
    :param year: Year to chart for
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param output_format: The output format (eg, "pngcairo")
    :type output_format: str
    """

    cur.execute("""
select date_trunc('hour', s.time_stamp)  as time_stamp,
       coalesce(sum(s.rainfall),0)
    from sample s
inner join station st on st.station_id = s.station_id
inner join (select max(time_stamp) as ts, station_id from sample
            where time_stamp::date = %s group by station_id) as max_ts
        on s.time_stamp <= max_ts.ts     -- 604800 seconds in a week.
       and s.time_stamp >= (max_ts.ts - (604800 * '1 second'::interval))
  where st.code = %s
    group by date_trunc('hour', s.time_stamp)
    order by date_trunc('hour', s.time_stamp) asc
    """, (plot_date, station_code,))
    rainfall_data = cur.fetchall()

    # Columns in the query
    COL_HOUR = 0
    COL_RAINFALL = 1

    # Write the data file for gnuplot
    file_data = ['# hour\trainfall\n']
    FORMAT_STRING = '{0}\t{1}\n'
    rain_total = 0
    for record in rainfall_data:
        # TODO: Handle missing data
        file_data.append(FORMAT_STRING.format(str(record[COL_HOUR]),
                                              str(record[COL_RAINFALL])))
        rain_total += record[COL_RAINFALL]

    data_filename = dest_dir + '7-day_hourly_rainfall.dat'
    data_file = open(data_filename, 'w+')
    data_file.writelines(file_data)
    data_file.close()

    empty = rain_total == 0.0

    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 1140
            height = 600
        else:
            width = 570    # Default width and height
            height = 300

        # Plot Temperature and Dew Point
        output_filename = dest_dir + '7-day_hourly_rainfall'
        if large:
            output_filename = dest_dir + '7-day_hourly_rainfall_large'

        plot_rainfall(data_filename,
                      output_filename,
                      "Hourly Rainfall",
                      width,
                      height,
                      empty,
                      columns='3:xtic(strcol(1)." ".strcol(2))',
                      output_format=output_format)