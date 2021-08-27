# coding=utf-8
"""
Functions for encoding and decoding LOOP packets.
"""

from collections import namedtuple
import datetime
import struct
from davis_logger.record_types.util import CRC, ms_to_mph, mm_to_inch, \
    inch_to_mm, mph_to_ms, inhg_to_mb, mb_to_inhg, deserialise_8bit_temp, \
    serialise_8bit_temp, undash_8bit, dash_8bit, deserialise_16bit_temp, \
    serialise_16bit_temp, f_to_c, c_to_f

__author__ = 'david'

Loop = namedtuple(
    'Loop',
    (
        'barTrend',
        'nextRecord',
        'barometer',
        'insideTemperature',
        'insideHumidity',
        'outsideTemperature',
        'windSpeed',
        'averageWindSpeed10min',
        'windDirection',
        'extraTemperatures',
        'soilTemperatures',
        'leafTemperatures',
        'outsideHumidity',
        'extraHumidities',
        'rainRate',
        'UV',
        'solarRadiation',
        'stormRain',
        'startDateOfCurrentStorm',
        'dayRain',
        'monthRain',
        'yearRain',
        'dayET',
        'monthET',
        'yearET',
        'soilMoistures',
        'leafWetness',
        'insideAlarms',
        'rainAlarms',
        'outsideAlarms',
        'extraTempHumAlarms',
        'soilAndLeafAlarms',
        'transmitterBatteryStatus',
        'consoleBatteryVoltage',
        'forecastIcons',
        'forecastRuleNumber',
        'timeOfSunrise',
        'timeOfSunset',
    )
)

Loop2 = namedtuple(
    'Loop2',
    (
        'barTrend',
        'barometer',
        'insideTemperature',
        'insideHumidity',
        'outsideTemperature',
        'windSpeed',
        'windDirection',
        'averageWindSpeed10min',  # Higher resolution than in Loop
        'averageWindSpeed2min',
        'windGust10m',
        'windGust10mDirection',
        'dewPoint',
        'outsideHumidity',
        'heatIndex',
        'windChill',
        'thswIndex',
        'rainRate',
        'UV',
        'solarRadiation',
        'stormRain',
        'startDateOfCurrentStorm',
        'dayRain',
        'last15minRain',
        'lastHourRain',
        'dayET',
        'last24hourRain',
        'barometricReductionMethod',
        'userBarometricOffset',
        'barometricCalibrationNumber',
        'barometricSensorRaw',
        'absoluteBarometricPressure',
        'altimeterSetting',

        # These are all pointers to locations within the console/envoy EEPROM
        # where the graph is stored. The information is useless unless you're
        # going to query the EEPROM further which this application will never
        # do. They're only here to help test the serialise & deserialise
        # functions using real data from a VP2 console.
        'next10mWindSpeedGraphPointer',
        'next15mWindSpeedGraphPointer',
        'nextHourlyWindSpeedGraphPointer',
        'nextDailyWindSpeedGraphPointer',
        'nextMinuteRainGraphPointer',
        'nextRainStormGraphPointer',
        'indexToMinuteWithinHour',
        'nextMonthlyRain',
        'nextYearlyRain',
        'nextSeasonalRain',

        # This purpose and contents of this field is undefined. Its included
        # here for the same reason as the graph pointers - to allow data to pass
        # through the serialise and deserialise functions without any detectable
        # changes.
        'undefinedField'
    )
)


def decode_current_storm_date(binary_val):
    """
    Decodes the current storm start date.
    :param binary_val: Current storm date as a 16bit int. Or just an int.
    :type binary_val: int
    :return:
    """

    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    # | MONTH - 0xF000| DAY - 0xF80           | YEAR - 0x7F               |
    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    #    15  14  13  11  12  11  10   9   8   7   6   5   4   3   2   1   0

    # 0xFFFF (-1 as a signed short) is the dashed value.
    if binary_val == 0xFFFF:
        return None

    month_mask = 0xF000
    day_mask = 0x0F80
    year_mask = 0x007F

    month = (binary_val & month_mask) >> 12
    day = (binary_val & day_mask) >> 7
    year = 2000 + (
        binary_val & year_mask)  # TODO: check that this can't be negative

    return datetime.date(year=year, month=month, day=day)


def encode_current_storm_date(storm_date):
    """
    Converts the supplied date to the 16bit value used for the current storm
    date.
    :param storm_date: Date to encode
    :type storm_date: datetime.date
    :return: Encodes the supplied date in the format used for the current
    storm start date.
    :rtype: int
    """

    # -1 (0xFFFF) is the dashed value.
    if storm_date is None:
        return 0xFFFF

    year = storm_date.year
    year -= 2000
    month = storm_date.month
    day = storm_date.day

    value = (month << 12) + (day << 7) + year

    return value


def decode_time(int_time):
    """
    Converts the supplied integer to time.
    :param int_time: The time as an integer (3 or 4 digits)
    :type int_time: int
    :return: The time as a time object
    :rtype: datetime.time
    """
    string = str(int_time)

    if len(string) == 3:
        string = '0' + string

    if len(string) != 4:
        raise Exception('invalid timestamp: {0}'.format(int_time))

    hour = string[0:2]
    minute = string[2:]

    return datetime.time(hour=int(hour), minute=int(minute))


def encode_time(timestamp):
    """
    Encodes the supplied timestamp as a single 16bit integer.
    :param timestamp: The timestamp to encode
    :type timestamp: datetime.time
    :return: The timestamp as an integer
    :rtype: int
    """

    hour = int(timestamp.hour)
    minute = int(timestamp.minute)

    return hour * 100 + minute


