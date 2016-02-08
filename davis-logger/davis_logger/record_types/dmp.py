# coding=utf-8
"""
The format used by the DMP and DMPAFT commands.
"""
from collections import namedtuple
import struct
import datetime
from twisted.python import log
from davis_logger.record_types.util import inhg_to_mb, mph_to_ms, \
    inch_to_mm, mb_to_inhg, ms_to_mph, mm_to_inch, CRC, deserialise_8bit_temp, \
    serialise_8bit_temp, undash_8bit, dash_8bit, deserialise_16bit_temp, \
    serialise_16bit_temp

__author__ = 'david'

# Dmp = namedtuple(
#     'Dmp',
#     (
#         'dateStamp',
#         'timeStamp',
#         'timeZone',
#         'dateInteger',
#         'timeInteger',
#         'outsideTemperature',
#         'highOutsideTemperature',
#         'lowOutsideTemperature',
#         'rainfall',
#         'highRainRate',
#         'barometer',
#         'solarRadiation',
#         'numberOfWindSamples',
#         'insideTemperature',
#         'insideHumidity',
#         'outsideHumidity',
#         'averageWindSpeed',
#         'highWindSpeed',
#         'highWindSpeedDirection',
#         'prevailingWindDirection',
#         'averageUVIndex',
#         'ET',
#         'highSolarRadiation',
#         'highUVIndex',
#         'forecastRule',
#         'leafTemperature',
#         'leafWetness',
#         'soilTemperatures',
#         # download record type
#         'extraHumidities',
#         'extraTemperatures',
#         'soilMoistures'
#     )
# )

class Dmp(object):
    def __init__(self, dateStamp, timeStamp, timeZone, dateInteger, timeInteger,
        outsideTemperature, highOutsideTemperature, lowOutsideTemperature,
        rainfall, highRainRate, barometer, solarRadiation, numberOfWindSamples,
        insideTemperature, insideHumidity, outsideHumidity, averageWindSpeed,
        highWindSpeed, highWindSpeedDirection, prevailingWindDirection,
        averageUVIndex, ET, highSolarRadiation, highUVIndex, forecastRule,
        leafTemperature, leafWetness, soilTemperatures, extraHumidities,
        extraTemperatures, soilMoistures):

        self.dateStamp = dateStamp
        self.timeStamp = timeStamp
        self.timeZone = timeZone
        self.dateInteger = dateInteger
        self.timeInteger = timeInteger
        self.outsideTemperature = outsideTemperature
        self.highOutsideTemperature = highOutsideTemperature
        self.lowOutsideTemperature = lowOutsideTemperature
        self.rainfall = rainfall
        self.highRainRate = highRainRate
        self.barometer = barometer
        self.solarRadiation = solarRadiation
        self.numberOfWindSamples = numberOfWindSamples
        self.insideTemperature = insideTemperature
        self.insideHumidity = insideHumidity
        self.outsideHumidity = outsideHumidity
        self.averageWindSpeed = averageWindSpeed
        self.highWindSpeed = highWindSpeed
        self.highWindSpeedDirection = highWindSpeedDirection
        self.prevailingWindDirection = prevailingWindDirection
        self.averageUVIndex = averageUVIndex
        self.ET = ET
        self.highSolarRadiation = highSolarRadiation
        self.highUVIndex = highUVIndex
        self.forecastRule = forecastRule
        self.leafTemperature = leafTemperature
        self.leafWetness = leafWetness
        self.soilTemperatures = soilTemperatures
        self.extraHumidities = extraHumidities
        self.extraTemperatures = extraTemperatures
        self.soilMoistures = soilMoistures



def split_page(page_string):
    """
    Splits a DMP page into its three components: the page sequence, record
    set (5 records) and its CRC.
    :param page_string:
    :return:
    """
    page_number = ord(page_string[0])

    recordA = page_string[1:53]
    recordB = page_string[53:105]
    recordC = page_string[105:157]
    recordD = page_string[157:209]
    recordE = page_string[209:261]
    #reserved = page_string[261:265]
    crc = struct.unpack(CRC.FORMAT,page_string[265:267])[0]

    return page_number, [recordA, recordB, recordC, recordD, recordE], crc


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

    page = struct.pack('B', page_number)
    for record in records:
        page += record
    page += '\xff\xff\xff\xff'

    crc = CRC.calculate_crc(page)

    page += struct.pack(CRC.FORMAT, crc)

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

    hour = binary_val / 100
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


