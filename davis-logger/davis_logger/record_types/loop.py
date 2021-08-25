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
    serialise_16bit_temp

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