def loop_fmt(split_arrays=False):
    """
    Builds the loop format string
    :param split_arrays: If a separate character should be included for each field (no BBB instead of 3B)
    :type split_arrays: bool
    :return: struct format string
    :rtype: str
    """
    alignment = '<'
    parts = [
        ('3s', 'Magic number ("LOO")'),
        ('b', 'Bar trend'),
        ('B', 'Packet type'),
        ('H', 'Next record ID'),  # ('h', 'Next record ID'),
        ('H', 'Barometer'),  # ('h', 'Barometer'),
        ('h', 'Inside temperature'),
        ('B', 'Inside humidity'),
        ('h', 'Outside temperature'),
        ('B', 'Wind speed'),
        ('B', 'Average wind speed 10m'),
        ('H', 'Wind direction'),  # ('h', 'Wind direction'),
        ('7B', 'Extra temperatures 1-7'),
        ('4B', 'Soil temperatures 1-4'),
        ('4B', 'Leaf temperatures 1-4'),
        ('B', 'Outside humidity'),
        ('7B', 'Extra humidities 1-7'),
        ('H', 'Rain rate'),  # ('h', 'Rain rate'),
        ('B', 'UV index'),
        ('H', 'Solar radiation'),  # ('h', 'Solar radiation'),
        ('H', 'Storm rain'),  # ('h', 'Storm rain'),
        ('H', 'Current storm start date'),  # ('h', 'Current storm start date'),
        ('H', 'Day rain'),  # ('h', 'Day rain'),
        ('H', 'Month rain'),  # ('h', 'Month rain'),
        ('H', 'Year rain'),  # ('h', 'Year rain'),
        ('H', 'Day ET'),  # ('h', 'Day ET'),
        ('H', 'Month ET'),  # ('h', 'Month ET'),
        ('H', 'Year ET'),  # ('h', 'Year ET'),
        ('4B', 'Soil moisture 1-4'),
        ('4B', 'Leaf wetness 1-4'),
        ('B', 'Inside alarms'),
        ('B', 'Rain alarms'),
        ('2B', 'Outside alarms'),
        ('8B', 'Extra temperature+humidity alarms 1-8'),
        ('4B', 'Leaf+Soil alarms 1-4'),
        ('B', 'TX battery status'),
        ('H', 'Console battery voltage'),  # ('h', 'Console battery voltage'),
        ('B', 'Forecast icons'),
        ('B', 'Forecast rule number'),
        ('H', 'Time of sunrise'),
        ('H', 'Time of sunset'),
        ('2s', 'Terminator'),
    ]

    if split_arrays:
        x = []
        for item in parts:
            fmt = item[0]
            desc = item[1]

            try:
                count = int(fmt[0])
                f = fmt[1]
                fmt = f * count
            except ValueError:
                pass  # Don't care
            x.append((fmt, desc))
        parts = x

    return alignment + ''.join([x[0] for x in parts])


def loop2_fmt():
    """
    Builds the loop2 format string
    :return: struct format string
    :rtype: str
    """
    alignment = '<'
    parts = [
        ('3s', 'Magic number ("LOO")'),
        ('b', 'Bar trend'),
        ('B', 'Packet type'),
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Barometer'),
        ('h', 'Inside temperature'),
        ('B', 'Inside humidity'),
        ('h', 'Outside temperature'),
        ('B', 'Wind speed'),
        ('B', 'Reserved (1 byte)'),  # Filled with 0xFF
        ('H', 'Wind direction'),

        # 0.1mph resolution rather than 1mph resolution as in LOOP1
        ('H', 'Average wind speed, 10m'),
        ('H', 'Average wind speed, 2m'),
        ('H', '10m Wind Gust'),
        ('H', '10m Wind Gust direction'),
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Dew point'),
        ('B', 'Reserved (1 byte)'),  # Filled with 0xFF
        ('B', 'Outside humidity'),
        ('B', 'Reserved (1 byte)'),  # Filled with 0xFF
        ('h', 'Heat index'),  # Whole degrees F, 255 = dashed
        ('h', 'Wind chill'),  # Whole degrees F, 255 = dashed
        ('h', 'THSW index'),  # Whole degrees F, 255 = dashed
        ('H', 'Rain rate'),
        ('B', 'UV index'),
        ('H', 'Solar radiation'),
        ('H', 'Storm rain'),
        ('H', 'Current storm start date'),
        ('H', 'Day rain'),
        ('H', 'Last 15-min rain'),
        ('H', 'Last hour rain'),
        ('H', 'Day ET'),
        ('H', 'Last 24-hour rain'),
        ('B', 'Barometric reduction method'),  # 0=user offset, 1=Altimeter, 2=NOAA (VP2)
        ('H', 'User-entered barometric offset'),  # 1000th of an inch
        ('H', 'Barometric calibration number'),  # Calibration offset in 1000th of an inch
        ('H', 'Barometric sensor raw reading'),  # In 1000th of an inch
        ('H', 'Absolute barometric pressure'),  # In 1000th of an inch. Equals raw+user offset
        ('H', 'Altimeter setting'),  # In 1000th of an inch
        ('B', 'Reserved (1 byte)'),  # 0xFF
        ('B', 'Reserved (1 byte)'),  # Undefined

        # Why are graph points useful? Surely anything that could query
        # the station for graph data is perfectly capable fo producing the
        # data itself.
        ('B', 'Next 10m wind speed graph pointer'),
        ('B', 'Next 15m wind speed graph pointer'),
        ('B', 'Next hourly wind speed graph pointer'),
        ('B', 'Next daily wind speed graph pointer'),
        ('B', 'Next minute rain graph pointer'),
        ('B', 'Next rain storm graph pointer'),
        ('B', 'Index to the minute within an hour'),
        ('B', 'Next monthly rain'),
        ('B', 'Next yearly rain'),
        ('B', 'Next seasonal rain'),

        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF
        ('H', 'Reserved (2 bytes)'),  # Filled with 0x7FFF

        ('2s', 'Terminator'),
    ]

    return alignment + ''.join([x[0] for x in parts])


