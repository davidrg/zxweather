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
    if binary_val == -1:
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
        return -1

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
        raise Exception('invalid timestamp')

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


def deserialise_loop(loop_string, rainCollectorSize=0.2):
    """
    Takes a LOOP packet from the console and converts it into a namedtuple.
    :param loop_string: 97-character string from the console (packet minus CRC)
    :type loop_string: str
    :param rainCollectorSize: Size of the rain collector in millimeters
    :type rainCollectorSize: float
    :return: loop packet
    :rtype: Loop
    """
    loop_format = '<3sbBhhhBhBBh7B4B4BB7BhBhhhhhhhhh4B4BBB2B8B4BBhBBHH2s'

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
        day_ET, month_ET, year_ET, soil_moisture_1, soil_moisture_2, \
        soil_moisture_3, soil_moisture_4, leaf_wetness_1, leaf_wetness_2, \
        leaf_wetness_3, leaf_wetness_4, inside_alarms, rain_alarms, \
        outside_alarms_A, outside_alarms_B, ext_th_alarms_1, ext_th_alarms_2, \
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
        rainRate=rain_rate * rainCollectorSize,
        UV=decoded_uv,
        solarRadiation=solar_radiation,
        stormRain=inch_to_mm(storm_rain / 100.0),
        startDateOfCurrentStorm=decode_current_storm_date(
            current_storm_start_date),
        dayRain=day_rain * rainCollectorSize,
        monthRain=month_rain * rainCollectorSize,
        yearRain=year_rain * rainCollectorSize,
        dayET=inch_to_mm(day_ET * 1000),
        monthET=inch_to_mm(month_ET * 100),
        yearET=inch_to_mm(year_ET * 100),
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
            outside_alarms_A,
            outside_alarms_B],
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


def serialise_loop(loop, rainCollectorSize=0.2):
    """
    Converts LOOP data into the string representation used by the console
    :param loop: Loop data
    :type loop: Loop
    :param rainCollectorSize: Size of the rain collector in millimeters
    :type rainCollectorSize: float
    :returns: The loop thing as a string
    :rtype: str
    """

    loop_format = '<3sbBhhhBhBBh7B4B4BB7BhBhhhhhhhhh4B4BBB2B8B4BBhBBHH'

    if loop.solarRadiation is None:
        solarRadiation = 32767
    else:
        solarRadiation = loop.solarRadiation

    packed = struct.pack(
        loop_format,
        'LOO',  # Magic number
        loop.barTrend,
        0,  # Packet type. 0 = LOOP
        loop.nextRecord,
        mb_to_inhg(loop.barometer * 1000),
        serialise_16bit_temp(loop.insideTemperature),
        dash_8bit(loop.insideHumidity),
        serialise_16bit_temp(loop.outsideTemperature),
        ms_to_mph(loop.windSpeed),
        ms_to_mph(loop.averageWindSpeed10min),
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
        loop.rainRate / rainCollectorSize,
        dash_8bit(loop.UV),
        solarRadiation,
        mm_to_inch(loop.stormRain) * 100,
        encode_current_storm_date(loop.startDateOfCurrentStorm),
        int(loop.dayRain / rainCollectorSize),
        int(loop.monthRain / rainCollectorSize),
        int(loop.yearRain / rainCollectorSize),
        mm_to_inch(loop.dayET) * 1000,
        mm_to_inch(loop.monthET) * 100,
        mm_to_inch(loop.yearET) * 100,
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
        int(((loop.consoleBatteryVoltage / 300.0) * 512) * 100),
        loop.forecastIcons,
        loop.forecastRuleNumber,
        encode_time(loop.timeOfSunrise),
        encode_time(loop.timeOfSunset)
    )

    packed += '\n\r'

    crc = CRC.calculate_crc(packed)
    packed_crc = struct.pack(CRC.FORMAT, crc)

    return packed + packed_crc
