# coding=utf-8
"""
The format used by the DMP and DMPAFT commands.
"""
import struct
import datetime
from davis_logger.record_types.util import inhg_to_mb, mph_to_ms, \
    inch_to_mm, mb_to_inhg, ms_to_mph, mm_to_inch, CRC, deserialise_8bit_temp, \
    serialise_8bit_temp, undash_8bit, dash_8bit, deserialise_16bit_temp, \
    serialise_16bit_temp

__author__ = 'david'


class Dmp(object):
    """
    An archive record from a Vantage Pro2 or Vue weather station
    """
    def __init__(self, date_stamp, time_stamp, time_zone, date_integer, time_integer,
                 outside_temperature, high_outside_temperature, low_outside_temperature,
                 rainfall, high_rain_rate, barometer, solar_radiation, number_of_wind_samples,
                 inside_temperature, inside_humidity, outside_humidity, average_wind_speed,
                 high_wind_speed, high_wind_speed_direction, prevailing_wind_direction,
                 average_uv_index, evapotranspiration, high_solar_radiation, high_uv_index,
                 forecast_rule, leaf_temperatures, leaf_wetness, soil_temperatures,
                 extra_humidities, extra_temperatures, soil_moistures):

        self.dateStamp = date_stamp
        self.timeStamp = time_stamp
        self.timeZone = time_zone
        self.dateInteger = date_integer
        self.timeInteger = time_integer
        self.outsideTemperature = outside_temperature
        self.highOutsideTemperature = high_outside_temperature
        self.lowOutsideTemperature = low_outside_temperature
        self.rainfall = rainfall
        self.highRainRate = high_rain_rate
        self.barometer = barometer
        self.solarRadiation = solar_radiation
        self.numberOfWindSamples = number_of_wind_samples
        self.insideTemperature = inside_temperature
        self.insideHumidity = inside_humidity
        self.outsideHumidity = outside_humidity
        self.averageWindSpeed = average_wind_speed
        self.highWindSpeed = high_wind_speed
        self.highWindSpeedDirection = high_wind_speed_direction
        self.prevailingWindDirection = prevailing_wind_direction
        self.averageUVIndex = average_uv_index
        self.ET = evapotranspiration
        self.highSolarRadiation = high_solar_radiation
        self.highUVIndex = high_uv_index
        self.forecastRule = forecast_rule
        self.leafTemperature = leaf_temperatures
        self.leafWetness = leaf_wetness
        self.soilTemperatures = soil_temperatures
        self.extraHumidities = extra_humidities
        self.extraTemperatures = extra_temperatures
        self.soilMoistures = soil_moistures

    # This doesn't really work due to difficulty comparing floating point
    # numbers. Python 3.5 adds math.isclose which should solve the problem once
    # we're able to set Python 3.5 as the minimum supported version.
    # def __eq__(self, other):
    #     """
    #     Compares this Dmp to the other Dmp
    #     :param other:
    #     :type other: Dmp
    #     :return: If other is equal to self
    #     """
    #     result = self.dateStamp == other.dateStamp \
    #         and self.timeStamp == other.timeStamp \
    #         and self.timeZone == other.timeZone \
    #         and self.dateInteger == other.dateInteger \
    #         and self.timeInteger == other.timeInteger \
    #         and self.outsideTemperature == other.outsideTemperature \
    #         and self.highOutsideTemperature == other.highOutsideTemperature \
    #         and self.lowOutsideTemperature == other.lowOutsideTemperature \
    #         and self.rainfall == other.rainfall \
    #         and self.highRainRate == other.highRainRate \
    #         and self.barometer == other.barometer \
    #         and self.solarRadiation == other.solarRadiation \
    #         and self.numberOfWindSamples == other.numberOfWindSamples \
    #         and self.insideTemperature == other.insideTemperature \
    #         and self.insideHumidity == other.insideHumidity \
    #         and self.outsideHumidity == other.outsideHumidity \
    #         and self.averageWindSpeed == other.averageWindSpeed \
    #         and self.highWindSpeed == other.highWindSpeed \
    #         and self.highWindSpeedDirection == other.highWindSpeedDirection \
    #         and self.prevailingWindDirection == other.prevailingWindDirection \
    #         and self.averageUVIndex == other.averageUVIndex \
    #         and self.ET == other.ET \
    #         and self.highSolarRadiation == other.highSolarRadiation \
    #         and self.highUVIndex == other.highUVIndex \
    #         and self.forecastRule == other.forecastRule \
    #         and self.leafTemperature == other.leafTemperature \
    #         and self.leafWetness == other.leafWetness \
    #         and self.soilTemperatures == other.soilTemperatures \
    #         and self.extraHumidities == other.extraHumidities \
    #         and self.extraTemperatures == other.extraTemperatures \
    #         and self.soilMoistures == other.soilMoistures
    #
    #     return result

    def __repr__(self):
        """
        Converts a Dmp record to a string for easier diffing
        """
        result = "Dmp(dateStamp={dateStamp}, timeStamp={timeStamp}, " \
                 "timeZone={timeZone}, dateInteger={dateInteger}, " \
                 "timeInteger={timeInteger}, " \
                 "outsideTemperature={outsideTemperature}, " \
                 "highOutsideTemperature={highOutsideTemperature}, " \
                 "lowOutsideTemperature={lowOutsideTemperature}, " \
                 "rainfall={rainfall}, highRainRate={highRainRate}, " \
                 "barometer={barometer}, solarRadiation={solarRadiation}, " \
                 "numberOfWindSamples={numberOfWindSamples}, " \
                 "insideTemperature={insideTemperature}, " \
                 "insideHumidity={insideHumidity}, " \
                 "outsideHumidity={outsideHumidity}, " \
                 "averageWindSpeed={averageWindSpeed}, " \
                 "highWindSpeed={highWindSpeed}, " \
                 "highWindSpeedDirection={highWindSpeedDirection}, " \
                 "prevailingWindDirection={prevailingWindDirection}, " \
                 "averageUVIndex={averageUVIndex}, ET={ET}, " \
                 "highSolarRadiation={highSolarRadiation}, " \
                 "highUVIndex={highUVIndex}, forecastRule={forecastRule}, " \
                 "leafTemperature={leafTemperature}, leafWetness={leafWetness}, " \
                 "soilTemperatures={soilTemperatures}, " \
                 "extraHumidities={extraHumidities}, " \
                 "extraTemperatures={extraTemperatures}, " \
                 "soilMoistures={soilMoistures}".format(
                    dateStamp=self.dateStamp,
                    timeStamp=self.timeStamp,
                    timeZone=self.timeZone,
                    dateInteger=self.dateInteger,
                    timeInteger=self.timeInteger,
                    outsideTemperature=self.outsideTemperature,
                    highOutsideTemperature=self.highOutsideTemperature,
                    lowOutsideTemperature=self.lowOutsideTemperature,
                    rainfall=self.rainfall,
                    highRainRate=self.highRainRate,
                    barometer=self.barometer,
                    solarRadiation=self.solarRadiation,
                    numberOfWindSamples=self.numberOfWindSamples,
                    insideTemperature=self.insideTemperature,
                    insideHumidity=self.insideHumidity,
                    outsideHumidity=self.outsideHumidity,
                    averageWindSpeed=self.averageWindSpeed,
                    highWindSpeed=self.highWindSpeed,
                    highWindSpeedDirection=self.highWindSpeedDirection,
                    prevailingWindDirection=self.prevailingWindDirection,
                    averageUVIndex=self.averageUVIndex,
                    ET=self.ET,
                    highSolarRadiation=self.highSolarRadiation,
                    highUVIndex=self.highUVIndex,
                    forecastRule=self.forecastRule,
                    leafTemperature=self.leafTemperature,
                    leafWetness=self.leafWetness,
                    soilTemperatures=self.soilTemperatures,
                    extraHumidities=self.extraHumidities,
                    extraTemperatures=self.extraTemperatures,
                    soilMoistures=self.soilMoistures)

        return result