PACKET_TYPE_LOOP = 0
PACKET_TYPE_LOOP2 = 1


def get_packet_type(loop_packet):
    """

    :param loop_packet:
    :return:
    """

    fmt = '<3sbB'

    magic, bar_trend, packet_type = struct.unpack(fmt, loop_packet[:5])

    return packet_type


def deserialise_loop(loop_string, rain_collector_size=0.2):
    """
    Takes a LOOP packet from the console and converts it into a namedtuple.
    :param loop_string: 97-character string from the console (packet minus CRC)
    :type loop_string: str
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :return: loop packet
    :rtype: Loop
    """
    #  loop_format = '<3sbBhhhBhBBh7B4B4BB7BhBhhhhhhhhh4B4BBB2B8B4BBhBBHH2s'
    loop_format = loop_fmt()

    # Here we unpack the loop packet using that nasty format string above.
    # Oh what a lot of variables.
    magic, bar_trend, packet_type, next_record, barometer, inside_temperature, \
        inside_humidity, outside_temperature, wind_speed, \
        average_wind_speed_10m, wind_direction, ext_temp_1, ext_temp_2, \
        ext_temp_3, ext_temp_4, ext_temp_5, ext_temp_6, ext_temp_7, \
        soil_temp_1, soil_temp_2, soil_temp_3, soil_temp_4, leaf_temp_1, \
        leaf_temp_2, leaf_temp_3, leaf_temp_4, outside_humidity, ext_humid_1, \
        ext_humid_2, ext_humid_3, ext_humid_4, ext_humid_5, ext_humid_6, \
        ext_humid_7, rain_rate, uv, solar_radiation, storm_rain, \
        current_storm_start_date, day_rain, month_rain, year_rain, \
        day_et, month_et, year_et, soil_moisture_1, soil_moisture_2, \
        soil_moisture_3, soil_moisture_4, leaf_wetness_1, leaf_wetness_2, \
        leaf_wetness_3, leaf_wetness_4, inside_alarms, rain_alarms, \
        outside_alarms_a, outside_alarms_b, ext_th_alarms_1, ext_th_alarms_2, \
        ext_th_alarms_3, ext_th_alarms_4, ext_th_alarms_5, ext_th_alarms_6, \
        ext_th_alarms_7, ext_th_alarms_8, soil_leaf_alarm_1, soil_leaf_alarm_2,\
        soil_leaf_alarm_3, soil_leaf_alarm_4, tx_battery_status, \
        console_battery_voltage, forecast_icons, forecast_rule_number, \
        time_of_sunrise, time_of_sunset, \
        term_chars = struct.unpack(loop_format, loop_string)

    if solar_radiation == 32767:
        solar_radiation = None

    decoded_uv = undash_8bit(uv)
    if decoded_uv is not None:
        decoded_uv /= 10.0

    if bar_trend == 80:
        # Revision A LOOP packet - offset 3 is the letter 'P' instead of the
        # 3-hour barometer trend. This makes for a header of "LOOP". No bar
        # trend is available for this station.
        bar_trend = None

    # Now pack all those variables into something a little more workable.
    # We'll to unit conversions, etc, at the same time.
    loop = Loop(
        barTrend=bar_trend,
        nextRecord=next_record,
        barometer=inhg_to_mb(barometer / 1000.0),
        insideTemperature=deserialise_16bit_temp(inside_temperature),
        insideHumidity=undash_8bit(inside_humidity),
        outsideTemperature=deserialise_16bit_temp(outside_temperature),
        windSpeed=mph_to_ms(wind_speed),
        averageWindSpeed10min=mph_to_ms(average_wind_speed_10m),
        windDirection=wind_direction,
        extraTemperatures=[
            deserialise_8bit_temp(ext_temp_1),
            deserialise_8bit_temp(ext_temp_2),
            deserialise_8bit_temp(ext_temp_3),
            deserialise_8bit_temp(ext_temp_4),
            deserialise_8bit_temp(ext_temp_5),
            deserialise_8bit_temp(ext_temp_6),
            deserialise_8bit_temp(ext_temp_7)],
        soilTemperatures=[
            deserialise_8bit_temp(soil_temp_1),
            deserialise_8bit_temp(soil_temp_2),
            deserialise_8bit_temp(soil_temp_3),
            deserialise_8bit_temp(soil_temp_4)],
        leafTemperatures=[
            deserialise_8bit_temp(leaf_temp_1),
            deserialise_8bit_temp(leaf_temp_2),
            deserialise_8bit_temp(leaf_temp_3),
            deserialise_8bit_temp(leaf_temp_4)],
        outsideHumidity=undash_8bit(outside_humidity),
        extraHumidities=[
            undash_8bit(ext_humid_1),
            undash_8bit(ext_humid_2),
            undash_8bit(ext_humid_3),
            undash_8bit(ext_humid_4),
            undash_8bit(ext_humid_5),
            undash_8bit(ext_humid_6),
            undash_8bit(ext_humid_7)],
        rainRate=rain_rate * rain_collector_size,
        UV=decoded_uv,
        solarRadiation=solar_radiation,
        # Manual says this is 100ths of an inch. The manual is wrong:
        stormRain=storm_rain * rain_collector_size,
        startDateOfCurrentStorm=decode_current_storm_date(
            current_storm_start_date),
        dayRain=day_rain * rain_collector_size,
        monthRain=month_rain * rain_collector_size,
        yearRain=year_rain * rain_collector_size,
        dayET=inch_to_mm(day_et * 1000),
        monthET=inch_to_mm(month_et * 100),
        yearET=inch_to_mm(year_et * 100),
        soilMoistures=[
            undash_8bit(soil_moisture_1),
            undash_8bit(soil_moisture_2),
            undash_8bit(soil_moisture_3),
            undash_8bit(soil_moisture_4)],
        leafWetness=[
            undash_8bit(leaf_wetness_1),
            undash_8bit(leaf_wetness_2),
            undash_8bit(leaf_wetness_3),
            undash_8bit(leaf_wetness_4)],
        insideAlarms=inside_alarms,
        rainAlarms=rain_alarms,
        outsideAlarms=[
            outside_alarms_a,
            outside_alarms_b],
        extraTempHumAlarms=[
            ext_th_alarms_1,
            ext_th_alarms_2,
            ext_th_alarms_3,
            ext_th_alarms_4,
            ext_th_alarms_5,
            ext_th_alarms_6,
            ext_th_alarms_7,
            ext_th_alarms_8],
        soilAndLeafAlarms=[
            soil_leaf_alarm_1,
            soil_leaf_alarm_2,
            soil_leaf_alarm_3,
            soil_leaf_alarm_4],
        transmitterBatteryStatus=tx_battery_status,
        consoleBatteryVoltage=((console_battery_voltage * 300) / 512.0) / 100.0,
        forecastIcons=forecast_icons,
        forecastRuleNumber=forecast_rule_number,
        timeOfSunrise=decode_time(time_of_sunrise),
        timeOfSunset=decode_time(time_of_sunset)
    )
    return loop


