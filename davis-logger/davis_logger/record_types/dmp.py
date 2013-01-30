# coding=utf-8
"""
The format used by the DMP and DMPAFT commands.
"""
from collections import namedtuple
import struct

__author__ = 'david'

Dmp = namedtuple(
    'Dmp',
    (
        'dateStamp',
        'timeStamp',
        'outsideTemperature',
        'highOutsideTemperature',
        'lowOutsideTemperature',
        'rainfall',
        'highRainRate',
        'barometer',
        'solarRadiation',
        'numberOfWindSamples',
        'insideTemperature',
        'insideHumidity',
        'outsideHumidity',
        'averageWindSpeed',
        'highWindSpeed',
        'highWindSpeedDirection',
        'prevailingWindDirection',
        'averageUVIndex',
        'ET',
        'highSolarRadiation',
        'highUVIndex',
        'forecastRule',
        'leafTemperature',
        'leafWetness',
        'soilTemperatures',
        # download record type
        'extraHumidities',
        'extraTemperatures',
        'soilMoistures'
    )
)


def deserialise_dmp(dmp_string):
    """
    Deserialised a dmp string into a Dmp tuple.
    :param dmp_string: DMP record in string format as sent by the console
    :type dmp_string: str
    :return: The DMP record as a named tuple.
    :rtype: Dmp
    """

    format = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B2B4B'

    dateStamp, timeStamp, outsideTemperature, highOutsideTemperature, \
    lowOutsideTemperature, rainfall, highRainRate, barometer, solarRadiation, \
    numberOfWindSamples, insideTemperature, insideHumidity, outsideHumidity, \
    averageWindSpeed, highWindSpeed, highWindSpeedDirection, \
    prevailingWindDirection, averageUVIndex, ET, highSolarRadiation, \
    highUVIndex, forecastRule, leafTemperature_1, leafTemperature_2, \
    leafWetness_1, leafWetness_2, soilTemperature_1, soilTemperature_2, soilTemperature_3, soilTemperature_4,\
    downloadRecordType, extraHumidity_1, extraHumidity_2, extraTemperature_1, extraTemperature_2, \
    soilMoisture_1, soilMoisture_2, soilMoisture_3, soilMoisture_4 = struct.unpack(format, dmp_string)

    unpacked = Dmp(
        dateStamp=dateStamp,
        timeStamp=timeStamp,
        outsideTemperature=outsideTemperature,
        highOutsideTemperature=highOutsideTemperature,
        lowOutsideTemperature=lowOutsideTemperature,
        rainfall=rainfall,
        highRainRate=highRainRate,
        barometer=barometer,
        solarRadiation=solarRadiation,
        numberOfWindSamples=numberOfWindSamples,
        insideTemperature=insideTemperature,
        insideHumidity=insideHumidity,
        outsideHumidity=outsideHumidity,
        averageWindSpeed=averageWindSpeed,
        highWindSpeed=highWindSpeed,
        highWindSpeedDirection=highWindSpeedDirection,
        prevailingWindDirection=prevailingWindDirection,
        averageUVIndex=averageUVIndex,
        ET=ET,
        highSolarRadiation=highSolarRadiation,
        highUVIndex=highUVIndex,
        forecastRule=forecastRule,
        leafTemperature = [
            leafTemperature_1,
            leafTemperature_2
        ],
        leafWetness = [
            leafWetness_1,
            leafWetness_2
        ],
        soilTemperatures = [
            soilTemperature_1,
            soilTemperature_2,
            soilTemperature_3,
            soilTemperature_4
        ],
        extraHumidities = [
            extraHumidity_1,
            extraHumidity_2
        ],
        extraTemperatures = [
            extraTemperature_1,
            extraTemperature_2
        ],
        soilMoistures = [
            soilMoisture_1,
            soilMoisture_2,
            soilMoisture_3,
            soilMoisture_4
        ]
    )

    return unpacked


def serialise_dmp(dmp):
    """
    Takes data from a DMP packet and converts it into the string format
    used by the console.
    :param dmp: DMP packet data
    :type dmp: Dmp
    :return: The Dmp tuple packed into a string.
    :rtype: str
    """

    format = '<HHhhhHHHHHhBBBBBBBBHBB2B2B4BB2B2B4B'

    # TODO: unit conversions, etc.
    packed = struct.pack(
        format,
        dmp.dateStamp,
        dmp.timeStamp,
        dmp.outsideTemperature,
        dmp.highOutsideTemperature,
        dmp.lowOutsideTemperature,
        dmp.rainfall,
        dmp.highRainRate,
        dmp.barometer,
        dmp.solarRadiation,
        dmp.numberOfWindSamples,
        dmp.insideTemperature,
        dmp.insideHumidity,
        dmp.outsideHumidity,
        dmp.averageWindSpeed,
        dmp.highWindSpeed,
        dmp.highWindSpeedDirection,
        dmp.prevailingWindDirection,
        dmp.averageUVIndex,
        dmp.ET,
        dmp.highSolarRadiation,
        dmp.highUVIndex,
        dmp.forecastRule,
        dmp.leafTemperature[0],
        dmp.leafTemperature[1],
        dmp.leafWetness[0],
        dmp.leafWetness[1],
        dmp.soilTemperatures[0],
        dmp.soilTemperatures[1],
        dmp.soilTemperatures[2],
        dmp.soilTemperatures[3],
        0, # Record type. 0x00 == B, 0xFF == A
        dmp.extraHumidities[0],
        dmp.extraHumidities[1],
        dmp.extraTemperatures[0],
        dmp.extraTemperatures[1],
        dmp.soilMoistures[0],
        dmp.soilMoistures[1],
        dmp.soilMoistures[2],
        dmp.soilMoistures[4]
    )

    return packed