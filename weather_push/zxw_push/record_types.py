# coding=utf-8
from collections import namedtuple

__author__ = 'david'
#
## For uploading only - missing sample_id
#BaseLiveRecord = namedtuple(
#    'BaseLiveRecord',
#    (
#        'station_code', 'download_timestamp', 'indoor_humidity',
#        'indoor_temperature', 'temperature', 'humidity', 'pressure',
#        'average_wind_speed', 'gust_wind_speed', 'wind_direction'
#        )
#)
#
#BaseSampleRecord = namedtuple(
#    'BaseSampleRecord',
#    (
#        'station_code', 'temperature', 'humidity', 'indoor_temperature',
#        'indoor_humidity', 'pressure', 'average_wind_speed', 'gust_wind_speed',
#        'wind_direction', 'rainfall', 'download_timestamp', 'time_stamp'
#        )
#)
#
## For uploading only - missing sample_id
#WH1080SampleRecord = namedtuple(
#    'WH1080SampleRecord',
#    (
#        'sample_interval', 'record_number', 'last_in_batch', 'invalid_data',
#        'wind_direction', 'total_rain', 'rain_overflow'
#        )
#)