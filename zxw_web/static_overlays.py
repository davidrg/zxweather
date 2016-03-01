# coding=utf-8
"""
This file handles the static overlay routes. These serve static files up out
of the /static directory. Any requests that don't match some other route
eventually fall through to here. If that request happens to match a file on
disk it is served up, otherwise a 404 error.
"""
from datetime import datetime
from mimetypes import guess_type
from wsgiref.handlers import format_date_time
from time import mktime
import web
import config
import os
from database import get_station_id

__author__ = 'David Goodwin'

class overlay_file:
    """ Handles static overlay files. """

    @staticmethod
    def check_about_html_file(path_name, file):
        """
        If the file being requested is /station-name/about.html but that
        file doesn't exist then /about.html is served up instead. This is to
        handle cases where the user hasn't copied the file into the station
        static data directory.

        :param path_name: computed local pathname for the file
        :param file: File being requested
        :return: Potentially updated local pathname.
        """

        # 'about.html', when in the site root or station root, is special. It
        # is an HTML file intended to be modified by the user to describe
        # their weather station. This chunk of code is to handle cases where
        # the user has failed to copy the file into the station static data
        # directory.

        parts = file.split('/')
        if len(parts) == 2 and parts[1] == 'about.html':
            # Looks like a station-level about page. Check that its a valid
            # station code.
            station_code = parts[0]
            if get_station_id(station_code) is not None:
                # Its a valid station-level about page URL. Redirect it if it
                # doesn't exist.
                if not os.path.exists(path_name):
                    new_path_name = config.static_data_dir + 'about.html'
                    print("WARNING: Station-level about file redirected to "
                          "site-level. To suppress this warning, copy {0} to "
                          "{1}".format(new_path_name, path_name))
                    return new_path_name

        return path_name

    def GET(self, file):
        """
        Gets the specified static file.
        :param file: Name of the file to get headers for
        :type file: string
        """

        if file.startswith(".."):
            raise web.Forbidden()

        # TODO: Make this less stupid: Use os.path.join, switch path separators
        # in file to platform native, collapse any relative bits, ensure the
        # result is under the static data dir and *then* serve the file up.

        path_name = config.static_data_dir + file

        path_name = overlay_file.check_about_html_file(path_name, file)

        if os.path.isdir(path_name):
            raise web.NotFound()

        return overlay_file.get_file(path_name)

    def HEAD(self, file):
        """
        Gets headers for the specified file
        :param file: Name of the file to get headers for
        :type file: string
        """

        if file.startswith(".."):
            raise web.Forbidden()
        path_name = config.static_data_dir + file

        path_name = overlay_file.check_about_html_file(path_name, file)

        if os.path.isdir(path_name):
            raise web.NotFound()

        return overlay_file.head_file(path_name)

    @staticmethod
    def file_headers(filename):
        """
        Sets headers for a static file. If the static file does not exist,
        a 404 error is raised.
        :param filename: File to set headers for
        :type filename: string
        :raise: web.NotFound if the file does not exist.
        """
        if not os.path.exists(filename):
            print "static file {0} not found".format(filename)
            raise web.NotFound()

        #filename = os.path.basename(full_filename)

        # Handle a few extensions specially. Otherwise, for example, python claims
        # png files are "image/x-png" which causes chrome to download the image
        # rather than display it.
        if filename.endswith(".png"):
            content_type = "image/png"
        elif filename.endswith(".dat"):
            content_type = "text/plain"
        else:
            content_type = guess_type(filename)[0]

        web.header("Content-Type", content_type)
        web.header("Content-Length", str(os.path.getsize(filename)))

        timestamp = datetime.fromtimestamp(os.path.getmtime(filename))
        web.header("Last-Modified",
                   format_date_time(mktime(timestamp.timetuple())))

    @staticmethod
    def get_file(pathname):
        """
        Streams a file back to the client.

        :param pathname: Local file to stream
        :return:
        """
        print "GET: " + pathname
        overlay_file.file_headers(pathname)

        f = open(pathname, 'rb')
        while 1:
            buf = f.read(1024 * 8)
            if not buf:
                break
            yield buf

    @staticmethod
    def head_file(pathname):
        """
        For handling HEAD requests for static files.
        :param pathname: File to get headers for
        :type pathname: string
        :return: Nothing.
        """
        overlay_file.file_headers(pathname)

        return ""
