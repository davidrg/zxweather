# coding=utf-8
"""
Tests for the DMP encoding/decoding code.
"""
import struct
import unittest
from davis_logger.record_types.dmp import deserialise_dmp, serialise_dmp, split_page, build_page
from davis_logger.record_types.util import CRC

__author__ = 'david'

class dmp_tests(unittest.TestCase):
    dmp_page = '\x00\x49\x1a\xeb\x05\x0a\x03\x0a\x03\x04\x03\x00\x00\x00\x00' \
               '\xa3\x75\xff\x7f\x5f\x00\x3a\x03\x2c\x2c\x01\x07\x0e\x0b\xff' \
               '\x00\xff\x7f\xff\xc1\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff' \
               '\xff\xff\xff\xff\x00\x00\x01\x20\x49\x1a\xf0\x05\x0d\x03\x0d' \
               '\x03\x0a\x03\x00\x00\x00\x00\xa4\x75\xff\x7f\x75\x00\x29\x03' \
               '\x2e\x2c\x01\x06\x0e\x0c\xff\x00\xff\x7f\xff\xc1\x00\x08\x00' \
               '\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01\x20' \
               '\x49\x1a\xf5\x05\x09\x03\x0e\x03\x09\x03\x00\x00\x00\x00\xa2' \
               '\x75\xff\x7f\x75\x00\x20\x03\x2f\x29\x01\x04\x0a\x0b\xff\x00' \
               '\xff\x7f\xff\xc1\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff' \
               '\xff\xff\xff\x00\x00\x01\x20\x49\x1a\xfa\x05\x08\x03\x09\x03' \
               '\x06\x03\x00\x00\x00\x00\x9f\x75\xff\x7f\x75\x00\x1b\x03\x30' \
               '\x2b\x01\x05\x0c\x0c\xff\x00\xff\x7f\xff\xc1\x00\x08\x00\x08' \
               '\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01\x20\x49' \
               '\x1a\xff\x05\x05\x03\x0a\x03\x05\x03\x00\x00\x00\x00\x9f\x75' \
               '\xff\x7f\x73\x00\x19\x03\x31\x29\x02\x07\x0d\x0a\xff\x00\xff' \
               '\x7f\xff\xc1\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff' \
               '\xff\xff\x00\x00\x01\x20\xff\xff\xff\xff\x2c\x13'

    dmp_records = [
        '\x49\x1a\xeb\x05\x0a\x03\x0a\x03\x04\x03\x00\x00\x00\x00\xa3\x75\xff'
        '\x7f\x5f\x00\x3a\x03\x2c\x2c\x01\x07\x0e\x0b\xff\x00\xff\x7f\xff\xc1'
        '\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01'
        '\x20',
        '\x49\x1a\xf0\x05\x0d\x03\x0d\x03\x0a\x03\x00\x00\x00\x00\xa4\x75\xff'
        '\x7f\x75\x00\x29\x03\x2e\x2c\x01\x06\x0e\x0c\xff\x00\xff\x7f\xff\xc1'
        '\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01'
        '\x20',
        '\x49\x1a\xf5\x05\x09\x03\x0e\x03\x09\x03\x00\x00\x00\x00\xa2\x75\xff'
        '\x7f\x75\x00\x20\x03\x2f\x29\x01\x04\x0a\x0b\xff\x00\xff\x7f\xff\xc1'
        '\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01'
        '\x20',
        '\x49\x1a\xfa\x05\x08\x03\x09\x03\x06\x03\x00\x00\x00\x00\x9f\x75\xff'
        '\x7f\x75\x00\x1b\x03\x30\x2b\x01\x05\x0c\x0c\xff\x00\xff\x7f\xff\xc1'
        '\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01'
        '\x20',
        '\x49\x1a\xff\x05\x05\x03\x0a\x03\x05\x03\x00\x00\x00\x00\x9f\x75\xff'
        '\x7f\x73\x00\x19\x03\x31\x29\x02\x07\x0d\x0a\xff\x00\xff\x7f\xff\xc1'
        '\x00\x08\x00\x08\x00\x08\x00\x08\x00\xff\xff\xff\xff\xff\x00\x00\x01'
        '\x20',
    ]

    sequence = 0

    crc = struct.unpack(CRC.FORMAT, '\x2c\x13')[0]

    def test_split_page(self):
        sequence, records, crc = split_page(self.dmp_page)
        self.assertEqual(sequence, self.sequence)
        self.assertEqual(crc, self.crc)
        self.assertListEqual(records, self.dmp_records)

    def test_build_page(self):
        sequence, records, crc = split_page(self.dmp_page)
        page = build_page(sequence, records)

        self.assertEqual(self.dmp_page, page)

    def test_round_trip(self):
        for record in self.dmp_records:
            decoded = deserialise_dmp(record)

            encoded = serialise_dmp(decoded)
            decoded2 = deserialise_dmp(encoded)

            # Check each field matches
            self.assertEqual(decoded.dateStamp, decoded2.dateStamp)
            self.assertEqual(decoded.timeStamp, decoded2.timeStamp)
            self.assertEqual(decoded.timeZone, decoded2.timeZone)
            self.assertEqual(decoded.dateInteger, decoded2.dateInteger)
            self.assertEqual(decoded.timeInteger, decoded2.timeInteger)
            self.assertEqual(decoded.outsideTemperature,
                             decoded2.outsideTemperature)
            self.assertEqual(decoded.highOutsideTemperature,
                             decoded2.highOutsideTemperature)
            self.assertEqual(decoded.lowOutsideTemperature,
                             decoded2.lowOutsideTemperature)
            self.assertEqual(decoded.rainfall, decoded2.rainfall)
            self.assertEqual(decoded.highRainRate, decoded2.highRainRate)
            self.assertEqual(decoded.barometer, decoded2.barometer)
            self.assertEqual(decoded.solarRadiation, decoded2.solarRadiation)
            self.assertEqual(decoded.numberOfWindSamples,
                             decoded2.numberOfWindSamples)
            self.assertEqual(decoded.insideTemperature,
                             decoded2.insideTemperature)
            self.assertEqual(decoded.insideHumidity, decoded2.insideHumidity)
            self.assertEqual(decoded.outsideHumidity, decoded2.outsideHumidity)
            self.assertEqual(decoded.averageWindSpeed,
                             decoded2.averageWindSpeed)
            self.assertEqual(decoded.highWindSpeed, decoded2.highWindSpeed)
            self.assertEqual(decoded.highWindSpeedDirection,
                             decoded2.highWindSpeedDirection)
            self.assertEqual(decoded.prevailingWindDirection,
                             decoded2.prevailingWindDirection)
            self.assertEqual(decoded.averageUVIndex, decoded2.averageUVIndex)
            self.assertEqual(decoded.ET, decoded2.ET)
            self.assertEqual(decoded.highSolarRadiation,
                             decoded2.highSolarRadiation)
            self.assertEqual(decoded.highUVIndex, decoded2.highUVIndex)
            self.assertEqual(decoded.forecastRule, decoded2.forecastRule)
            self.assertEqual(decoded.leafTemperature, decoded2.leafTemperature)
            self.assertEqual(decoded.leafWetness, decoded2.leafWetness)
            self.assertEqual(decoded.soilTemperatures,
                             decoded2.soilTemperatures)
            self.assertEqual(decoded.extraHumidities, decoded2.extraHumidities)
            self.assertEqual(decoded.extraTemperatures,
                             decoded2.extraTemperatures)
            self.assertEqual(decoded.soilMoistures, decoded2.soilMoistures)

            self.assertEqual(record, encoded)
