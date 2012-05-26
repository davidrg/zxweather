"""
Handles pages at the year level.
"""
from web.contrib.template import render_jinja
from ui.baseui import BaseUI
from cache import year_cache_control
from data.database import year_exists
from ui.modern_ui import ModernUI
import os
from ui_route import validate_request, html_file

__author__ = 'David Goodwin'


basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')

def get_year(ui,station, year):
    """
    Gives an overview for a year.
    :param ui: UI to use
    :type ui: str
    :param station: Station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param year: Page year
    :type year: integer
    :return: View data
    """
    current_location = '/s/' + station + '/' + str(year) + '/'

    class data:
        """ Data required by the view """
        this_year = year
        prev_url = None
        next_url = None

        year_stamp = year

        next_year = year + 1
        prev_year = year - 1

        # A list of months in the year that have data
        months = [(item,item.capitalize()) for item in BaseUI.get_year_months(year)]

        # Min/max values for the year.
        records = BaseUI.get_yearly_records(year)

    # See if any data exists for the previous and next months (no point
    # showing a navigation link if there is no data)
    if year_exists(data.prev_year):
        data.prev_url = '../' + str(data.prev_year)

    if year_exists(data.next_year):
        data.next_url = '../' + str(data.next_year)

    year_cache_control(year)

    if ui == 's':
        # Figure out any URLs the page needs to know.
        class urls:
            """Various URLs required by the view"""
            root = '../../../'
            ui_root = '../../'
            data_base = root + 'data/{0}/{1}/'.format(station,year)
            daily_records = data_base + 'datatable/daily_records.json'
            prev_url = data.prev_url
            next_url = data.next_url

        nav_urls = ModernUI.get_nav_urls(station, current_location)
        return modern_templates.year(nav=nav_urls,data=data,urls=urls)
    else:
        return basic_templates.year(data=data)

class year:
    """
    Gives an overview for a year
    """
    def GET(self, ui, station, year):
        """
        Fetches the year page (index.html) after doing some basic validation.
        :param ui: UI to fetch the page for
        :type ui: str
        :param station: Station to fetch data for
        :type station: str
        :param year: Year page to get
        :type year: str
        :return: HTML data for the appropriate year, station and ui
        :rtype: str
        """
        validate_request(ui,station,year)
        html_file()
        return get_year(ui, station, int(year))

