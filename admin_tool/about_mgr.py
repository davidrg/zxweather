# coding=utf-8
"""
Manages upgrading about.html files.
"""
from ui import get_string
from station_mgr import get_station_codes
import os

__author__ = 'david'

template_start = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <title>About</title>
    <link rel="stylesheet" href="/css/bootstrap.css" type="text/css"/>
    <style>
        body {
            padding-top: 60px; /* Account for the navbar */
        }
    </style>
    <link rel="stylesheet" href="/css/bootstrap-responsive.css" type="text.css" />
</head>
<!-- TEMPLATE_V2 -->
<body>
    <div class="navbar navbar-fixed-top">
        <div class="navbar-inner">
            <div class="container">
                <a class="btn btn-navbar" data-toggle="collapse" data-target=".nav-collapse">
                    <span class="icon-bar"></span>
                    <span class="icon-bar"></span>
                    <span class="icon-bar"></span>
                </a>
                <a id="nav_brand" class="brand" href="#"></a>
                <div class="nav-collapse">
                    <ul class="nav">
                        <li><a href="index.html">Now</a></li>
                        <li><a id="nav_lnk_yesterday" href="">Yesterday</a></li>
                        <li><a id="nav_lnk_this_month" href="">This Month</a></li>
                        <li><a id="nav_lnk_this_year" href="">This Year</a></li>
                        <li class="active"><a href="about.html">About</a></li>
                    </ul>
                </div><!--/.nav-collapse -->
            </div>
        </div>
    </div>

    <div class="container">
    <!-- BEGIN_USER_CONTENT -->
"""

template_end = """    <!-- END_USER_CONTENT -->
    </div>
    <script type="text/javascript" src="/js/jquery.min.js"></script>
    <script type="text/javascript">
        /* As this isn't a template we have to ask the server for a few
         * details (links and site name). */
        $.getJSON('about.json', function(data) {
            $("#nav_brand").html(data['site_name']);
            $(document).attr("title","About - " + data['site_name']);

            if (data['archive_mode']) {
                var nav = $(".nav");
                nav.empty();
                nav.append('<li><a href="index.html">Home</a></li>');
                nav.append('<li class="active"><a href="about.html">About</a></li>');
            } else {
                $("#nav_lnk_yesterday").attr('href',data['yesterday']);
                $("#nav_lnk_this_month").attr('href',data['this_month']);
                $("#nav_lnk_this_year").attr('href',data['this_year']);
            }
        });

    </script>
</body>
</html>"""

def upgrade_file(filename):
    """
    Upgrades the specified about.html file
    :param filename: Name of the file to upgrade
    """

    f = open(filename, "r")

    user_data = ""

    in_user_area = False

    for line in f:
        if line.strip() == "<!-- BEGIN_USER_CONTENT -->":
            in_user_area = True

        elif line.strip() == "<!-- END_USER_CONTENT -->":
            break
        elif in_user_area:
            user_data += line

    f.close()

    new_file = template_start + user_data + template_end

    f = open(filename, "w")
    f.write(new_file)
    f.close()

def upgrade_about(cur):
    """
    Attempts to upgrade about pages automatically.
    :param cur: A database cursor
    """

    static_dir = None


    import ConfigParser
    config = ConfigParser.ConfigParser()
    config.read(['config.cfg','../zxw_web/config.cfg', 'zxw_web/config.cfg', '/etc/zxweather.cfg'])

    try:
        static_dir = config.get("site", "static_data_dir")
    except ConfigParser.NoSectionError:
        pass

    print("""
Upgrade about.html
------------------

This procedure will attempt to upgrade any customised about.html files for
the web interface.

To proceed you must enter the full path for the web interfaces static data
directory.
    """)

    if static_dir is None:
        static_dir = get_string("Static data directory", required=True)
    else:
        static_dir = get_string("Static data directory", static_dir)

    print("Searching for station directories...")

    for code in get_station_codes(cur):
        about_file = os.path.join(static_dir, code, "about.html")
        if os.path.exists(about_file):
            print("Found about.html for {code}".format(code=code))
            upgrade_file(about_file)

    print("done.")