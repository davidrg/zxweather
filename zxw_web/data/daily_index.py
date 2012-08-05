# coding=utf-8
"""
Index page for day level data sources.
"""
from datetime import date
import os
import web
from web.contrib.template import render_jinja
from data.daily import datasources, datatable_datasources
from database import day_exists

__author__ = 'David Goodwin'


class index:
    """
    Provides an index of available daily data sources
    """
    def GET(self, station, year, month, day):
        """
        Returns an index page containing a list of json files available for
        the day.
        :param station: Station to get data for
        :type station: string
        :param year: Year to get data for
        :type year: string
        :param month: Month to get data for
        :type month: string
        :param day: Day to get data for.
        :type day: string
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        # Make sure the day actually exists in the database before we go
        # any further.
        if not day_exists(date(int(year),int(month),int(day))):
            raise web.NotFound()

        web.header('Content-Type', 'text/html')
        return render.daily_data_index(data_sources=datasources,
                                       dt_datasources=datatable_datasources)