def serialise_loop(loop, rain_collector_size=0.2, include_crc=True):
    """
    Converts LOOP data into the string representation used by the console
    :param loop: Loop data
    :type loop: Loop
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :param include_crc: Calculate the CRC code and return with the Loop data
    :type include_crc: bool
    :returns: The loop thing as a string
    :rtype: bytes
    """

    loop_format = loop_fmt()

    if loop.solarRadiation is None:
        solar_radiation = 32767
    else:
        solar_radiation = loop.solarRadiation

    if loop.UV is None:
        uv = 255
    else:
        uv = int(round(loop.UV * 10.0, 0))

    barometer = 0
    if loop.barometer is not None:
        barometer = int(round(mb_to_inhg(loop.barometer * 1000), 0))

    result = bytearray()

    result.extend(struct.pack(
        loop_format,
        b'LOO',  # Magic number
        loop.barTrend,
        0,  # Packet type. 0 = LOOP
        loop.nextRecord,
        barometer,
        serialise_16bit_temp(loop.insideTemperature),
        dash_8bit(loop.insideHumidity),
        serialise_16bit_temp(loop.outsideTemperature),
        int(round(ms_to_mph(loop.windSpeed), 0)),
        int(round(ms_to_mph(loop.averageWindSpeed10min), 0)),
        loop.windDirection,
        serialise_8bit_temp(loop.extraTemperatures[0]),
        serialise_8bit_temp(loop.extraTemperatures[1]),
        serialise_8bit_temp(loop.extraTemperatures[2]),
        serialise_8bit_temp(loop.extraTemperatures[3]),
        serialise_8bit_temp(loop.extraTemperatures[4]),
        serialise_8bit_temp(loop.extraTemperatures[5]),
        serialise_8bit_temp(loop.extraTemperatures[6]),
        serialise_8bit_temp(loop.soilTemperatures[0]),
        serialise_8bit_temp(loop.soilTemperatures[1]),
        serialise_8bit_temp(loop.soilTemperatures[2]),
        serialise_8bit_temp(loop.soilTemperatures[3]),
        serialise_8bit_temp(loop.leafTemperatures[0]),
        serialise_8bit_temp(loop.leafTemperatures[1]),
        serialise_8bit_temp(loop.leafTemperatures[2]),
        serialise_8bit_temp(loop.leafTemperatures[3]),
        dash_8bit(loop.outsideHumidity),
        dash_8bit(loop.extraHumidities[0]),
        dash_8bit(loop.extraHumidities[1]),
        dash_8bit(loop.extraHumidities[2]),
        dash_8bit(loop.extraHumidities[3]),
        dash_8bit(loop.extraHumidities[4]),
        dash_8bit(loop.extraHumidities[5]),
        dash_8bit(loop.extraHumidities[6]),
        int(round(loop.rainRate / rain_collector_size, 0)),
        uv,
        solar_radiation,
        int(round(loop.stormRain / rain_collector_size, 0)),
        encode_current_storm_date(loop.startDateOfCurrentStorm),
        int(round(loop.dayRain / rain_collector_size, 0)),
        int(round(loop.monthRain / rain_collector_size, 0)),
        int(round(loop.yearRain / rain_collector_size, 0)),
        int(round(mm_to_inch(loop.dayET) / 1000.0, 0)),
        int(round(mm_to_inch(loop.monthET) / 100.0, 0)),
        int(round(mm_to_inch(loop.yearET) / 100.0, 0)),
        dash_8bit(loop.soilMoistures[0]),
        dash_8bit(loop.soilMoistures[1]),
        dash_8bit(loop.soilMoistures[2]),
        dash_8bit(loop.soilMoistures[3]),
        dash_8bit(loop.leafWetness[0]),
        dash_8bit(loop.leafWetness[1]),
        dash_8bit(loop.leafWetness[2]),
        dash_8bit(loop.leafWetness[3]),
        loop.insideAlarms,  # Inside alarms
        loop.rainAlarms,  # Rain alarms
        loop.outsideAlarms[0],  # Outside alarms A,
        loop.outsideAlarms[1],  # Outside alarms B,
        loop.extraTempHumAlarms[0],
        loop.extraTempHumAlarms[1],
        loop.extraTempHumAlarms[2],
        loop.extraTempHumAlarms[3],
        loop.extraTempHumAlarms[4],
        loop.extraTempHumAlarms[5],
        loop.extraTempHumAlarms[6],
        loop.extraTempHumAlarms[7],
        loop.soilAndLeafAlarms[0],
        loop.soilAndLeafAlarms[1],
        loop.soilAndLeafAlarms[2],
        loop.soilAndLeafAlarms[3],
        loop.transmitterBatteryStatus,
        int(round(((loop.consoleBatteryVoltage / 300.0) * 512) * 100, 0)),
        loop.forecastIcons,
        loop.forecastRuleNumber,
        encode_time(loop.timeOfSunrise),
        encode_time(loop.timeOfSunset),
        b'\n\r'
    ))

    if include_crc:
        crc = CRC.calculate_crc(result)
        packed_crc = struct.pack(CRC.FORMAT, crc)

        result.extend(packed_crc)

    return result


