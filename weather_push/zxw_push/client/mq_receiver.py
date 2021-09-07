import json
from datetime import datetime
from twisted.internet import reactor, protocol, defer, task
from twisted.python import log
from ..common.util import Event


__author__ = 'david'

class RabbitMqReceiver(object):
    """
    Receives live data updates via RabbitMQ
    """

    def __init__(self, username, password, vhost, hostname, port, exchange):
        self._username = username
        self._password = password
        self._vhost = vhost
        self._hostname = hostname
        self._port = port
        self._exchange = exchange

        self.LiveUpdate = Event()

        self._station_codes = []

    def connect(self, latest_sample_info):
        """
        Connects to RabbitMQ. The supplied latest sample info is used as a
        list of stations the remote server knows about.

        :param latest_sample_info: Station info from remote server
        :return:
        """
        log.msg(latest_sample_info)
        # Grab a list of stations the remote server knows about
        for station in latest_sample_info:
            self._station_codes.append(station)

        try:
            log.msg("Attempting to connect to message broker...")
            import pika
            from pika.credentials import PlainCredentials
            from pika.adapters import twisted_connection

            credentials = PlainCredentials(self._username, self._password)

            parameters = pika.ConnectionParameters(virtual_host=self._vhost,
                                                   credentials=credentials)
            cc = protocol.ClientCreator(
                reactor, twisted_connection.TwistedProtocolConnection,
                parameters)

            d = cc.connectTCP(self._hostname, self._port)
            d.addCallback(lambda proto: proto.ready)
            d.addCallback(self._mq_setup)

        except ImportError:
            log.msg("*** WARNING: RabbitMQ Client library (pika) is not "
                    "available. Data will NOT be received from RabbitMQ.")

    @defer.inlineCallbacks
    def _mq_setup(self, connection):
        log.msg("Broker connected. Configuring...")

        channel = yield connection.channel()

        yield channel.exchange_declare(exchange=self._exchange, exchange_type='topic')

        result = yield channel.queue_declare(exclusive=True)

        queue_name = result.method.queue

        # routing keys are:
        #   <station-code>.<data-type>
        # for example:
        #   rua.live
        #   rua.samples
        # We're interested in live data only for all stations
        routing_key = "*.live"

        yield channel.queue_bind(exchange=self._exchange, queue=queue_name,
                                 routing_key=routing_key)

        yield channel.basic_qos(prefetch_count=1)

        queue_object, consumer_tag = yield channel.basic_consume(
            queue=queue_name, no_ack=True)

        l = task.LoopingCall(self._read, queue_object)

        l.start(0.01)

    @defer.inlineCallbacks
    def _read(self, queue_object):

        ch, method, properties, body = yield queue_object.get()

        station_code = method.routing_key.split(".")[0].upper()

        if station_code not in self._station_codes:
            return  # Station isn't known to the server. Ignore it.

        if properties.content_type.lower() != "application/json":
            log.msg("Unsupported content type {0} (source app_id is {1})".format(
                properties.content_type, properties.app_id))
            return

        # Only davis stations are supported at this time.
        if properties.type is None or properties.type.lower() != "davislive":
            log.msg("Unsupported message type {0} (source app_id is {1})".format(
                properties.type, properties.app_id
            ))

        # See the mq-example.json file shipped with the davis-logger for an
        # example of what this document looks like.
        data = json.loads(body)

        if data["startDateOfCurrentStorm"] is not None:
            current_storm_date = datetime.strptime(data["startDateOfCurrentStorm"],
                                                   "%Y-%m-%d").date()
        else:
            current_storm_date = None

        live_packet = {
            # Generic live data
            "station_code": station_code,
            "download_timestamp": datetime.now(),
            "indoor_humidity": data["insideHumidity"],
            "indoor_temperature": data["insideTemperature"],
            "temperature": data["outsideTemperature"],
            "humidity": data["outsideHumidity"],
            "pressure": data["absoluteBarometricPressure"],
            "msl_pressure": data["barometer"],
            "average_wind_speed": data["windSpeed"],
            "gust_wind_speed": None,
            "wind_direction": data["windDirection"],

            # Davis specific data
            "bar_trend": data["barTrend"],
            "rain_rate": data["rainRate"],
            "storm_rain": data["stormRain"],
            "current_storm_start_date": current_storm_date,
            "transmitter_battery": data["transmitterBatteryStatus"],
            "console_battery_voltage": data["consoleBatteryVoltage"],
            "forecast_icon": data["forecastIcons"],
            "forecast_rule_id": data["forecastRuleNumber"],
            "uv_index": data["UV"],
            "solar_radiation": data["solarRadiation"],
            "average_wind_speed_2m": data["averageWindSpeed2min"],
            "average_wind_speed_10m": data["averageWindSpeed10min"],
            "gust_wind_speed_10m": data["windGust10m"],
            "gust_wind_direction_10m": data["windGust10mDirection"],
            "heat_index": data["heatIndex"],
            "thsw_index": data["thswIndex"],
            "altimeter_setting": data["altimeterSetting"],

            "extra_fields": {
                "leaf_wetness_1": data["leafWetness1"],
                "leaf_wetness_2": data["leafWetness2"],
                "leaf_temperature_1": data["leafTemperature1"],
                "leaf_temperature_2": data["leafTemperature2"],
                # The other two leaf wetness and temperature values aren't supported
                # here as there appears to be no way to actually use them - the docs
                # claim you can only have one Leaf station (broadcasting 2 leaf
                # sensors) plus a Soil station (broadcasting no leaf sensors) or
                # a leaf+soil station broadcasting only 2 leaf sensors.
                "soil_moisture_1": data["soilMoisture1"],
                "soil_moisture_2": data["soilMoisture2"],
                "soil_moisture_3": data["soilMoisture3"],
                "soil_moisture_4": data["soilMoisture4"],
                "soil_temperature_1": data["soilTemperature1"],
                "soil_temperature_2": data["soilTemperature2"],
                "soil_temperature_3": data["soilTemperature3"],
                "soil_temperature_4": data["soilTemperature4"],
                "extra_temperature_1": data["extraTemperature1"],
                "extra_temperature_2": data["extraTemperature2"],
                "extra_temperature_3": data["extraTemperature3"],
                "extra_humidity_1": data["extraHumidity1"],
                "extra_humidity_2": data["extraHumidity2"]
                # The other extra temperatures and humidities aren't logged so we
                # simply don't bother supporting them at all.
            }
        }

        self.LiveUpdate.fire(live_packet, "DAVIS")

