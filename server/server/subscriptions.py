# coding=utf-8
"""
Handles streaming samples around
"""
from datetime import datetime, timedelta
import pytz
from server.database import get_live_csv, get_sample_csv

__author__ = 'david'

subscriptions = {}
_last_sample_ts = None

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

def station_live_updated(station_code):
    """
    Called when the live data for the specified station has been updated in.
    the database. This will retrieve the data from the database and broadcast
    it out to all subscribers.
    :param station_code: Station that has just received new live data.
    :type station_code: str
    """

    get_live_csv(station_code).addCallback(_station_live_updated_callback, station_code)

def _station_samples_updated_callback(data, code):
    global _last_sample_ts

    for row in data:
        _last_sample_ts = row[0]
        # We use ISO 8601 date formatting for output.
        csv_data = 's,"{0}",{1}'.format(
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

    now = datetime.utcnow().replace(tzinfo = pytz.utc)
    if _last_sample_ts is None:
        # Take two minutes off so we're sure to grab what ever sample just
        # triggered this function.
        _last_sample_ts = now - timedelta(minutes=2)

    # If we've not broadcast anything for the last few hours we don't want to
    # suddenly spam all clients with a hundred records because someone
    # took the data logger back online. If our last broadcast was a few hours
    # ago then reset it to 20 minutes ago
    if _last_sample_ts < now - timedelta(hours=4):
        _last_sample_ts = now - timedelta(minutes=20)

    get_sample_csv(station_code, _last_sample_ts).addCallback(
        _station_samples_updated_callback, station_code)


