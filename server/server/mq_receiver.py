import json
from datetime import datetime
from twisted.internet import reactor, protocol, defer, task
from twisted.python import log

from server.subscriptions import station_live_updated

__author__ = 'david'

_instance = None


def mq_listener_connect(hostname, port, username, password, vhost, exchange):
    global _instance
    _instance = RabbitMqReceiver(username, password, vhost,
                                 hostname, port, exchange)
    _instance.connect()


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

    def connect(self):
        """
        Connects to RabbitMQ. The supplied latest sample info is used as a
        list of stations the remote server knows about.

        :param latest_sample_info: Station info from remote server
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

            def _conn_ready(c):
                c.ready.addCallback(lambda _: c)
                return c.ready

            d = cc.connectTCP(self._hostname, self._port)
            d.addCallback(_conn_ready)
            d.addCallback(self._mq_setup)

        except ImportError:
            log.msg("*** WARNING: RabbitMQ Client library (pika) is not "
                    "available. Data will NOT be received from RabbitMQ.")

    @defer.inlineCallbacks
    def _mq_setup(self, connection):
        log.msg("Broker connected. Configuring...")

        channel = yield connection.channel()

        yield channel.exchange_declare(exchange=self._exchange,
                                       exchange_type='topic')

        # '' = no queue name = auto-generate one
        result = yield channel.queue_declare('', exclusive=True)

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
            queue=queue_name, auto_ack=True)

        log.msg("Broker configured.")

        l = task.LoopingCall(self._read, queue_object)

        l.start(0.01)

    @defer.inlineCallbacks
    def _read(self, queue_object):

        ch, method, properties, body = yield queue_object.get()

        station_code = method.routing_key.split(".")[0].lower()

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

        if data["startDateOfCurrentStorm"] is not None:
            current_storm_date = datetime.strptime(data["startDateOfCurrentStorm"],
                                                   "%Y-%m-%d").date()
        else:
            current_storm_date = None

        data["startDateOfCurrentStorm"] = current_storm_date

        station_live_updated(station_code, data)