def split_page(page_string):
    """
    Splits a DMP page into its three components: the page sequence, record
    set (5 records) and its CRC.
    :param page_string:
    :type page_string: bytes
    :return:
    """
    page_number = ord(page_string[0:1])

    record_a = page_string[1:53]
    record_b = page_string[53:105]
    record_c = page_string[105:157]
    record_d = page_string[157:209]
    record_e = page_string[209:261]
    # reserved = page_string[261:265]
    crc = struct.unpack(CRC.FORMAT, page_string[265:267])[0]

    return page_number, [record_a, record_b, record_c, record_d, record_e], crc


def build_page(page_number, records):
    """
    Builds a DMP page with the specified page number and set of five record.
    :param page_number: The page number to use
    :type page_number: int
    :param records: Five serialised DMP records
    :type records: list
    :return: A DMP page with CRC, etc.
    :rtype: str
    """

    assert(len(records) == 5)
    assert(0 <= page_number <= 255)

    page = bytearray()

    page.extend(struct.pack('B', page_number))
    for record in records:
        page.extend(record)
    page.extend(b'\xff\xff\xff\xff')

    crc = CRC.calculate_crc(page)

    page.extend(struct.pack(CRC.FORMAT, crc))

    return page


def decode_date(binary_val):
    """
    Decodes the supplied date in the format used by DMP records.
    :param binary_val: DMP Record date.
    :type binary_val: int
    :return:
    """

    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    # | YEAR                      | MONTH         | DAY               |
    # +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    #    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0

    if binary_val == 0xFFFF:
        return None

    year_mask = 0xFE00
    month_mask = 0x01E0
    day_mask = 0x001F

    year = 2000 + ((binary_val & year_mask) >> 9)
    month = (binary_val & month_mask) >> 5
    day = (binary_val & day_mask)

    return datetime.date(year=year, month=month, day=day)


