# coding=utf-8
"""
Index page for day level data sources.
"""
import mimetypes
from datetime import date
import os
import web
from web.contrib.template import render_jinja
from data.daily import datasources, datatable_datasources
from database import day_exists, get_station_id, get_image_sources_for_station, \
    get_image_source, get_day_images_for_source

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

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        # Make sure the day actually exists in the database before we go
        # any further.
        if not day_exists(date(int(year),int(month),int(day)), station_id):
            raise web.NotFound()

        # See if we have any images for this station
        sources = get_image_sources_for_station(station_id)

        web.header('Content-Type', 'text/html')
        return render.daily_data_index(data_sources=datasources,
                                       dt_datasources=datatable_datasources,
                                       image_sources=sources)


class images:
    """
    Provides an index of available daily data sources
    """
    def GET(self, station, year, month, day, source):
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
        :param source: Image source code
        :type source: str
        """
        template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join('templates'))
        render = render_jinja(template_dir, encoding='utf-8')

        dt = date(int(year),int(month),int(day))

        station_id = get_station_id(station)
        if station_id is None:
            raise web.NotFound()

        # Make sure the day actually exists in the database before we go
        # any further.
        if not day_exists(dt, station_id):
            raise web.NotFound()

        source = get_image_source(station_id, source)
        if source is None:
            raise web.NotFound()

        image_list = get_day_images_for_source(source.image_source_id, dt)

        extensions = dict()

        image_set = []

        for image in image_list:
            image_set.append(image)
            mime = image.mime_type

            if mime not in extensions.keys():
                ext = mimetypes.guess_extension(mime,False)
                if ext == ".jpe":
                    ext = ".jpeg"
                extensions[mime] = ext

        web.header('Content-Type', 'text/html')
        return render.image_index(
                source_name=source.source_name,
                date=dt,
                image_list=image_set,
                extensions=extensions)
