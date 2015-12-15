# coding=utf-8
"""
Handles streaming samples around
"""
from datetime import datetime, timedelta

import math
import pytz
from server.database import get_live_csv, get_sample_csv, get_station_hw_type

__author__ = 'david'

subscriptions = {}
_last_sample_ts = {}
_latest_live = {}

def subscribe(subscriber, station, include_live, include_samples):
    """
    Adds a new station data subscription
    :param subscriber: The object that data should be delivered to
    :param station: The station code the subscription is for
    :type station: str
    :param include_live: If live data is wanted
    :type include_live: bool
    :param include_samples: If samples are wanted
    :type include_samples: bool
    """
    global subscriptions, _latest_live

    if station not in subscriptions:
        subscriptions[station] = {"s":[],"l":[]}

    if include_live:
        subscriptions[station]["l"].append(subscriber)

        # Send the most recent sample if we have one.
        if station in _latest_live:
            subscriber.live_data(_latest_live[station])
    if include_samples:
        subscriptions[station]["s"].append(subscriber)

def unsubscribe(subscriber, station):
    """
    Removes the subscriber from the specified station
    :param subscriber: The object holding the subscription
    :param station: the station to unsubscribe from
    :type station: str
    """
    global subscriptions

    if subscriber in subscriptions[station]["s"]:
        subscriptions[station]["s"].remove(subscriber)

    if subscriber in subscriptions[station]["l"]:
        subscriptions[station]["l"].remove(subscriber)

def deliver_live_data(station, data):
    """
    delivers live data to all subscribers
    :param station: The station this data is for
    :type station: str
    :param data: Live data
    :type data: str
    """
    global subscriptions, _latest_live

    _latest_live[station] = data

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["l"]

    for subscriber in subscribers:
        subscriber.live_data(data)

def deliver_sample_data(station, data):
    """
    delivers sample data to all subscribers
    :param station: The station this data is for
    :type station: str
    :param data: Sample data
    :type data: str
    """
    global subscriptions

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["s"]

    for subscriber in subscribers:
        subscriber.sample_data(data)

def _station_live_updated_callback(data, code):

    row = data[0]

    dat = "l,{0}".format(row[0])
    deliver_live_data(code, dat)


# Format strings for live data broadcast over RabbitMQ.
base_live_fmt = "{outsideTemperature},{dewPoint},{apparentTemperature}," \
                "{windChill},{outsideHumidity},{insideTemperature}," \
                "{insideHumidity},{barometer},{windSpeed},{windDirection}"
davis_live_fmt = base_live_fmt + ",{barTrend},{rainRate},{stormRain}," \
                                 "{startDateOfCurrentStorm}," \
                                 "{transmitterBatteryStatus}," \
                                 "{consoleBatteryVoltage},{forecastIcons}," \
                                 "{forecastRuleNumber},{UV},{solarRadiation}"


def round_maybe_none_to_2dp(val):
    if val is None:
        return None
    return "{0:.2f}".format(val)


def wind_chill(temperature, wind_speed):
    """
    Calculates Wind Chill given temperature and wind speed. This is a port of
    the PL/pgSQL implementation.

    :param temperature: Outside temperature in degrees C
    :type temperature: float
    :param wind_speed: Average wind speed in m/s
    :type wind_speed: float
    :return: Wind speed in degrees C
    :rtype: float
    """
    if temperature is None or wind_speed is None:
        return None

    wind_kph = wind_speed * 3.6
    if wind_kph <= 4.8 or temperature > 10.0:
        return temperature

    result = 13.12 \
        + (temperature * 0.6215) \
        + (((0.3965 * temperature) - 11.37) * pow(wind_kph, 0.16))

    if result > temperature:
        return temperature

    return result


def apparent_temperature(temperature, wind_speed, relative_humidity):
    """
    Calculates Apparent Temperature given temperature, wind speed and humidity.
    This is a port of the PL/pgSQL implementation.

    :param temperature: Outside temperature in degrees C
    :type temperature: float
    :param wind_speed: Wind speed in m/s
    :type wind_speed: float
    :param relative_humidity: Relative Humidity
    :type relative_humidity: float
    :return: Apparent Temperature in degrees C
    :rtype: float
    """

    if temperature is None or wind_speed is None or relative_humidity is None:
        return None

    # Water Vapour Pressure in hPa
    wvp = (relative_humidity / 100.0) \
        * 6.105 * pow(2.718281828,
                      ((17.27 * temperature) / (237.7 + temperature)))

    return temperature + (0.33 * wvp) - (0.7 * wind_speed) - 4.00


