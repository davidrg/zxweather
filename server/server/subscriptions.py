# coding=utf-8
"""
Handles streaming samples around
"""
from datetime import datetime, timedelta

import math
import pytz
from server.database import get_live_csv, get_sample_csv, get_station_hw_type, \
    get_image_csv

__author__ = 'david'

subscriptions = {}
_last_sample_ts = {}
_latest_live = {}


def subscribe(subscriber, station, include_live, include_samples,
              include_images, any_order=False):
    """
    Adds a new station data subscription
    :param subscriber: The object that data should be delivered to
    :param station: The station code the subscription is for
    :type station: str
    :param include_live: If live data is wanted
    :type include_live: bool
    :param include_samples: If samples are wanted
    :type include_samples: bool
    :param include_images: If images are wanted. Image subscriptions only
                           contain enough data to find the image, they don't
                           include the images themselves.
    :type include_images: bool
    :param any_order: If samples should be streamed as they're inserted into
                      the database. When OFF out-of-order samples will be
                      ignored entirely
    :type any_order: bool
    """
    global subscriptions, _latest_live

    station = station.lower()

    if station not in subscriptions:
        subscriptions[station] = {"s": [], "sa": [], "l": [], "i": []}

    if include_live:
        subscriptions[station]["l"].append(subscriber)

        # Send the most recent sample if we have one.
        if station in _latest_live:
            subscriber.live_data(_latest_live[station])
    if include_samples:
        if any_order:
            subscriptions[station]["sa"].append(subscriber)
        else:
            subscriptions[station]["s"].append(subscriber)
    if include_images:
        subscriptions[station]["i"].append(subscriber)


def unsubscribe(subscriber, station):
    """
    Removes the subscriber from the specified station
    :param subscriber: The object holding the subscription
    :param station: the station to unsubscribe from
    :type station: str
    """
    global subscriptions

    station = station.lower()

    if subscriber in subscriptions[station]["s"]:
        subscriptions[station]["s"].remove(subscriber)

    if subscriber in subscriptions[station]["sa"]:
        subscriptions[station]["sa"].remove(subscriber)

    if subscriber in subscriptions[station]["l"]:
        subscriptions[station]["l"].remove(subscriber)

    if subscriber in subscriptions[station]["i"]:
        subscriptions[station]["i"].remove(subscriber)


def deliver_live_data(station, data):
    """
    delivers live data to all subscribers
    :param station: The station this data is for
    :type station: str
    :param data: Live data
    :type data: str
    """
    global subscriptions, _latest_live

    station = station.lower()

    _latest_live[station] = data

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["l"]

    for subscriber in subscribers:
        subscriber.live_data(data)


def deliver_image_data(station, data):
    """
    delivers image data to all subscribers
    :param station: The station this image is for
    :type station: str
    :param data: Image data
    :type data: str
    """
    global subscriptions

    station = station.lower()

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["i"]

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

    station = station.lower()

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["s"]

    for subscriber in subscribers:
        subscriber.sample_data(data)


def deliver_unordered_sample_data(station, data):
    """
    delivers potentially unordered sample data to all subscribers
    :param station: The station this data is for
    :type station: str
    :param data: Sample data
    :type data: str
    """
    global subscriptions

    station = station.lower()

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["sa"]

    for subscriber in subscribers:
        subscriber.sample_data(data)


def _station_live_updated_callback(data, code):

    code = code.lower()

    row = data[0]

    dat = "l,{0}".format(row[0])
    deliver_live_data(code, dat)


def _new_image_callback(data):
    row = data[0]
    station_code = row[0].lower()
    source_code = row[1].lower()
    type_code = row[2].lower()
    time_stamp = row[3]  # Timzone is at original local time
    mime_type = row[4]
    id = row[5]

    dat = "i,{station_code},{source_code},{type_code},{timestamp}," \
          "{mime_type},{id}".format(
            station_code=station_code,
            source_code=source_code,
            type_code=type_code,
            timestamp=time_stamp.isoformat(),
            mime_type=mime_type,
            id=id)

    deliver_image_data(station_code, dat)


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

    station_code = station_code.lower()

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
        # UVIndex, SolarRadiation, leafWetnesses, leafTemperatures,
        # soilMoistures, soilTemperatures, extraTemperatures, extraHumidities

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


def new_image(image_id):
    # We have the ID of a new image. We need to fetch that images metadata
    # (including the station its source is associated with) then push the
    # notification out.
    get_image_csv(image_id).addCallback(_new_image_callback)


def _station_samples_updated_callback(data, code):
    global _last_sample_ts

    code = code.lower()

    for row in data:
        prev_ts = _last_sample_ts[code]

        # We use ISO 8601 date formatting for output.
        csv_data = 's,{0},{1}'.format(row[0], row[1])

        # These subscribers don't care if they get the occasional sample
        # out-of-order as long as they get all the samples.
        deliver_unordered_sample_data(code, csv_data)

        if prev_ts is None or row[0] > prev_ts:
            _last_sample_ts[code] = row[0]

            # These subscribers only want sampled where the timestamp is greater
            # than the timestamp on the previous sample they received.
            deliver_sample_data(code, csv_data)


def new_station_sample(station_code, sample_id):
    """
    Called when a new sample with the specified id is available for the
    specified station. This will fetch the sample and broadcast it.
    :param station_code: Station the sample is for
    :type station_code: str
    :param sample_id: ID of the sample
    :type sample_id: int
    :return:
    """

    global _last_sample_ts

    station_code = station_code.lower()

    if station_code not in _last_sample_ts:
        _last_sample_ts[station_code] = None

    if _last_sample_ts[station_code] is not None:
        now = datetime.utcnow().replace(tzinfo=pytz.utc)

        # If we've not broadcast anything for the last few hours we don't want
        # to suddenly spam all clients with a hundred records because someone
        # took the data logger back online. If our last broadcast was a few
        # hours ago then reset it to 20 minutes ago
        if _last_sample_ts[station_code] < now - timedelta(hours=4):
            _last_sample_ts[station_code] = now - timedelta(minutes=20)

    get_sample_csv(station_code, None, None, sample_id).addCallback(
        _station_samples_updated_callback, station_code)


