# coding=utf-8
import json
from twisted.internet import reactor, protocol, defer, task
from twisted.python import log
from util import Event


__author__ = 'david'

class RabbitMqReceiver(object):
    """
    Receives live data updates via RabbitMQ
    """

    def __init__(self, username, password, vhost, hostname, port, exchange,
                 station_code):
        self._username = username
        self._password = password
        self._vhost = vhost
        self._hostname = hostname
        self._port = port
        self._exchange = exchange

        self.LiveUpdate = Event()

        self._station_code = station_code

    def connect(self):
        """
        Connects to RabbitMQ.

        :param station_code: Station code to listen for
        :return:
        """

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

        yield channel.exchange_declare(exchange=self._exchange, type='topic')

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

        station_code = method.routing_key.split(".")[0]

        if station_code != self._station_code:
            return  # Station isn't the one we care about

        if properties.content_type.lower() != "application/json":
            log.msg("Unsupported content type {0} (source app_id is {1})".format(
                properties.content_type, properties.app_id))
            return

        # Only davis stations are supported at this time.
        if properties.type is None or properties.type.lower() != "davislive":
            log.msg("Unsupported message type {0} (source app_id is {1})".format(
                properties.type, properties.app_id
            ))

        data = json.loads(body)

        self.LiveUpdate.fire(data["UV"], data["solarRadiation"])