def encode_date(record_date):
    """
    Encodes the supplied date in the format used by DMP records
    :param record_date: Date to encode
    :type record_date: datetime.date
    :return: Encodes the supplied date in the format used by DMP records
    :rtype: int
    """

    if record_date is None:
        return 0xFFFF

    year = record_date.year
    year -= 2000
    month = record_date.month
    day = record_date.day

    value = (year << 9) + (month << 5) + day

    return value


def decode_time(binary_val):
    """
    Decodes the time format used in DMP records
    :param binary_val: Time value (short integer)
    :type binary_val: int
    :return: Decoded time
    :rtype: datetime.time
    """

    if binary_val == 0xFFFF:
        return None

    hour = binary_val // 100
    minute = binary_val - (hour * 100)

    return datetime.time(hour=hour, minute=minute)


def encode_time(record_time):
    """
    Encodes the supplied time in the format used by DMP records
    :param record_time: Time to encode
    :type record_time: datetime.time
    :return: Encoded time
    :rtype: int
    """

    if record_time is None:
        return 0xFFFF

    return record_time.hour * 100 + record_time.minute


_compass_points = [
    0,  # N
    22.5,  # NNE
    45,  # NE
    67.5,  # ENE
    90,  # E
    112.5,  # ESE
    135,  # SE
    157.5,  # SSE
    180,  # S
    202.5,  # SSW
    225,  # SW
    247.5,  # WSW
    270,  # W
    292.5,  # WNW
    315,  # NW
    337.5  # NNW
]


def _deserialise_wind_direction_code(code):
    """
    Converts the supplied wind direction code into degrees.
    :param code: wind direction code
    :type code: int
    :return: Degrees
    :rtype: int
    """

    if code > 15:
        return None
    else:
        return _compass_points[code]


def _serialise_wind_direction_code(value):
    """
    Converts the supplied direction in degrees for one of the compass points
    to an integer between 0 and 15.
    :param value: The value to serialise
    :return:
    """
    if value is None:
        return 255

    return _compass_points.index(value)


