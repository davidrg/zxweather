"""
Base class for UIs
"""

import os
from web.contrib.template import render_jinja

__author__ = 'David Goodwin'

class BaseUI:
    """
    Base class for user interfaces.
    """
    def __init__(self, template_dir):
        """
        Initialise the base UI functionality.
        :param template_dir: The directory to look for templates in.
        :type template_dir: string
        """

        self.template_dir = os.path.join(os.path.dirname(__file__),
                                    os.path.join(template_dir))
        self.render = render_jinja(self.template_dir, encoding='utf-8')