def dew_point(temperature, relative_humidity):
    """
    Calculates Dew Point given temperature and relative humidity. This is a port
    of the PL/pgSQL function.
    :param temperature: Temperature in degrees C
    :type temperature: float
    :param relative_humidity: Humidity
    :type relative_humidity: float
    :return: Dew point in degrees C
    :rtype: float
    """

    if temperature is None or relative_humidity is None:
        return None

    a = 17.271
    b = 237.7  # in degrees C
    gamma = ((a * temperature) / (b + temperature)) + \
        math.log(relative_humidity / 100.0)

    return (b * gamma) / (a - gamma)


def station_live_updated(station_code, data=None):
    """
    Called when the live data for the specified station has been updated in.
    the database. This will retrieve the data from the database and broadcast
    it out to all subscribers.
    :param station_code: Station that has just received new live data.
    :type station_code: str
    :param data: Live data, unencoded
    :type data: dict
    """

    if data is None:
        get_live_csv(station_code).addCallback(_station_live_updated_callback,
                                               station_code)
    else:
        # Encode the data as CSV then fire it off.
        # The standard format is:
        # Temperature, DewPoint, ApparentTemperature, WindChill,
        # RelativeHumidity, IndoorTemperature, IndoorRelativeHumidity,
        # AbsolutePressure, AverageWindSpeed, WindDirection,
        #
        # If the station hardware type is DAVIS we use this format instead:
        # Temperature, DewPoint, ApparentTemperature, WindChill,
        # RelativeHumidity, IndoorTemperature, IndoorRelativeHumidity,
        # AbsolutePressure, AverageWindSpeed, WindDirection,

        # BarTrend, RainRate, StormRain, CurrentStormStartDate,
        # TransmitterBattery, ConsoleBattery, ForecastIcon, ForecastRuleId,
        # UVIndex, SolarRadiation

        # The keys in the data structure don't match the rest of zxweather as
        # the davis logger was the first one to support outputting live data
        # via RabbitMQ and it just serializes one of its internal data
        # structures out to JSON. Probably should have designed that better.
        # Oh well.

        # We need to calculate Dew Point, Apparent Temperature and Wind Chill
        # ourselves as these values aren't broadcast over RabbitMQ.

        data["dewPoint"] = dew_point(data["outsideTemperature"],
                                     data["outsideHumidity"])
        data["apparentTemperature"] = apparent_temperature(
                data["outsideTemperature"],
                data["windSpeed"],
                data["outsideHumidity"])

        data["windChill"] = wind_chill(
                data["outsideTemperature"], data["windSpeed"])

        data["outsideTemperature"] = round_maybe_none_to_2dp(
                data["outsideTemperature"])
        data["dewPoint"] = round_maybe_none_to_2dp(data["dewPoint"])
        data["apparentTemperature"] = round_maybe_none_to_2dp(
                data["apparentTemperature"])
        data["windChill"] = round_maybe_none_to_2dp(data["windChill"])
        data["insideTemperature"] = round_maybe_none_to_2dp(
                data["insideTemperature"])
        data["windSpeed"] = round_maybe_none_to_2dp(data["windSpeed"])
        data["barometer"] = round_maybe_none_to_2dp(data["barometer"])

        if get_station_hw_type(station_code) == 'DAVIS':
            data["consoleBatteryVoltage"] = \
                round_maybe_none_to_2dp(data["consoleBatteryVoltage"])

            val = davis_live_fmt.format(**data)
        else:
            val = base_live_fmt.format(**data)

        # _station_live_updated_callback is expecting the result of a database
        # query - one row with one column.
        val = [[val, ], ]
        _station_live_updated_callback(val, station_code)


def _station_samples_updated_callback(data, code):
    global _last_sample_ts

    for row in data:
        _last_sample_ts[code] = row[0]
        # We use ISO 8601 date formatting for output.
        csv_data = 's,{0},{1}'.format(
            row[0].strftime("%Y-%m-%d %H:%M:%S"), row[1])
        deliver_sample_data(code, csv_data)


def new_station_samples(station_code):
    """
    Called when new samples are available for the specified station. This will
    retrieve all new records since last time and broadcast them out to all
    subscribers.
    :param station_code:
    :return:
    """
    global _last_sample_ts

    if station_code not in _last_sample_ts:
        _last_sample_ts[station_code] = None

    if _last_sample_ts[station_code] is not None:
        now = datetime.utcnow().replace(tzinfo = pytz.utc)

        # If we've not broadcast anything for the last few hours we don't want
        # to suddenly spam all clients with a hundred records because someone
        # took the data logger back online. If our last broadcast was a few
        # hours ago then reset it to 20 minutes ago
        if _last_sample_ts[station_code] < now - timedelta(hours=4):
            _last_sample_ts[station_code] = now - timedelta(minutes=20)

    get_sample_csv(station_code, _last_sample_ts[station_code]).addCallback(
        _station_samples_updated_callback, station_code)


