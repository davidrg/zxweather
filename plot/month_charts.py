from datetime import date
from gnuplot import plot_graph

__author__ = 'David Goodwin'


def month_charts(cur, dest_dir, month, year, station_code, output_format,
                 hw_config, write_data):
    """
    Charts detailing weather for a single month.
    :param cur: Database cursor
    :param dest_dir: Directory to write images to
    :param month: Month to chart for
    :param year: Year to chart for
    :param station_code: The code for the station to plot data for
    :type station_code: str
    :param output_format: The output format (eg, "pngcairo")
    :type output_format: str
    :param hw_config: station hardware configuration details
    :type hw_config: weatherplot.StationConfig
    :return:
    """

    data_filename = dest_dir + 'gnuplot_data.dat'

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

    if write_data:
        cur.execute("""select cur.time_stamp,
       round(cur.temperature::numeric, 2),
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
inner join sample prev on prev.station_id = cur.station_id
        and prev.time_stamp = (
            select max(pm.time_stamp)
            from sample pm
            where pm.time_stamp < cur.time_stamp
            and pm.station_id = cur.station_id)
inner join station s on s.station_id = cur.station_id
left outer join davis_sample ds on ds.sample_id = cur.sample_id
where date(date_trunc('month',cur.time_stamp)) = %s
  and lower(s.code) = lower(%s)
order by cur.time_stamp asc""", (date(year, month, 1), station_code))

        weather_data = cur.fetchall()

        # Write the data file for gnuplot
        file_data = [
            '# timestamp\ttemperature\tdew point\tapparent temperature\twind chill'
            '\trelative humidity\tabsolute pressure\tindoor temperature'
            '\tindoor relative humidity\trainfall\taverage wind speed\tgust wind speed\twind direction\tuv index\tsolar radiation\n']

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
        file = open(data_filename, 'w+')
        file.writelines(file_data)
        file.close()
    else:
        cur.execute("""select min(cur.time_stamp) min_ts, max(cur.time_stamp) as max_ts
        from sample cur
        inner join station s on s.station_id = cur.station_id
        left outer join davis_sample ds on ds.sample_id = cur.sample_id
        where date(date_trunc('month',cur.time_stamp)) = %s
          and lower(s.code) = lower(%s)""", (date(year, month, 1), station_code))

        weather_data = cur.fetchone()

        x_range = (str(weather_data[0]),
                   str(weather_data[1]))

    for large in [True, False]:
        # Create both large and regular versions of each plot.
        if large:
            width = 2340
            height = 600
        else:
            width = 1170
            height = 300

        # Plot Temperature and Dew Point
        output_filename = dest_dir + 'temperature_tdp'
        if large:
            output_filename = dest_dir + 'temperature_tdp_large'

        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'temperature_awc'
        if large:
            output_filename = dest_dir + 'temperature_awc_large'
        plot_graph(output_filename,
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'humidity'
        if large:
            output_filename = dest_dir + 'humidity_large'
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
                   xdata_is_time=True,
                   xlabel='Day of Month',
                   x_format='%d',
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
        output_filename = dest_dir + 'indoor_temperature'
        if large:
            output_filename = dest_dir + 'indoor_temperature_large'
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
                       xdata_is_time=True,
                       xlabel='Day of Month',
                       x_format='%d',
                       timefmt_is_date=True,
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
                       xdata_is_time=True,
                       xlabel='Day of Month',
                       x_format='%d',
                       timefmt_is_date=True,
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