def deserialise_dmp(dmp_string, rainCollectorSize=0.2):
    """
    Deserialised a dmp string into a Dmp tuple.
    :param dmp_string: DMP record in string format as sent by the console
    :type dmp_string: str
    :param rainCollectorSize: Size of the rain collector in millimeters
    :type rainCollectorSize: float
    :return: The DMP record as a named tuple.
    :rtype: Dmp
    """

    dmp_format = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B3B4B'

    dateStamp, timeStamp, outsideTemperature, highOutsideTemperature, \
        lowOutsideTemperature, rainfall, highRainRate, barometer, \
        solarRadiation, numberOfWindSamples, insideTemperature, \
        insideHumidity, outsideHumidity, averageWindSpeed, highWindSpeed, \
        highWindSpeedDirection, prevailingWindDirection, averageUVIndex, ET, \
        highSolarRadiation, highUVIndex, forecastRule, leafTemperature_1, \
        leafTemperature_2, leafWetness_1, leafWetness_2, soilTemperature_1, \
        soilTemperature_2, soilTemperature_3, soilTemperature_4, \
        downloadRecordType, extraHumidity_1, extraHumidity_2, \
        extraTemperature_1, extraTemperature_2, extraTemperature_3,\
        soilMoisture_1, soilMoisture_2, soilMoisture_3, soilMoisture_4 = \
        struct.unpack(dmp_format, dmp_string)

    if solarRadiation == 32767:
        solarRadiation = None

    if highSolarRadiation == 32767:
        highSolarRadiation = None

    if averageWindSpeed == 255:
        averageWindSpeed = None
    else:
        averageWindSpeed = mph_to_ms(averageWindSpeed)

    if averageUVIndex == 255:
        averageUVIndex = None
    else:
        averageUVIndex /= 10.0

    highUVIndex /= 10.0

    # TODO: bug: This should be ET / 1000.
    #  When this is fixed a data migration for Sandy Bay will be required.
    #  Also, zxw_web\database.py\get_day_evapotranspiration() will
    #  need adjusting.
    ET = inch_to_mm(ET * 1000)

    unpacked = Dmp(
        dateStamp=decode_date(dateStamp),
        timeStamp=decode_time(timeStamp),
        timeZone=None,
        dateInteger=dateStamp,
        timeInteger=timeStamp,
        outsideTemperature=deserialise_16bit_temp(outsideTemperature),
        highOutsideTemperature=deserialise_16bit_temp(
            highOutsideTemperature, True),
        lowOutsideTemperature=deserialise_16bit_temp(lowOutsideTemperature),
        rainfall=rainfall * rainCollectorSize,
        highRainRate=highRainRate * rainCollectorSize,
        barometer=inhg_to_mb(barometer / 1000.0),
        solarRadiation=solarRadiation,
        numberOfWindSamples=numberOfWindSamples,
        insideTemperature=deserialise_16bit_temp(insideTemperature),
        insideHumidity=undash_8bit(insideHumidity),
        outsideHumidity=undash_8bit(outsideHumidity),
        averageWindSpeed=averageWindSpeed,
        highWindSpeed=mph_to_ms(highWindSpeed),
        highWindSpeedDirection=_deserialise_wind_direction_code(
            highWindSpeedDirection),
        prevailingWindDirection=_deserialise_wind_direction_code(
            prevailingWindDirection),
        averageUVIndex=averageUVIndex,
        ET=ET,
        highSolarRadiation=highSolarRadiation,
        highUVIndex=undash_8bit(highUVIndex),
        forecastRule=forecastRule,
        leafTemperature=[
            deserialise_8bit_temp(leafTemperature_1),
            deserialise_8bit_temp(leafTemperature_2)
        ],
        leafWetness=[
            undash_8bit(leafWetness_1),
            undash_8bit(leafWetness_2)
        ],
        soilTemperatures=[
            deserialise_8bit_temp(soilTemperature_1),
            deserialise_8bit_temp(soilTemperature_2),
            deserialise_8bit_temp(soilTemperature_3),
            deserialise_8bit_temp(soilTemperature_4)
        ],
        extraHumidities=[
            undash_8bit(extraHumidity_1),
            undash_8bit(extraHumidity_2)
        ],
        extraTemperatures=[
            deserialise_8bit_temp(extraTemperature_1),
            deserialise_8bit_temp(extraTemperature_2),
            deserialise_8bit_temp(extraTemperature_3)
        ],
        soilMoistures=[
            undash_8bit(soilMoisture_1),
            undash_8bit(soilMoisture_2),
            undash_8bit(soilMoisture_3),
            undash_8bit(soilMoisture_4)
        ]
    )

    return unpacked


def serialise_dmp(dmp, rainCollectorSize=0.2):
    """
    Takes data from a DMP packet and converts it into the string format
    used by the console.
    :param dmp: DMP packet data
    :type dmp: Dmp
    :param rainCollectorSize: Size of the rain collector in millimeters
    :type rainCollectorSize: float
    :return: The Dmp tuple packed into a string.
    :rtype: str
    """

    dmp_format = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B3B4B'

    if dmp.solarRadiation is None:
        solarRadiation = 32767
    else:
        solarRadiation = dmp.solarRadiation

    if dmp.highSolarRadiation is None:
        highSolarRadiation = 32767
    else:
        highSolarRadiation = dmp.highSolarRadiation

    if dmp.averageWindSpeed is None:
        averageWindSpeed = 255
    else:
        averageWindSpeed = ms_to_mph(dmp.averageWindSpeed)

    if dmp.averageUVIndex is None:
        averageUVIndex = 255
    else:
        averageUVIndex = dmp.averageUVIndex / 10

    highUVIndex = dmp.highUVIndex * 10

    packed = struct.pack(
        dmp_format,
        encode_date(dmp.dateStamp),
        encode_time(dmp.timeStamp),
        serialise_16bit_temp(dmp.outsideTemperature),
        serialise_16bit_temp(dmp.highOutsideTemperature, True),
        serialise_16bit_temp(dmp.lowOutsideTemperature),
        int(dmp.rainfall / rainCollectorSize),
        int(dmp.highRainRate / rainCollectorSize),
        mb_to_inhg(dmp.barometer * 1000),
        solarRadiation,
        dmp.numberOfWindSamples,
        serialise_16bit_temp(dmp.insideTemperature),
        dash_8bit(dmp.insideHumidity),
        dash_8bit(dmp.outsideHumidity),
        averageWindSpeed,
        ms_to_mph(dmp.highWindSpeed),
        _serialise_wind_direction_code(dmp.highWindSpeedDirection),
        _serialise_wind_direction_code(dmp.prevailingWindDirection),
        averageUVIndex,
        mm_to_inch(dmp.ET),
        highSolarRadiation,
        highUVIndex,
        dmp.forecastRule,
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