"""
This package contains code for producing the user interface (HTML pages for
human consumption rather than JSON data for use by other systems)
"""

from datetime import timedelta, datetime
from months import month_name
from url_util import relative_url

__author__ = 'David Goodwin'

def get_nav_urls(station, current_url):
    """
    Gets a dict containing standard navigation URLs relative to the
    current location in the site.
    :param station: The station we are currently working with.
    :type station: str, unicode
    :param current_url: Where in the site we currently are
    :type current_url: str
    """

    now = datetime.now().date()
    yesterday = now - timedelta(1)

    home = '/s/' + station + '/'
    yesterday = home + str(yesterday.year) + '/' +\
                month_name[yesterday.month] + '/' +\
                str(yesterday.day) + '/'
    this_month = home + str(now.year) + '/' + month_name[now.month] + '/'
    this_year = home + str(now.year) + '/'
    about = home + 'about.html'

    urls = {
        'home': relative_url(current_url, home),
        'yesterday': relative_url(current_url,yesterday),
        'this_month': relative_url(current_url,this_month),
        'this_year': relative_url(current_url,this_year),
        'about': relative_url(current_url, about),
        }

    return urls