def deserialise_loop2(loop2_string, rain_collector_size=0.2):
    """
    Takes a LOOP2 packet from the console and converts it into a namedtuple.
    :param loop2_string: 97-character string from the console (packet minus CRC)
    :type loop2_string: bytes
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :return: loop2 packet
    :rtype: Loop2
    """
    loop2_format = loop2_fmt()

    magic, bar_trend, packet_type, reserved_a, barometer, inside_temperature, \
        inside_humidity, outside_temperature, wind_speed, reserved_b, \
        wind_direction, average_wind_speed_10m, average_wind_speed_2m, \
        wind_gust_10m, wind_gust_direction_10m, reserved_c, reserved_d, dew_point, \
        reserved_e, outside_humidity, reserved_f, heat_index, wind_chill, \
        thsw_index, rain_rate, uv_index, solar_radiation, storm_rain, \
        current_storm_start_date, day_rain, last_15_min_rain, last_hour_rain, \
        day_et, last_24_hour_rain, barometric_reduction_method, \
        user_entered_barometric_offset, barometric_calibration_number, \
        barometric_sensor_raw_reading, absolute_barometric_pressure, \
        altimeter_setting, reserved_g, reserved_h, \
        next_10m_wind_speed_graph_pointer, next_15m_wind_speed_graph_pointer, \
        next_hourly_wind_speed_graph_pointer, next_daily_wind_speed_graph_pointer, \
        next_minute_rain_graph_pointer, next_rain_storm_graph_pointer, \
        index_to_the_minute_within_an_hour, next_monthly_rain, next_yearly_rain, \
        next_seasonal_rain, reserved_i, reserved_j, reserved_k, reserved_l, \
        reserved_m, reserved_n, terminator = struct.unpack(loop2_format,
                                                           loop2_string)

    if solar_radiation == 32767:
        solar_radiation = None

    decoded_uv = undash_8bit(uv_index)
    if decoded_uv is not None:
        decoded_uv /= 10.0

    if bar_trend == 80:
        # Revision A LOOP packet - offset 3 is the letter 'P' instead of the
        # 3-hour barometer trend. This makes for a header of "LOOP". No bar
        # trend is available for this station.
        bar_trend = None

    def _255_dashed_16bit_temp(temp):
        if temp == 255:
            return None
        return f_to_c(temp)

    # Now pack all those variables into something a little more workable.
    # We'll to unit conversions, etc, at the same time.
    loop = Loop2(
        barTrend=bar_trend,
        barometer=inhg_to_mb(barometer / 1000.0),
        insideTemperature=deserialise_16bit_temp(inside_temperature),
        insideHumidity=undash_8bit(inside_humidity),
        outsideTemperature=deserialise_16bit_temp(outside_temperature),
        windSpeed=mph_to_ms(wind_speed),

        # The doc says this is at 0.1mph resolution. It is apparently wrong:
        # https://www.wxforum.net/index.php?topic=22399.0
        averageWindSpeed10min=mph_to_ms(average_wind_speed_10m),
        windDirection=wind_direction,
        outsideHumidity=undash_8bit(outside_humidity),
        rainRate=rain_rate * rain_collector_size,
        UV=decoded_uv,
        solarRadiation=solar_radiation,
        # Manual says this is 100ths of an inch. The manual is wrong:
        stormRain=storm_rain * rain_collector_size,
        startDateOfCurrentStorm=decode_current_storm_date(
            current_storm_start_date),
        dayRain=day_rain * rain_collector_size,
        dayET=inch_to_mm(day_et * 1000),

        # These are unique to the Loop2 packet
        averageWindSpeed2min=mph_to_ms(average_wind_speed_2m),
        windGust10m=mph_to_ms(wind_gust_10m),
        windGust10mDirection=wind_gust_direction_10m,
        dewPoint=_255_dashed_16bit_temp(dew_point),
        heatIndex=_255_dashed_16bit_temp(heat_index),
        windChill=_255_dashed_16bit_temp(wind_chill),
        thswIndex=_255_dashed_16bit_temp(thsw_index),
        last15minRain=last_15_min_rain * rain_collector_size,
        lastHourRain=last_hour_rain * rain_collector_size,
        last24hourRain=last_24_hour_rain * rain_collector_size,
        barometricReductionMethod=barometric_reduction_method,
        userBarometricOffset=inhg_to_mb(user_entered_barometric_offset / 1000.0),
        barometricCalibrationNumber=inhg_to_mb(barometric_calibration_number / 1000.0),
        barometricSensorRaw=inhg_to_mb(barometric_sensor_raw_reading / 1000.0),
        absoluteBarometricPressure=inhg_to_mb(absolute_barometric_pressure / 1000.0),
        altimeterSetting=inhg_to_mb(altimeter_setting / 1000.0),

        # These are useless but we've got them here so we can round-trip real
        # data from a VP2 console through the serialise function
        next10mWindSpeedGraphPointer=next_10m_wind_speed_graph_pointer,
        next15mWindSpeedGraphPointer=next_15m_wind_speed_graph_pointer,
        nextHourlyWindSpeedGraphPointer=next_hourly_wind_speed_graph_pointer,
        nextDailyWindSpeedGraphPointer=next_daily_wind_speed_graph_pointer,
        nextMinuteRainGraphPointer=next_minute_rain_graph_pointer,
        nextRainStormGraphPointer=next_rain_storm_graph_pointer,
        indexToMinuteWithinHour=index_to_the_minute_within_an_hour,
        nextMonthlyRain=next_monthly_rain,
        nextYearlyRain=next_yearly_rain,
        nextSeasonalRain=next_seasonal_rain,

        # This "undefined" field seems to increment every 30 LOOP2 packets
        # (possibly it increments every 30 packets of any type). As packets are
        # sent every 2 seconds it would appear this field is counting minutes.
        # It counts from 0 up to 9 then wraps back around to 0.
        # We're only carrying its value around for the same reason as the graph
        # pointers: so we can pass data from the station through
        # deserialise-> serialise and check we've got identical data at the
        # other end.
        undefinedField=reserved_h
    )
    return loop


