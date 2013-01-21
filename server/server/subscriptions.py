# coding=utf-8
"""
Handles streaming samples around
"""
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

    subscriptions[station]["s"].remove(subscriber)
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

    subscribers = subscriptions[station]["s"]

    for subscriber in subscribers:
        subscriber.sample_data(data)