def deserialise_dmp(dmp_record, rain_collector_size=0.2, rev_b_firmware=True):
    """
    Deserialised a dmp string into a Dmp tuple.
    :param dmp_record: DMP record in string format as sent by the console
    :type dmp_record: bytes
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :param rev_b_firmware: If this station is using Rev. B firmware.
    :type rev_b_firmware: bool
    :return: The DMP record as a named tuple.
    :rtype: Dmp
    """

    dmp_format_a = '<HHhhhHHHHHhBBBBBBBBB4B4B4B2B2BHHB'
    dmp_format_b = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B3B4B'

    # byte 42 is guaranteed to be 0 for revision B and 0xFF for revision A.
    is_rev_a = dmp_record[42] == b'\xFF'

    if not is_rev_a:
        date_stamp, time_stamp, outside_temperature, high_outside_temperature, \
            low_outside_temperature, rainfall, high_rain_rate, barometer, \
            solar_radiation, number_of_wind_samples, inside_temperature, \
            inside_humidity, outside_humidity, average_wind_speed, high_wind_speed, \
            high_wind_speed_direction, prevailing_wind_direction, average_uv_index, et, \
            high_solar_radiation, high_uv_index, forecast_rule, leaf_temperature_1, \
            leaf_temperature_2, leaf_wetness_1, leaf_wetness_2, soil_temperature_1, \
            soil_temperature_2, soil_temperature_3, soil_temperature_4, \
            download_record_type, extra_humidity_1, extra_humidity_2, \
            extra_temperature_1, extra_temperature_2, extra_temperature_3,\
            soil_moisture_1, soil_moisture_2, soil_moisture_3, soil_moisture_4 = \
            struct.unpack(dmp_format_b, dmp_record)
    else:
        # This has never been tested. It may not work.
        assert not rev_b_firmware
        date_stamp, time_stamp, outside_temperature, high_outside_temperature, \
            low_outside_temperature, rainfall, high_rain_rate, barometer, \
            solar_radiation, number_of_wind_samples, inside_temperature, \
            inside_humidity, outside_humidity, average_wind_speed, high_wind_speed, \
            high_wind_speed_direction, prevailing_wind_direction, average_uv_index, et, \
            reserved_a, soil_moisture_1, soil_moisture_2, soil_moisture_3, soil_moisture_4, \
            soil_temperature_1, soil_temperature_2, soil_temperature_3, soil_temperature_4, \
            leaf_wetness_1, leaf_wetness_2, leaf_wetness_3, leaf_wetness_4, \
            extra_temperature_1, extra_temperature_2, extra_humidity_1, extra_humidity_2, \
            reed_closed_count, reed_opened_count, reserved_b \
            = struct.unpack(dmp_format_a, dmp_record)

        # These fields are unsupported in Rev. A dump records so we'll just
        # pretend they're null.
        high_solar_radiation = 32767
        high_uv_index = 255
        forecast_rule = None
        leaf_temperature_1 = 255
        leaf_temperature_2 = 255
        extra_temperature_3 = 255

    if solar_radiation == 32767:
        solar_radiation = None

    if high_solar_radiation == 32767:
        high_solar_radiation = None

    if average_wind_speed == 255:
        average_wind_speed = None
    else:
        average_wind_speed = mph_to_ms(average_wind_speed)

    if average_uv_index == 255:
        average_uv_index = None
    else:
        average_uv_index /= 10.0

    high_uv_index /= 10.0

    et = inch_to_mm(et / 1000.0)

    unpacked = Dmp(
        date_stamp=decode_date(date_stamp),
        time_stamp=decode_time(time_stamp),
        time_zone=None,
        date_integer=date_stamp,
        time_integer=time_stamp,
        outside_temperature=deserialise_16bit_temp(outside_temperature),
        high_outside_temperature=deserialise_16bit_temp(
            high_outside_temperature, True),
        low_outside_temperature=deserialise_16bit_temp(low_outside_temperature),
        rainfall=rainfall * rain_collector_size,
        high_rain_rate=high_rain_rate * rain_collector_size,
        barometer=inhg_to_mb(barometer / 1000.0),
        solar_radiation=solar_radiation,
        number_of_wind_samples=number_of_wind_samples,
        inside_temperature=deserialise_16bit_temp(inside_temperature),
        inside_humidity=undash_8bit(inside_humidity),
        outside_humidity=undash_8bit(outside_humidity),
        average_wind_speed=average_wind_speed,
        high_wind_speed=mph_to_ms(high_wind_speed),
        high_wind_speed_direction=_deserialise_wind_direction_code(
            high_wind_speed_direction),
        prevailing_wind_direction=_deserialise_wind_direction_code(
            prevailing_wind_direction),
        average_uv_index=average_uv_index,
        evapotranspiration=et,
        high_solar_radiation=high_solar_radiation,
        high_uv_index=undash_8bit(high_uv_index),
        forecast_rule=forecast_rule,
        leaf_temperatures=[
            deserialise_8bit_temp(leaf_temperature_1),
            deserialise_8bit_temp(leaf_temperature_2)
        ],
        leaf_wetness=[
            undash_8bit(leaf_wetness_1),
            undash_8bit(leaf_wetness_2)
        ],
        soil_temperatures=[
            deserialise_8bit_temp(soil_temperature_1),
            deserialise_8bit_temp(soil_temperature_2),
            deserialise_8bit_temp(soil_temperature_3),
            deserialise_8bit_temp(soil_temperature_4)
        ],
        extra_humidities=[
            undash_8bit(extra_humidity_1),
            undash_8bit(extra_humidity_2)
        ],
        extra_temperatures=[
            deserialise_8bit_temp(extra_temperature_1),
            deserialise_8bit_temp(extra_temperature_2),
            deserialise_8bit_temp(extra_temperature_3)
        ],
        soil_moistures=[
            undash_8bit(soil_moisture_1),
            undash_8bit(soil_moisture_2),
            undash_8bit(soil_moisture_3),
            undash_8bit(soil_moisture_4)
        ]
    )

    return unpacked


