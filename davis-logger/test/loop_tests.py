# coding=utf-8
"""
Tests the functions that deal with the loop packet type.
"""
from datetime import date, time
import unittest
from davis_logger.record_types.loop import Loop, serialise_loop, deserialise_loop

__author__ = 'david'


class loop_tests(unittest.TestCase):

    def test_round_trip(self):
        loop = Loop(
            barTrend = -20,
            nextRecord = 5,
            barometer = 21.3,
            insideTemperature = 25.2,
            insideHumidity = 50,
            outsideTemperature = 18.3,
            windSpeed = 1,
            averageWindSpeed10min = 5,
            windDirection = 120,
            extraTemperatures = [
                0,
                10,
                0,
                0,
                0,
                0,
                0],
            soilTemperatures = [
                0,
                0,
                0,
                0],
            leafTemperatures = [
                0,
                0,
                0,
                0],
            outsideHumidity = 98,
            extraHumidities = [
                0,
                0,
                0,
                0,
                0,
                0,
                0],
            rainRate = 20,
            UV = 1,
            solarRadiation = 2,
            stormRain = 20,
            startDateOfCurrentStorm = date(year=2012,month=6,day=15),
            dayRain = 20,         # Station config
            monthRain = 30,     # affects these
            yearRain = 40,       # values
            dayET = 0,
            monthET = 1,
            yearET = 2,
            soilMoistures = [
                0,
                1,
                2,
                3],
            leafWetness = [
                0,
                1,
                2,
                3],
            insideAlarms = 1,
            rainAlarms = 1,
            outsideAlarms = [
                2,
                2],
            extraTempHumAlarms = [
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10],
            soilAndLeafAlarms = [
                0,
                1,
                2,
                3],
            transmitterBatteryStatus = 1,
            consoleBatteryVoltage = 6.3,
            forecastIcons = 1,
            forecastRuleNumber = 2,
            timeOfSunrise = time(hour=6,minute=00),
            timeOfSunset = time(hour=18,minute=20)
        )

        encoded = serialise_loop(loop)
        decoded = deserialise_loop(encoded[0:97])
        print(repr(encoded))
        self.maxDiff = None
        self.assertDictEqual(decoded._asdict(), loop._asdict())
