# coding=utf-8
"""
Listens for updates from the database and broadcasts them out to all
subscribers.
"""
from txpostgres import txpostgres
from server.subscriptions import station_live_updated, new_station_samples, \
    new_image

__author__ = 'david'


def observer(notify):
    """
    Called when ever notifications are received.
    :param notify: The notification.
    """

    if notify.channel == "live_data_updated":
        station_live_updated(notify.payload)
    elif notify.channel == "new_sample":
        new_station_samples(notify.payload)
    elif notify.channel == "new_image":
        new_image(notify.payload)


def listener_connect(connection_string):
    """
    Connects the database notification listener
    :param connection_string: Database connection string
    :type connection_string: str
    """
    global _listen_conn, _listen_conn_d, _last_sample_ts

    # connect to the database
    _listen_conn = txpostgres.Connection()
    _listen_conn_d = _listen_conn.connect(connection_string)

    # add a NOTIFY observer
    _listen_conn.addNotifyObserver(observer)

    _listen_conn_d.addCallback(lambda _: _listen_conn.runOperation(
            "listen live_data_updated"))
    _listen_conn_d.addCallback(lambda _: _listen_conn.runOperation(
            "listen new_sample"))
    _listen_conn_d.addCallback(lambda _: _listen_conn.runOperation(
            "listen new_image"))