def serialise_dmp(dmp, rain_collector_size=0.2):
    """
    Takes data from a DMP packet and converts it into the string format
    used by the console.
    :param dmp: DMP packet data
    :type dmp: Dmp
    :param rain_collector_size: Size of the rain collector in millimeters
    :type rain_collector_size: float
    :return: The Dmp tuple packed into a string.
    :rtype: bytes
    """

    dmp_format = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B3B4B'

    if dmp.solarRadiation is None:
        solar_radiation = 32767
    else:
        solar_radiation = dmp.solarRadiation

    if dmp.highSolarRadiation is None:
        high_solar_radiation = 32767
    else:
        high_solar_radiation = dmp.highSolarRadiation

    if dmp.averageWindSpeed is None:
        average_wind_speed = 255
    else:
        average_wind_speed = int(round(ms_to_mph(dmp.averageWindSpeed), 0))

    if dmp.averageUVIndex is None:
        average_uv_index = 255
    else:
        average_uv_index = int(round(dmp.averageUVIndex / 10, 0))

    if dmp.highUVIndex is None:
        high_uv_index = 0
    else:
        high_uv_index = int(round(dmp.highUVIndex * 10, 0))

    if dmp.rainfall is None:
        rainfall = 0
    else:
        rainfall = int(round(dmp.rainfall / rain_collector_size, 0))

    if dmp.highRainRate is None:
        high_rain_rate = 0
    else:
        high_rain_rate = int(round(dmp.highRainRate / rain_collector_size, 0))

    if dmp.barometer is None:
        barometer = 0
    else:
        barometer = int(round(mb_to_inhg(dmp.barometer * 1000), 0))

    if dmp.highWindSpeed is None:
        high_wind_speed = 0
    else:
        high_wind_speed = int(round(ms_to_mph(dmp.highWindSpeed), 0))

    if dmp.ET is None:
        evapotranspiration = 0
    else:
        evapotranspiration = int(round(mm_to_inch(dmp.ET), 0))

    if dmp.numberOfWindSamples is None:
        number_of_wind_samples = 0
    else:
        number_of_wind_samples = dmp.numberOfWindSamples

    if dmp.forecastRule is None:
        forecast_rule = 193
    else:
        forecast_rule = dmp.forecastRule

    packed = struct.pack(
        dmp_format,
        encode_date(dmp.dateStamp),
        encode_time(dmp.timeStamp),
        serialise_16bit_temp(dmp.outsideTemperature),
        serialise_16bit_temp(dmp.highOutsideTemperature, True),
        serialise_16bit_temp(dmp.lowOutsideTemperature),
        rainfall,
        high_rain_rate,
        barometer,
        solar_radiation,
        number_of_wind_samples,
        serialise_16bit_temp(dmp.insideTemperature),
        dash_8bit(dmp.insideHumidity),
        dash_8bit(dmp.outsideHumidity),
        average_wind_speed,
        high_wind_speed,
        _serialise_wind_direction_code(dmp.highWindSpeedDirection),
        _serialise_wind_direction_code(dmp.prevailingWindDirection),
        average_uv_index,
        evapotranspiration,
        high_solar_radiation,
        high_uv_index,
        forecast_rule,
        serialise_8bit_temp(dmp.leafTemperature[0]),
        serialise_8bit_temp(dmp.leafTemperature[1]),
        dash_8bit(dmp.leafWetness[0]),
        dash_8bit(dmp.leafWetness[1]),
        serialise_8bit_temp(dmp.soilTemperatures[0]),
        serialise_8bit_temp(dmp.soilTemperatures[1]),
        serialise_8bit_temp(dmp.soilTemperatures[2]),
        serialise_8bit_temp(dmp.soilTemperatures[3]),
        0,  # Record type. 0x00 == B, 0xFF == A
        dash_8bit(dmp.extraHumidities[0]),
        dash_8bit(dmp.extraHumidities[1]),
        serialise_8bit_temp(dmp.extraTemperatures[0]),
        serialise_8bit_temp(dmp.extraTemperatures[1]),
        serialise_8bit_temp(dmp.extraTemperatures[2]),
        dash_8bit(dmp.soilMoistures[0]),
        dash_8bit(dmp.soilMoistures[1]),
        dash_8bit(dmp.soilMoistures[2]),
        dash_8bit(dmp.soilMoistures[3])
    )

    return packed