def serialise_loop2(loop2, rain_collector_size=0.2, include_crc=True):
    """
    Converts LOOP2 data into the string representation used by the console
    :param loop2: Loop2 data
    :type loop2: Loop2
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :param include_crc: Calculate the CRC code and return with the Loop data
    :type include_crc: bool
    :returns: The loop thing as a string
    :rtype: bytes
    """

    loop2_format = loop2_fmt()

    if loop2.solarRadiation is None:
        solar_radiation = 32767
    else:
        solar_radiation = loop2.solarRadiation

    if loop2.UV is None:
        uv = 255
    else:
        uv = int(round(loop2.UV * 10.0, 0))

    barometer = 0
    if loop2.barometer is not None:
        barometer = int(round(mb_to_inhg(loop2.barometer * 1000), 0))

    # "The value is a signed two byte value in whole degrees F
    def _255_dash_16bit_temp(temp):
        if temp is None:
            return 255
        return int(round(c_to_f(temp), 0))

    result = bytearray()

    result.extend(struct.pack(
        loop2_format,
        b'LOO',  # Magic number
        loop2.barTrend,
        1,  # Packet type. 1 = LOOP2
        0x7FFF,  # 2-byte reserved
        barometer,
        serialise_16bit_temp(loop2.insideTemperature),
        dash_8bit(loop2.insideHumidity),
        serialise_16bit_temp(loop2.outsideTemperature),
        int(round(ms_to_mph(loop2.windSpeed), 0)),
        0xFF,  # 1-byte reserved
        loop2.windDirection,

        # The doc says these are at 0.1mph resolution. It is apparently wrong:
        # https://www.wxforum.net/index.php?topic=22399.0
        int(round(ms_to_mph(loop2.averageWindSpeed10min), 0)),
        int(round(ms_to_mph(loop2.averageWindSpeed2min), 0)),
        int(round(ms_to_mph(loop2.windGust10m), 0)),
        loop2.windGust10mDirection,
        0x7FFF,  # 2-byte reserved
        0x7FFF,  # 2-byte reserved
        _255_dash_16bit_temp(loop2.dewPoint),
        0xFF,  # 1-byte reserved
        dash_8bit(loop2.outsideHumidity),
        0xFF,  # 1-byte reserved
        _255_dash_16bit_temp(loop2.heatIndex),
        _255_dash_16bit_temp(loop2.windChill),
        _255_dash_16bit_temp(loop2.thswIndex),
        int(round(loop2.rainRate / rain_collector_size, 0)),
        uv,
        solar_radiation,
        int(round(loop2.stormRain / rain_collector_size, 0)),
        encode_current_storm_date(loop2.startDateOfCurrentStorm),
        int(round(loop2.dayRain / rain_collector_size, 0)),
        int(round(loop2.last15minRain / rain_collector_size, 0)),
        int(round(loop2.lastHourRain / rain_collector_size, 0)),
        int(round(mm_to_inch(loop2.dayET) / 1000.0, 0)),
        int(round(loop2.last24hourRain / rain_collector_size, 0)),
        loop2.barometricReductionMethod,
        int(round(mb_to_inhg(loop2.userBarometricOffset * 1000), 0)),
        int(round(mb_to_inhg(loop2.barometricCalibrationNumber * 1000), 0)),
        int(round(mb_to_inhg(loop2.barometricSensorRaw * 1000), 0)),
        int(round(mb_to_inhg(loop2.absoluteBarometricPressure * 1000), 0)),
        int(round(mb_to_inhg(loop2.altimeterSetting * 1000), 0)),
        0xFF,  # 1-byte reserved
        loop2.undefinedField,  # 1-byte "undefined" - assuming 0xFF

        # 10 graph pointers
        loop2.next10mWindSpeedGraphPointer,
        loop2.next15mWindSpeedGraphPointer,
        loop2.nextHourlyWindSpeedGraphPointer,
        loop2.nextDailyWindSpeedGraphPointer,
        loop2.nextMinuteRainGraphPointer,
        loop2.nextRainStormGraphPointer,
        loop2.indexToMinuteWithinHour,
        loop2.nextMonthlyRain,
        loop2.nextYearlyRain,
        loop2.nextSeasonalRain,

        # six 2-byte reserved
        0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,

        b'\n\r'
    ))

    if include_crc:
        crc = CRC.calculate_crc(result)
        packed_crc = struct.pack(CRC.FORMAT, crc)

        result.extend(packed_crc)

    return result


