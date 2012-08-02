# coding=utf-8
"""
Day-level data sources for Dygraphs charts.
"""
from data.daily import datasource_dispatch
from datetime import date, datetime
from cache import day_cache_control
from database import get_daily_records, get_daily_rainfall, get_latest_sample_timestamp, day_exists
import os
import web
from web.contrib.template import render_jinja
from config import db
import config
import json
from data.util import datetime_to_js_date, outdoor_sample_result_to_datatable, pretty_print

__author__ = 'David Goodwin'



# These are all the available datasources ('files') for the day datatable
# route.
dygraph_datasources = {
    'samples': {
        'desc': 'All outdoor samples for the day. Should be around 288 records.',
        'func': None
    },
    '7day_30m_avg_samples': {
        'desc': 'Averaged outdoor samples every 30 minutes for the past 7 days.',
        'func': get_7day_30mavg_samples_datatable
    },
    'indoor_samples': {
        'desc': 'All indoor samples for the day. Should be around 288 records.',
        'func': get_day_indoor_samples_datatable
    },
    '7day_30m_avg_indoor_samples': {
        'desc': 'Averaged indoor samples every 30 minutes for the past 7 days.',
        'func': get_7day_30mavg_indoor_samples_datatable
    },

}


class dg_json:
    """
    Gets data for a particular day in Googles DataTable format.
    """
    def GET(self, station, year, month, day, dataset):
        """
        Handles requests for per-day JSON data sources for Dygraphs charts
        format.
        :param station: Station to get data for
        :type station: str
        :param year: Year to get data for
        :type year: str
        :param month: month to get data for
        :type month: str
        :param day: day to get data for
        :type day: str
        :param dataset: The dataset (filename) to retrieve
        :type dataset: str
        :return: JSON Data for whatever dataset was requested.
        :raise: web.NotFound if an invalid request is made.
        """
        this_date = date(int(year),int(month),int(day))

        return datasource_dispatch(station,
                                   dygraph_datasources,
                                   dataset,
                                   this_date)