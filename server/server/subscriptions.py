# coding=utf-8
"""
Handles streaming samples around
"""
import math

from server.database import get_live_csv, get_sample_csv, get_station_hw_type, \
    get_image_csv

__author__ = 'david'

subscriptions = dict()


class StationSubscriptionManager(object):
    """
    Handles subscriptions for a single weather station
    """
    def __init__(self, station_code):
        self._station_code = station_code
        self._sample_subscribers = dict()
        self._ordered_sample_subscribers = dict()
        self._live_subscribers = dict()
        self._latest_live = dict()
        self._image_subscribers = []
        self._last_sample_ts = dict()

    def subscribe(self, subscriber, live, samples, images,
                  samples_in_any_order=False, live_format=1, sample_format=1):
        """
        Adds subscriptions for the specified subscriber.

        Note that if samples must be delivered in order then any out-of-order
        samples will be discarded - they won't be buffered and delivered later.

        :param subscriber:
        :param live: Receive live data
        :type live: bool
        :param samples: Receive new samples
        :type samples: bool
        :param images: Receive new image notifications
        :type images: bool
        :param samples_in_any_order: If samples can be delivered in any order
        :type samples_in_any_order: bool
        :param live_format: Format for live data records
        :type live_format: int
        :param sample_format: Format for sample records
        :type sample_format: int
        """
        if live:
            self.add_live_subscription(subscriber, live_format)

        if samples:
            self.add_sample_subscription(subscriber, samples_in_any_order, sample_format)

        if images:
            self.add_image_subscription(subscriber)

    def unsubscribe_all(self, subscriber):
        """
        Removes all subscriptions for the specified subscriber
        """
        self.remove_live_subscription(subscriber)
        self.remove_sample_subscription(subscriber)
        self.remove_image_subscription(subscriber)

    def add_sample_subscription(self, subscriber, any_order, sample_format):
        """
        Adds a subscription for samples/archive records.

        If samples must be delivered in order some samples may be missed (they
        won't be queued so they can be delivered in order)

        :param subscriber: An object to receive new samples
        :param any_order: If samples can be delivered in any order
        :type any_order: bool
        :param sample_format: Format to deliver samples in
        :type sample_format: int
        """

        if any_order:
            if sample_format not in self._sample_subscribers:
                self._sample_subscribers[sample_format] = []

            if subscriber in self._sample_subscribers[sample_format]:
                return

            self._sample_subscribers[sample_format].append(subscriber)
        else:
            if sample_format not in self._ordered_sample_subscribers:
                self._ordered_sample_subscribers[sample_format] = []

            if subscriber in self._ordered_sample_subscribers[sample_format]:
                return

            self._ordered_sample_subscribers[sample_format].append(subscriber)

    def remove_sample_subscription(self, subscriber):
        """
        Removes the sample subscription for the specified subscriber
        :param subscriber: Subscriber to unsubscribe
        """

        for fmt in self._sample_subscribers.keys():
            if fmt in self._sample_subscribers:
                if subscriber in self._sample_subscribers[fmt]:
                    self._sample_subscribers[fmt].remove(subscriber)

        for fmt in self._ordered_sample_subscribers.keys():
            if fmt in self._ordered_sample_subscribers:
                if subscriber in self._ordered_sample_subscribers[fmt]:
                    self._ordered_sample_subscribers[fmt].remove(subscriber)

    def add_image_subscription(self, subscriber):
        """
        Subscribe to new image notifications
        :param subscriber: Object to receive new image notifications
        """
        if subscriber not in self._image_subscribers:
            self._image_subscribers.append(subscriber)

    def remove_image_subscription(self, subscriber):
        """
        Removes the new image subscription for the specified subscriber
        :param subscriber:
        """
        if subscriber in self._image_subscribers:
            self._image_subscribers.remove(subscriber)

    def add_live_subscription(self, subscriber, record_format):
        """
        Adds a live data subscription for the specified subscriber
        :param subscriber: Subscriber to receive new live data
        :param record_format: Format the data should be delivered in
        """
        if record_format not in self._live_subscribers:
            self._live_subscribers[record_format] = []
            self._latest_live[record_format] = None

        if subscriber not in self._live_subscribers[record_format]:
            self._live_subscribers[record_format].append(subscriber)

            if self._latest_live[record_format] is not None:
                subscriber.live_data(self._latest_live[record_format])

    def remove_live_subscription(self, subscriber, record_format=None):
        """
        Removes the live data subscription of the specified format for the
        specified subscriber
        :param subscriber:
        :param record_format: format to remove the subscription for or None to
                              remove all subscriptions
        """
        if record_format is None:
            # Remove subscription for all formats
            for fmt in self._live_subscribers.keys():
                if subscriber in self._live_subscribers[fmt]:
                    self._live_subscribers[fmt].remove(subscriber)
            return

        if record_format not in self._live_subscribers:
            return

        if subscriber in self._live_subscribers[record_format]:
            self._live_subscribers[record_format].remove(subscriber)

    def deliver_sample(self, data, record_format):
        """
        Delivers a sample to all subscribers
        :param data: Data to deliver
        :type data: tuple of timestamp and str
        :param record_format: Format of the data being delivered
        :type record_format: int
        """

        for row in data:
            csv_data = 's,' + row[1]

            # These subscribers don't care if they get the occasional sample
            # out-of-order as long as they get all the samples.
            if record_format in self._sample_subscribers.keys():
                for subscriber in self._sample_subscribers[record_format]:
                    subscriber.sample_data(csv_data)

            # if _last_sample_ts[station_code] is not None:
            #     now = datetime.utcnow().replace(tzinfo=pytz.utc)
            # 
            #     # If we've not broadcast anything for the last few hours we don't want
            #     # to suddenly spam all clients with a hundred records because someone
            #     # took the data logger back online. If our last broadcast was a few
            #     # hours ago then reset it to 20 minutes ago
            #     if _last_sample_ts[station_code] < now - timedelta(hours=4):
            #         _last_sample_ts[station_code] = now - timedelta(minutes=20)

            if record_format not in self._last_sample_ts:
                self._last_sample_ts[record_format] = None

            prev_ts = self._last_sample_ts[record_format]

            if prev_ts is None or row[0] > prev_ts:
                self._last_sample_ts[record_format] = row[0]

                # These subscribers only want sampled where the timestamp is greater
                # than the timestamp on the previous sample they received.
                if record_format in self._ordered_sample_subscribers.keys():
                    for subscriber in self._ordered_sample_subscribers[record_format]:
                        subscriber.sample_data(csv_data)

    def deliver_image(self, image):
        """
        Delivers a new image notification to all subscribers
        :param image:
        """
        for subscriber in self._image_subscribers:
            subscriber.live_data(image)

    def deliver_live(self, live, record_format):
        """
        Delivers live data of the specified format to all subscribers for that
        format.
        :param live:
        :param record_format:
        """
        if record_format not in self._live_subscribers:
            return

        for subscriber in self._live_subscribers[record_format]:
            subscriber.live_data(live)
            self._latest_live[record_format] = live

    def enabled_live_record_formats(self):
        """
        Gets a list of all subscribed live record formats.

        :rtype: List of int
        """
        return [k for k in self._live_subscribers.keys() if len(self._live_subscribers[k]) > 0]

    def enabled_sample_record_formats(self):
        """
        Gets a list of all subscribed sample record formats.

        :rtype: set of int
        """
        return set([k for k in self._sample_subscribers.keys()
                    if len(self._sample_subscribers[k]) > 0] +
                   [k for k in self._ordered_sample_subscribers.keys()
                    if len(self._ordered_sample_subscribers[k]) > 0])


