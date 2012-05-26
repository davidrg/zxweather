"""
Controllers for month pages.
"""
from web.contrib.template import render_jinja
from months import month_name, month_number
from ui.baseui import BaseUI
from cache import month_cache_control
from database import month_exists
from ui.modern_ui import ModernUI
import os
from ui.ui_route import validate_request, html_file
from url_util import relative_url

__author__ = 'David Goodwin'

basic_template_dir = os.path.join(os.path.dirname(__file__),
                                  os.path.join('basic_templates'))
modern_template_dir = os.path.join(os.path.dirname(__file__),
                                   os.path.join('modern_templates'))

basic_templates = render_jinja(basic_template_dir, encoding='utf-8')
modern_templates = render_jinja(modern_template_dir, encoding='utf-8')


def get_previous_month(year,month):
    """
    Calculates the year and month that comes before the year and month
    parameters. The returned "previous year" will be year-1 if month==1,
    otherwise it will equal the year parameter.

    :param year: Year
    :type year: int
    :param month: Month
    :type month: int
    :return: Previous year, Previous month
    :rtype: int,int
    """
    previous_month = month - 1
    previous_year = year

    if not previous_month:
        previous_month = 12
        previous_year -= 1

    return previous_year,previous_month

def get_next_month(year,month):
    """
    Calculates the year and month that comes after the year and month
    parameters. The returned "next year" will be year+1 if month==12,
    otherwise it will equal the year parameter.

    :param year: Year
    :type year: int
    :param month: Month
    :type month: int
    :return: Next year, Next month
    :rtype: int,int
    """

    next_month = month + 1
    next_year = year

    if next_month > 12:
        next_month = 1
        next_year += 1

    return next_year, next_month

def month_nav_urls(ui,station,year,month, current_location):
    """
    Gets navigation URLs for the month page.
    :param ui: UI the page lives under.
    :type ui: str
    :param station: Station the page lives under.
    :type station: str or unicode
    :param year: Year that is being viewed.
    :type year: int
    :param month: Month that is being viewed.
    :type month: int
    :param current_location: Absolute URL for the current location.
    :type current_location: str
    :return: previous month url, next month url
    :rtype: str,str
    """

    previous_year, previous_month = get_previous_month(year,month)
    next_year, next_month = get_next_month(year,month)

    prev_url = ""
    next_url = ""

    url_format_string = '/{0}/{1}/{2}/{3}/'

    if month_exists(previous_year, previous_month):
        abs_url = url_format_string.format(ui,
                                           station,
                                           previous_year,
                                           month_name[previous_month])
        prev_url = relative_url(current_location, abs_url)

    if month_exists(next_year, next_month):
        abs_url = url_format_string.format(ui,
                                           station,
                                           next_year,
                                           month_name[next_month])
        next_url = relative_url(current_location, abs_url)

    return prev_url, next_url

def get_month(ui, station, year, month):
    """
    Gives an overview for a month.
    :param ui: UI to fetch
    :type ui: str
    :param station: Station to get data for.  Only used for building
    URLs at the moment
    :type station: string
    :param year: Page year
    :type year: integer
    :param month: Page month
    :type month: integer
    :return: View data
    """
    current_location = '/s/' + station + '/' + str(year) + '/' +\
                       month_name[month] + '/'

    previous_year, previous_month = get_previous_month(year,month)
    the_next_year, the_next_month = get_next_month(year,month)

    # TODO: Make this a dict sometime.
    class data:
        """ Data required by the view """
        year_stamp = year
        month_stamp = month
        current_data = None
        this_year = year

        prev_url, next_url = month_nav_urls(ui,
                                            station,
                                            year,
                                            month,
                                            current_location)

        next_month = month_name[the_next_month].capitalize()
        next_year = the_next_year

        prev_month = month_name[previous_month].capitalize()
        prev_year = previous_year

        this_month = month_name[month].capitalize()

        # List of days in the month that have data
        days = BaseUI.get_month_days(year,month)

        # Min/Max values for the month
        records = BaseUI.get_monthly_records(year,month)

    month_cache_control(year, month)
    if ui == 's':
        # TODO: Make this a dict.
        class urls:
            """Various URLs required by the view"""
            root = '../../../../'
            data_base = root + 'data/{0}/{1}/{2}/'.format(station,year,month)
            samples = data_base + 'datatable/samples.json'
            samples_30m_avg = data_base + 'datatable/30m_avg_samples.json'
            daily_records = data_base + 'datatable/daily_records.json'

        nav_urls = ModernUI.get_nav_urls(station, current_location)
        return modern_templates.month(nav=nav_urls, data=data,dataurls=urls)
    else:
        return basic_templates.month(data=data)


class month:
    """
    Gives an overview for a month
    """
    def GET(self, ui, station, year, month):
        """
        Gets the month page (index.html) after doing a bit of validation.
        :param ui: UI variant to get.
        :type ui: str
        :param station: Station to fetch data for.
        :type station: str
        :param year: Year to fetch
        :type year: str
        :param month: Month name (in lowercase) to fetch.
        :type month: str
        :return: HTML data for the page.
        :rtype: str
        """
        validate_request(ui,station,year,month)
        html_file()
        return get_month(ui, station, int(year), month_number[month])
