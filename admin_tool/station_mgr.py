# coding=utf-8
"""
Menus and routines for managing weather stations in the database.
"""
from ui import pause, get_string_with_length_indicator, get_string, get_code, \
    get_boolean, menu, get_number

__author__ = 'david'

def print_station_list(cur):
    """
    Prints out a list of all weather stations
    :param cur: Database cursor
    :returns: A list of valid station codes
    :rtype: list
    """
    cur.execute("""
select s.code, st.code as type_code, s.title
from station s
inner join station_type st on st.station_type_id = s.station_type_id
    """)

    results = cur.fetchall()

    print("Code  Type     Title")
    print("----- -------- -----------------------------------------------"
          "---------------")

    station_codes = []

    for result in results:
        station_codes.append(result[0])
        print("{0:<5} {1:<8} {2}".format(result[0],result[1],result[2]))

    print("----- -------- -----------------------------------------------"
          "---------------")

    return station_codes

def get_station_codes(cur):
    """
    Returns a list of station codes.
    :param cur: Database cursor
    :return: List of station codes
    :rtype: list
    """

    cur.execute("""
select s.code
from station s
    """)

    results = cur.fetchall()

    return [result[0] for result in results]

def list_stations(cur):
    """
    Displays a list of all weather stations in the database.
    :param cur: Database cursor
    """
    print("\nList stations\n-------------\nThe following weather stations are "
          "registered in the database.\n")

    print_station_list(cur)

    pause()

def get_new_station_info(cur, defaults):
    """
    Gets the details for a new weather station
    :param cur: Database cursor
    :param defaults: Default values
    :type defaults: dict
    :return:
    """

    print("""
Station Code is the short (up to 5 characters) name for your weather station
that appears in all URLs. For example, if your station was called 'foo' your
URLs would look something like http://example.com/s/foo/. The value you
enter here *must* match the value currently in your zxweather configuration
file (called station_name under the [site] section). This value can not be
changed later without breaking any links within your site.""")

    if defaults["code"] is None:
        required = True
        default = None
    else:
        required = False
        default = defaults["code"]

    station_code = get_string_with_length_indicator(
        "Station Code", default, 5, required)

    print("""
Station Name is a longer name for your station that will be displayed to
users.""")

    if defaults["name"] is None:
        required = True
        default = None
    else:
        required = False
        default = defaults["name"]

    station_name = get_string("Name", default, required)

    print("""
You may now enter an optional short description for your weather station.""")
    station_description = get_string("Description", defaults["description"])

    # Users should never select the 'GENERIC' hardware type unless they know
    # exactly what they're getting themselves into. For that reason it is
    # excluded from this list - they can only learn about its existence from
    # the documentation which explains it.
    cur.execute("select code,title from station_type where code <> 'GENERIC'")
    results = cur.fetchall()

    print("""
You must now select your weather station hardware type from the list below. If
your weather station is not in the list below then it is not directly supported
by zxweather at this time.

Code     Title
-------- ---------------------------------------------------------------------""")

    codes = ['GENERIC']

    for result in results:
        print("{0:<8} {1}".format(result[0],result[1]))
        codes.append(result[0].upper())
    print("-------- ------------------------------------------------------"
          "---------------")

    if defaults["type"] is None:
        required = True
        default = None
    else:
        required = False
        default = defaults["type"]

    hardware_code = get_code("Hardware type code", codes, default, required)

    print("""
The stations sample interval is how often new samples are loaded into the
database. Samples are used to plot charts and are also used for the current
conditions when live data is not available. The sample interval is in seconds
where 300 is 5 minutes.""")
    sample_interval = get_number("Sample interval", defaults["interval"])

    live_data = get_boolean("Is live data available for this station", defaults["live"])

    return {
        "code": station_code,
        "name": station_name,
        "description": station_description,
        "type": hardware_code,
        "interval": sample_interval,
        "live": live_data
    }

