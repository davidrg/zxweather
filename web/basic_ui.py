"""
A very basic HTML3/4 UI using images for the charts rather than Javascript.
Should be compatible with just about anything.
"""

from baseui import BaseUI
from cache import year_cache_control
from data.database import year_exists, get_years

__author__ = 'David Goodwin'

class BasicUI(BaseUI):
    """
    Class for the basic UI - plain old HTML 4ish output.
    """

    def __init__(self):
        BaseUI.__init__(self, 'basic_templates')
        pass

    @staticmethod
    def ui_code():
        """
        :return: URL code for the UI.
        """
        return 'b'
    @staticmethod
    def ui_name():
        """
        :return: Name of the UI.
        """
        return 'Basic HTML'
    @staticmethod
    def ui_description():
        """
        :return: A description of the UI.
        """
        return 'Very basic HTML UI. No JavaScript or CSS.'


