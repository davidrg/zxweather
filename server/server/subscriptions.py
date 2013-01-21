# coding=utf-8
"""
Handles streaming samples around
"""
from server.database import get_live_csv, get_station_hw_type

__author__ = 'david'

subscriptions = {}

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
    global subscriptions

    if station not in subscriptions:
        subscriptions[station] = {"s":[],"l":[]}

    if include_live:
        subscriptions[station]["l"].append(subscriber)
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
    global subscriptions

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
    :type data: list
    """
    global subscriptions

    if station not in subscriptions:
        return

    subscribers = subscriptions[station]["s"]

    for subscriber in subscribers:
        for record in data:
            subscriber.sample_data(record)

def _station_live_updated_callback(data, code):

    row = data[0]

    hw_type = get_station_hw_type(code)

    # Todo: Find some way of telling the client the record type (hardware)
    # that doesn't involve sending it in *every* record. The client only needs
    # to know once.

    dat = "l,{0},{1},{2}".format(code, hw_type, row[0])
    deliver_live_data(code, dat)

def station_live_updated(station_code):
    """
    Called when the live data for the specified station has been updated in.
    the database. This will retrieve the data from the database and broadcast
    it out to all subscribers.
    :param station_code: Station that has just received new live data.
    :type station_code: str
    """

    get_live_csv(station_code).addCallback(_station_live_updated_callback, station_code)

def new_station_samples(station_code):
    """
    Called when new samples are available for the specified station. This will
    retrieve all new records since last time and broadcast them out to all
    subscribers.
    :param station_code:
    :return:
    """

    print("New sample for " + station_code)
    deliver_sample_data(station_code, ["sample ping!"])