class LiveData(object):
    """
    A container for live data. Stores a combined view of the latest Loop and
    Loop2 data.

    the ready attribute is set to true when both a loop1 and a loop2 packet have
    been received. This indicates that all fields should now be populated.

    If loop1_only is set to True on construction then ready will be set to True
    when the first loop1 packet has been received.
    """
    def __init__(self, loop1_only):
        """
        :param loop1_only: If only loop1 packets are being received (False if
                           both loop1 and loop2 packets are being received)
        :type loop1_only: bool
        """
        self.lastUpdateType = None
        self.ready = False
        self._loop1_received = False
        self._loop2_received = loop1_only

        # Shared
        self.barTrend = None
        self.barometer = None
        self.insideTemperature = None
        self.insideHumidity = None
        self.outsideTemperature = None
        self.windSpeed = None
        self.averageWindSpeed10min = None
        self.windDirection = None
        self.outsideHumidity = None
        self.rainRate = None
        self.UV = None
        self.solarRadiation = None
        self.stormRain = None
        self.startDateOfCurrentStorm = None
        self.dayRain = None
        self.dayET = None

        # Loop1 Only
        self.nextRecord = None
        self.extraTemperatures = [None, None, None]
        self.soilTemperatures = [None, None, None, None]
        self.leafTemperatures = [None, None]
        self.extraHumidities = [None, None]
        self.monthRain = None
        self.yearRain = None
        self.monthET = None
        self.yearET = None
        self.soilMoistures = [None, None, None, None]
        self.leafWetness = [None, None]
        self.insideAlarms = None
        self.rainAlarms = None
        self.outsideAlarms = None
        self.extraTempHumAlarms = None
        self.soilAndLeafAlarms = None
        self.transmitterBatteryStatus = None
        self.consoleBatteryVoltage = None
        self.forecastIcons = None
        self.forecastRuleNumber = None
        self.timeOfSunrise = None
        self.timeOfSunset = None

        # Loop2 Only
        self.averageWindSpeed2min = None
        self.windGust10m = None
        self.windGust10mDirection = None
        self.dewPoint = None
        self.heatIndex = None
        self.windChill = None
        self.thswIndex = None
        self.last15minRain = None
        self.lastHourRain = None
        self.last24hourRain = None
        self.barometricReductionMethod = None
        self.userBarometricOffset = None
        self.barometricCalibrationNumber = None
        self.barometricSensorRaw = None
        self.absoluteBarometricPressure = None
        self.altimeterSetting = None

    def to_dict(self):
        """
        Creates a dict containing the latest data
        :return: A dict
        :rtype: dict
        """
        start_date_of_current_storm = None
        if self.startDateOfCurrentStorm is not None:
            start_date_of_current_storm = self.startDateOfCurrentStorm.isoformat()

        result = {
            "barTrend": self.barTrend,
            "barometer": self.barometer,
            "insideTemperature": self.insideTemperature,
            "insideHumidity": self.insideHumidity,
            "outsideTemperature": self.outsideTemperature,
            "windSpeed": self.windSpeed,
            "averageWindSpeed10min": self.averageWindSpeed10min,
            "windDirection": self.windDirection,
            "outsideHumidity": self.outsideHumidity,
            "rainRate": self.rainRate,
            "UV": self.UV,
            "solarRadiation": self.solarRadiation,
            "stormRain": self.stormRain,
            "startDateOfCurrentStorm": start_date_of_current_storm,
            "dayRain": self.dayRain,
            "dayET": self.dayET,

            "nextRecord": self.nextRecord,
            "extraTemperatures": self.extraTemperatures,
            "soilTemperatures": self.soilTemperatures,
            "leafTemperatures": self.leafTemperatures,
            "extraHumidities": self.extraHumidities,
            "monthRain": self.monthRain,
            "yearRain": self.yearRain,
            "monthET": self.monthET,
            "yearET": self.yearET,
            "soilMoistures": self.soilMoistures,
            "leafWetness": self.leafWetness,
            "insideAlarms": self.insideAlarms,
            "rainAlarms": self.rainAlarms,
            "outsideAlarms": self.outsideAlarms,
            "extraTempHumAlarms": self.extraTempHumAlarms,
            "soilAndLeafAlarms": self.soilAndLeafAlarms,
            "transmitterBatteryStatus": self.transmitterBatteryStatus,
            "consoleBatteryVoltage": self.consoleBatteryVoltage,
            "forecastIcons": self.forecastIcons,
            "forecastRuleNumber": self.forecastRuleNumber,
            "timeOfSunrise": self.timeOfSunrise.isoformat(),
            "timeOfSunset": self.timeOfSunset.isoformat(),

            "averageWindSpeed2min": self.averageWindSpeed2min,
            "windGust10m": self.windGust10m,
            "windGust10mDirection": self.windGust10mDirection,
            "dewPoint": self.dewPoint,
            "heatIndex": self.heatIndex,
            "windChill": self.windChill,
            "thswIndex": self.thswIndex,
            "last15minRain": self.last15minRain,
            "lastHourRain": self.lastHourRain,
            "last24hourRain": self.last24hourRain,
            "barometricReductionMethod": self.barometricReductionMethod,
            "userBarometricOffset": self.userBarometricOffset,
            "barometricCalibrationNumber": self.barometricCalibrationNumber,
            "barometricSensorRaw": self.barometricSensorRaw,
            "absoluteBarometricPressure": self.absoluteBarometricPressure,
            "altimeterSetting": self.altimeterSetting
        }
        return result

    def _update_shared(self, loop):
        """
        Updates parameters common to both Loop and Loop2 packets
        :param loop: A Loop or Loop2 packet
        :type loop: Union[Loop, Loop2]
        :return:
        """
        # Shared
        self.barTrend = loop.barTrend
        self.barometer = loop.barometer
        self.insideTemperature = loop.insideTemperature
        self.insideHumidity = loop.insideHumidity
        self.outsideTemperature = loop.outsideTemperature
        self.windSpeed = loop.windSpeed
        self.averageWindSpeed10min = loop.averageWindSpeed10min
        self.windDirection = loop.windDirection
        self.outsideHumidity = loop.outsideHumidity
        self.rainRate = loop.rainRate
        self.UV = loop.UV
        self.solarRadiation = loop.solarRadiation
        self.stormRain = loop.stormRain
        self.startDateOfCurrentStorm = loop.startDateOfCurrentStorm
        self.dayRain = loop.dayRain
        self.dayET = loop.dayET

        if self._loop1_received and self._loop2_received:
            self.ready = True

    def update_loop(self, loop):
        """
        Updates with data from a Loop packet
        :param loop: A loop packet
        :type loop: Loop
        """
        self._loop1_received = True
        self.lastUpdateType = 1
        self._update_shared(loop)

        # Loop1 Only
        self.nextRecord = loop.nextRecord
        self.extraTemperatures = loop.extraTemperatures
        self.soilTemperatures = loop.soilTemperatures
        self.leafTemperatures = loop.leafTemperatures
        self.extraHumidities = loop.extraHumidities
        self.monthRain = loop.monthRain
        self.yearRain = loop.yearRain
        self.monthET = loop.monthET
        self.yearET = loop.yearET
        self.soilMoistures = loop.soilMoistures
        self.leafWetness = loop.leafWetness
        self.insideAlarms = loop.insideAlarms
        self.rainAlarms = loop.rainAlarms
        self.outsideAlarms = loop.outsideAlarms
        self.extraTempHumAlarms = loop.extraTempHumAlarms
        self.soilAndLeafAlarms = loop.soilAndLeafAlarms
        self.transmitterBatteryStatus = loop.transmitterBatteryStatus
        self.consoleBatteryVoltage = loop.consoleBatteryVoltage
        self.forecastIcons = loop.forecastIcons
        self.forecastRuleNumber = loop.forecastRuleNumber
        self.timeOfSunrise = loop.timeOfSunrise
        self.timeOfSunset = loop.timeOfSunset

    def update_loop2(self, loop2):
        """
        Updates with data from a Loop packet
        :param loop2: A loop2 packet
        :type loop2: Loop2
        """
        self._loop2_received = True
        self.lastUpdateType = 2
        self._update_shared(loop2)

        # Loop2 Only
        self.averageWindSpeed2min = loop2.averageWindSpeed2min
        self.windGust10m = loop2.windGust10m
        self.windGust10mDirection = loop2.windGust10mDirection
        self.dewPoint = loop2.dewPoint
        self.heatIndex = loop2.heatIndex
        self.windChill = loop2.windChill
        self.thswIndex = loop2.thswIndex
        self.last15minRain = loop2.last15minRain
        self.lastHourRain = loop2.lastHourRain
        self.last24hourRain = loop2.last24hourRain
        self.barometricReductionMethod = loop2.barometricReductionMethod
        self.userBarometricOffset = loop2.userBarometricOffset
        self.barometricCalibrationNumber = loop2.barometricCalibrationNumber
        self.barometricSensorRaw = loop2.barometricSensorRaw
        self.absoluteBarometricPressure = loop2.absoluteBarometricPressure
        self.altimeterSetting = loop2.altimeterSetting