def subscribe(subscriber, station, include_live, include_samples,
              include_images, live_format, sample_format, any_order=False):
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
    :param live_format: Live record format
    :type live_format: int
    :param sample_format: Sample record format
    :type sample_format: int
    :param any_order: If samples should be streamed as they're inserted into
                      the database. When OFF out-of-order samples will be
                      ignored entirely
    :type any_order: bool
    """
    global subscriptions

    station = station.lower()

    if station not in subscriptions:
        subscriptions[station] = StationSubscriptionManager(station)

    mgr = subscriptions[station]  # Type: StationSubscriptionManager

    mgr.subscribe(subscriber, include_live, include_samples, include_images,
                  any_order, live_format, sample_format)


def unsubscribe(subscriber, station):
    """
    Removes the subscriber from the specified station
    :param subscriber: The object holding the subscription
    :param station: the station to unsubscribe from
    :type station: str
    """
    global subscriptions

    if station.lower() not in subscriptions:
        return

    subscriptions[station.lower()].unsubscribe_all(subscriber)


def _station_live_updated_callback(data, record_format, code):
    global subscriptions

    code = code.lower()

    row = data[0]

    dat = "l,{0}".format(row[0])

    if code not in subscriptions:
        return

    subscriptions[code].deliver_live(dat, record_format)


def _new_image_callback(data):
    global subscriptions

    row = data[0]
    station_code = row[0].lower()
    source_code = row[1].lower()
    type_code = row[2].lower()
    time_stamp = row[3]  # Timezone is at original local time
    mime_type = row[4]
    id = row[5]

    if station_code not in subscriptions:
        return

    dat = "i,{station_code},{source_code},{type_code},{timestamp}," \
          "{mime_type},{id}".format(
            station_code=station_code,
            source_code=source_code,
            type_code=type_code,
            timestamp=time_stamp.isoformat(),
            mime_type=mime_type,
            id=id)

    # deliver_image_data(station_code, dat)
    subscriptions[station_code].deliver_image(dat)


# Format strings for live data broadcast over RabbitMQ.
live_formats = {
    1: {
        "base": "{outsideTemperature},{dewPoint},{apparentTemperature}," 
                "{windChill},{outsideHumidity},{insideTemperature}," 
                "{insideHumidity},{barometer},{windSpeed},{windDirection}",
        "davis": ",{barTrend},{rainRate},{stormRain},{startDateOfCurrentStorm}," 
                 "{transmitterBatteryStatus},{consoleBatteryVoltage},"
                 "{forecastIcons},{forecastRuleNumber},{UV},{solarRadiation},"
                 "{leafWetness_1},{leafWetness_2},{leafTemperature_1},"
                 "{leafTemperature_2},{soilMoisture_1},{soilMoisture_2},"
                 "{soilMoisture_3},{soilMoisture_4},{soilTemperature_1},"
                 "{soilTemperature_2},{soilTemperature_3},"
                 "{soilTemperature_4}{extraTemperature_1},"
                 "{extraTemperature_2},{extraTemperature_3},"
                 "{extraHumidity_1},{extraHumidity_2}"
    },
    2: {
        "base": "{timestamp},{outsideTemperature},{dewPoint},"
                "{apparentTemperature},{windChill},{outsideHumidity},"
                "{insideTemperature},{insideHumidity},"
                "{absoluteBarometricPressure},{barometer},{windSpeed},"
                "{windDirection}",
        "davis": ",{barTrend},{rainRate},{stormRain},{startDateOfCurrentStorm}," 
                 "{transmitterBatteryStatus},{consoleBatteryVoltage},"
                 "{forecastIcons},{forecastRuleNumber},{UV},{solarRadiation},"
                 "{averageWindSpeed2min},{averageWindSpeed10min},{windGust10m},"
                 "{windGust10mDirection},{heatIndex},{thswIndex},"
                 "{altimeterSetting}{leafWetness1},{leafWetness2},"
                 "{leafTemperature1},{leafTemperature2},{soilMoisture1},"
                 "{soilMoisture2},{soilMoisture3},{soilMoisture4},"
                 "{soilTemperature1},{soilTemperature2},{soilTemperature3},"
                 "{soilTemperature4}{extraTemperature1},{extraTemperature2},"
                 "{extraTemperature3},{extraHumidity1},{extraHumidity2}"
    }
}


def round_maybe_none_to_2dp(val):
    """
    Rounds a float to 2dp if its not None
    :param val: Value to round
    :type val: float
    :return: Value as a 2dp string or None
    :rtype: str or None
    """
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

    if station_code not in subscriptions:
        return  # No active subscriptions for the station - ignore it.

    if data is None:
        formats = subscriptions[station_code].enabled_live_record_formats()

        for f in formats:
            get_live_csv(station_code, f).addCallback(
                _station_live_updated_callback, f, station_code)
    else:
        # Data isn't coming from the database - we've got to format it
        # ourselves. So encode the data as CSV then fire it off.

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

        # data["windChill"] = wind_chill(
        #         data["outsideTemperature"], data["windSpeed"])

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

        is_davis = get_station_hw_type(station_code) == 'DAVIS'

        for record_format in subscriptions[station_code].enabled_live_record_formats():
            base_format = live_formats[record_format]["base"]
            if is_davis:
                data["consoleBatteryVoltage"] = \
                    round_maybe_none_to_2dp(data["consoleBatteryVoltage"])

                format_string = base_format + live_formats[record_format]["davis"]

                val = format_string.format(**data)
            else:
                val = base_format.format(**data)

            # _station_live_updated_callback is expecting the result of a database
            # query - one row with one column.
            val = [[val, ], ]
            _station_live_updated_callback(val, record_format, station_code)


def new_image(image_id):
    """
    Called whenever a new image is available in the database
    :param image_id: ID of the new image
    :tyep image_id: int
    """
    # We have the ID of a new image. We need to fetch that images metadata
    # (including the station its source is associated with) then push the
    # notification out.
    get_image_csv(image_id).addCallback(_new_image_callback)


def _station_samples_updated_callback(data, code, record_format):
    global subscriptions

    code = code.lower()

    if code not in subscriptions:
        return

    subscriptions[code].deliver_sample(data, record_format)


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

    station_code = station_code.lower()

    if station_code not in subscriptions:
        return  # No active subscriptions for the station - ignore it

    record_formats = subscriptions[station_code].enabled_sample_record_formats()

    for record_format in record_formats:
        get_sample_csv(station_code, record_format, None, None, sample_id).addCallback(
            _station_samples_updated_callback, station_code, record_format)