def create_station(con):
    """
    Allows the user to create a new weather station.
    :param con: Database connection
    """
    cur = con.cursor()

    print("""
Create Station
--------------
This procedure allows you to create a new weather station. Note that this tool
does not allow stations to be removed or station codes or hardware types to be
changed later so ensure your choices are correct! Consult the documentation if
you are uncertain about anything.

You will now be prompted for some information about your weather station:
  - Station Code
  - Station Name
  - Description
  - Hardware Type
  - The stations sample interval
  - If live data is available from the station
Once you have answered the questions you will be given the opportunity to
review your responses and either edit them, create the station or cancel.""")

    station_info = {
        "code": None,
        "name": None,
        "description": "",
        "type": None,
        "interval": 300,
        "live": True
    }

    while True:
        station_info = get_new_station_info(cur, station_info)
        print("""
You entered the following details:
----------------------------------
Code: {code}
Name: {name}
Sample interval: {interval}
Live data available: {live}
Hardware type: {type}
Description:
{description}
----------------------------------
""".format(**station_info))

        print("If you no longer wish to create this station answer no to "
              "the following two\nquestions.\n")

        if get_boolean("Is the information entered correct? (y/n)", required=True):
            print("Creating station...")

            cur.execute("select station_type_id from station_type "
                        "where code = %s", (station_info["type"],))
            type_id = cur.fetchone()[0]

            cur.execute("""
insert into station(code, title, description, station_type_id,
                    sample_interval, live_data_available)
values(%s,%s,%s,%s,%s,%s)""", (
                station_info["code"], station_info["name"],
                station_info["description"], type_id, station_info["interval"],
                station_info["live"]))
            con.commit()
            cur.close()
            print("Station created.")
            return
        elif not get_boolean("Do you wish to correct it? (Answering no will "
                             "cancel)", True):
            return

    return

def get_updated_station_info(defaults):
    """
    Gets the details for a new weather station
    :param defaults: Default values
    :type defaults: dict
    :return:
    """

    station_name = get_string("Name", defaults["name"])
    station_description = get_string("Description", defaults["description"])
    sample_interval = get_number("Sample interval", defaults["interval"])
    live_data = get_boolean("Is live data available for this station", defaults["live"])

    return {
        "id": defaults["id"],
        "code": defaults["code"],
        "name": station_name,
        "description": station_description,
        "interval": sample_interval,
        "live": live_data
    }

def edit_station(con):
    """
    Allows the user to edit an existing station
    :param con:
    """
    print("\n\nEdit station")
    print("------------\n\nThe following stations are available:")
    cur = con.cursor()
    codes = print_station_list(cur)

    selected_station_code = get_code("Station to edit", codes, required=True)

    cur.execute("""
select station_id, title, description, sample_interval, live_data_available
from station where code = %s
    """, (selected_station_code,))
    result = cur.fetchone()

    station_info = {
        "id": result[0],
        "code": selected_station_code,
        "name": result[1],
        "description": result[2],
        "interval": result[3],
        "live": result[4]
    }
    while True:
        station_info = get_updated_station_info(station_info)
        print("""
You entered the following details:
----------------------------------
Name: {name}
Sample interval: {interval}
Live data available: {live}
Description:
{description}
----------------------------------
""".format(**station_info))

        if get_boolean("Is the information entered correct? (y/n)", required=True):
            print("Updating station...")

            cur.execute("""
        update station set title=%s, description=%s, sample_interval=%s,
                           live_data_available=%s
        where station_id = %s""", (
                station_info["name"],
                station_info["description"], station_info["interval"],
                station_info["live"], station_info["id"]))
            con.commit()
            cur.close()
            print("Station updated.")
            return
        elif not get_boolean("Do you wish to correct it?", True):
            return


def manage_stations(con):
    """
    Runs a menu allowing the user to select various station management options.
    :param con: Database connection
    """

    choices = [
        {
            "key": "1",
            "name": "List stations",
            "type": "func",
            "func": lambda : list_stations(con.cursor())
        },
        {
            "key": "2",
            "name": "Create station",
            "type": "func",
            "func": lambda : create_station(con)
        },
        {
            "key": "3",
            "name": "Edit station",
            "type": "func",
            "func": lambda : edit_station(con)
        },
        {
            "key": "0",
            "name": "Return",
            "type": "return"
        }
    ]

    menu(choices, prompt="\n\nManage Stations\n---------------\n\nSelect option")


