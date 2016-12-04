# coding=utf-8
"""
Index page for day level data sources.
"""
import json
import mimetypes
from datetime import date
import os
import web
from web.contrib.template import render_jinja

from cache import rfcformat
from data.daily import datasources, datatable_datasources
from database import day_exists, get_station_id, get_image_sources_for_station, \
    get_image_source, get_day_images_for_source, \
    get_most_recent_image_ts_for_source_and_date

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


class images_json:
    """
    Provides a list of images available for a particular day and image source
    """

    def HEAD(self, station, year, month, day, source):
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

        ts = get_most_recent_image_ts_for_source_and_date(
            source.image_source_id, dt)

        web.header('Content-Type', 'application/json')
        web.header('Last-Modified', rfcformat(ts))
        return

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

        image_set = []
        extensions = dict()
        latest_ts = None

        for image in image_list:
            if latest_ts is None or image.time_stamp > latest_ts:
                latest_ts = image.time_stamp

            mime = image.mime_type

            if mime not in extensions.keys():
                ext = mimetypes.guess_extension(mime, False)
                if ext == ".jpe":
                    ext = ".jpeg"
                extensions[mime] = ext

            url_prefix = image.time_stamp.time().strftime(
                    "%H_%M_%S") + '/' + image.type_code.lower()

            thumb_url = None
            if image.mime_type.startswith("image/"):
                thumb_url = url_prefix + '_thumbnail' + \
                            extensions[image.mime_type]

            metadata_url = None
            if image.has_metadata:
                metadata_url = url_prefix + '_metadata.json'

            img = {
                "id": image.id,
                "time_stamp": image.time_stamp.isoformat(),
                "mime_type": image.mime_type,
                "type_name": image.type_name,
                "type_code": image.type_code,
                "title": image.title,
                "description": image.description,
                "has_metadata": image.has_metadata,
                "image_url": url_prefix + '_full' + extensions[image.mime_type],
                "thumb_url": thumb_url,
                "metadata_url": metadata_url
            }

            image_set.append(img)

        result = json.dumps(image_set)

        web.header('Content-Type', 'application/json')
        web.header('Content-Length', str(len(result)))
        web.header('Last-Modified', rfcformat(latest_ts))
        return result
