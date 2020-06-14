# coding=utf-8
"""
Menus and routines for managing weather stations in the database.
"""
import json
from ui import pause, get_string_with_length_indicator, get_string, get_code, \
    get_boolean, menu, get_number, get_float

__author__ = 'david'


def print_station_list(cur):
    """
    Prints out a list of all weather stations
    :param cur: Database cursor
    :returns: A list of valid station codes
    :rtype: list
    """
    cur.execute("""
select lower(s.code) as code, lower(st.code) as type_code, s.title
from station s
inner join station_type st on st.station_type_id = s.station_type_id
order by sort_order ASC
    """)

    results = cur.fetchall()

    print("Code  Type     Title")
    print("----- -------- -----------------------------------------------"
          "---------------")

    station_codes = []

    for result in results:
        station_codes.append(result[0])
        print("{0:<5} {1:<8} {2}".format(result[0], result[1], result[2]))

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
select lower(s.code)
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
Station Coordinates:
Altitude (in meters) is required as its used for relative barometric pressure
calculation. Latitude and Longitude are optional and are used for sunrise
calculation, etc.
""")
    latitude = get_float("Latitude", defaults["latitude"])
    longitude = get_float("Longitude", defaults["longitude"])
    altitude = get_float("Altitude", defaults["altitude"],
                         defaults["altitude"] is None)

    print("\nThe Website Title is displayed at the top of all pages for this "
          "weather station in the web UI. If not set then the default value "
          "from the configuration file will be used.")
    site_title = get_string("Website Title", defaults["site_title"], False)

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
-------- ---------------------------------------------------------------------
""")

    codes = ['GENERIC']

    for result in results:
        print("{0:<8} {1}".format(result[0], result[1]))
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

    live_data = get_boolean("Is live data available for this station",
                            defaults["live"])

    print("""The List Order setting controls where the weather station should
appear in a list of multiple weather stations. If there will only be one
weather station in this database then you can leave this value as 0.""")
    sort_order = get_number("List order", defaults["sort_order"])

    davis_settings = None
    if hardware_code == "DAVIS":
        # Prompt for extra configuration data used by davis weather stations

        print("""
Choose which Davis weather station you are using from the list below. If you
are using a wireless Vantage Pro2 or Vantage Pro2 Plus ISS with a Vantage Vue
console then choose Vantage Pro2 or Vantage Pro2 Plus.

Code     Title
-------- ---------------------------------------------------------------------
PRO2     Vantage Pro2
PRO2C    Vantage Pro2 (non-wireless/cabled version)
PRO2P    Vantage Pro2 Plus (includes UV and Solar Radiation sensors)
PRO2CP   Vantage Pro2 Plus (non-wireless/cabled with UV and Solar sensors)
VUE      Vantage Vue
-------- ---------------------------------------------------------------------
""")

        station = get_code(
            "Hardware type code",
            ['PRO2', 'PRO2C', 'PRO2P', 'PRO2CP', 'VUE'],
            defaults['davis_settings']['hardware_type'],
            required)

        is_wireless = True
        has_solar_and_uv = False

        if station in ['PRO2C', 'PRO2CP']:
            is_wireless = False
        if station in ['PRO2P', 'PRO2CP']:
            has_solar_and_uv = True

        station_id = None

        if is_wireless:
            station_id = get_number("Which ID is the station broadcasting on?",
                                    defaults['davis_settings']['broadcast_id'])

        sensor_config = dict()

        davis_settings = {
            "hardware_type": station,
            "is_wireless": is_wireless,
            "has_solar_and_uv": has_solar_and_uv,
            "broadcast_id": station_id,
            "sensor_config": sensor_config
        }

    return {
        "code": station_code,
        "name": station_name,
        "latitude": latitude,
        "longitude": longitude,
        "altitude": altitude,
        "description": station_description,
        "type": hardware_code,
        "interval": sample_interval,
        "live": live_data,
        "sort_order": sort_order,
        "davis_settings": davis_settings,
        "site_title": site_title
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
  - Coordinates (latitude and longitude in decimal degrees, altitude in meters)
  - Title for website
  - Description
  - Hardware Type
  - The stations sample interval
  - If live data is available from the station
Once you have answered the questions you will be given the opportunity to
review your responses and either edit them, create the station or cancel.""")

    station_info = {
        "code": None,
        "name": None,
        "latitude": None,
        "longitude": None,
        "altitude": None,
        "description": "",
        "type": None,
        "interval": 300,
        "live": True,
        "sort_order": 0,
        "davis_settings": {
            'hardware_type': 'VPRO2',
            'broadcast_id': 1
        },
        "site_title": None
    }

    while True:
        station_info = get_new_station_info(cur, station_info)
        print("""
You entered the following details:
----------------------------------
Code: {code}
Name: {name}
Latitude: {latitude}
Longitude: {longitude}
Altitude: {altitude}
Website Title: {site_title}
Sample interval: {interval}
Live data available: {live}
Sort order: {sort_order}
Hardware type: {type}
""".format(**station_info))

        if station_info['type'] == "DAVIS":
            davis_info = station_info['davis_settings']
            print("""Weather station model: {hardware_type}
Station ID: {broadcast_id}
""".format(**davis_info))

        print("""Description:
{description}
----------------------------------
""".format(**station_info))

        print("If you no longer wish to create this station answer no to "
              "the following two\nquestions.\n")

        if get_boolean("Is the information entered correct? (y/n)",
                       required=True):
            print("Creating station...")

            cur.execute("select station_type_id from station_type "
                        "where lower(code) = lower(%s)", (station_info["type"],))
            type_id = cur.fetchone()[0]

            station_config = None

            if station_info['type'] == "DAVIS":
                station_config = json.dumps(station_info['davis_settings'])

            cur.execute("""
insert into station(code, title, description, station_type_id,
                    sample_interval, live_data_available, sort_order,
                    station_config, site_title, latitude, longitude, altitude)
values(upper(%s),%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
returning station_id""", (
                station_info["code"], station_info["name"],
                station_info["description"], type_id, station_info["interval"],
                station_info["live"], station_info["sort_order"],
                station_config, station_info["site_title"],
                station_info["latitude"], station_info["longitude"],
                station_info["altitude"]))
            result = cur.fetchone()
            station_id = result[0]

            cur.execute("insert into live_data(station_id) values(%s)",
                        (station_id, ))

            # Davis hardware has an extra live data record.
            if station_info['type'] == 'DAVIS':
                cur.execute(
                    "insert into davis_live_data(station_id) values(%s)",
                    (station_id,))

            con.commit()
            cur.close()
            print("Station created.")
            return
        elif not get_boolean("Do you wish to correct it? (Answering no will "
                             "cancel)", True):
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
    site_title = get_string("Website Title", defaults["site_title"])
    latitude = get_float("Latitude", defaults["latitude"])
    longitude = get_float("Longitude", defaults["longitude"])
    altitude = get_float("Altitude", defaults["altitude"], defaults["altitude"] is None)
    sample_interval = get_number("Sample interval", defaults["interval"])
    live_data = get_boolean("Is live data available for this station",
                            defaults["live"])
    sort_order = get_number("List order", defaults["sort_order"])

    davis_settings = defaults["davis_settings"]
    if defaults['hw_type'] == "DAVIS":
        # Prompt for extra configuration data used by davis weather stations

        print("""
    Choose which Davis weather station you are using form the list below. If you
    are using a wireless Vantage Pro2 or Vantage Pro2 Plus ISS with a Vantage Vue
    console then choose Vantage Pro2 or Vantage Pro2 Plus.

    Code     Title
    -------- ---------------------------------------------------------------------
    PRO2     Vantage Pro2
    PRO2C    Vantage Pro2 (non-wireless/cabled version)
    PRO2P    Vantage Pro2 Plus (includes UV and Solar Radiation sensors)
    PRO2CP   Vantage Pro2 Plus (non-wireless/cabled with UV and Solar sensors)
    VUE      Vantage Vue
    -------- ---------------------------------------------------------------------
    """)

        station = get_code(
            "Hardware type code",
            ['PRO2', 'PRO2C', 'PRO2P', 'PRO2CP', 'VUE'],
            defaults['davis_settings']['hardware_type'],
            False)

        is_wireless = True
        has_solar_and_uv = False

        if station in ['PRO2C', 'PRO2CP']:
            is_wireless = False
        if station in ['PRO2P', 'PRO2CP']:
            has_solar_and_uv = True

        station_id = None

        if is_wireless:
            station_id = get_number("Which ID is the station broadcasting on?",
                                    defaults['davis_settings']['broadcast_id'])

        davis_settings = {
            "hardware_type": station,
            "is_wireless": is_wireless,
            "has_solar_and_uv": has_solar_and_uv,
            "broadcast_id": station_id
        }

    return {
        "id": defaults["id"],
        "code": defaults["code"],
        "name": station_name,
        "latitude": latitude,
        "longitude": longitude,
        "altitude": altitude,
        "description": station_description,
        "interval": sample_interval,
        "live": live_data,
        "sort_order": sort_order,
        "hw_type": defaults["hw_type"],
        "davis_settings": davis_settings,
        "site_title": site_title
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
select stn.station_id,
       stn.title,
       stn.description,
       stn.sample_interval,
       stn.live_data_available,
       coalesce(stn.sort_order,0),
       upper(ht.code) as hardware_type,
       station_config,
       site_title,
       latitude,
       longitude,
       altitude
from station stn
inner join station_type ht on ht.station_type_id = stn.station_type_id
where lower(stn.code) = lower(%s)
    """, (selected_station_code,))
    result = cur.fetchone()

    station_info = {
        "id": result[0],
        "code": selected_station_code,
        "name": result[1],
        "description": result[2],
        "interval": result[3],
        "live": result[4],
        "sort_order": result[5],
        "hw_type": result[6],
        "davis_settings": None,
        "site_title": result[8],
        "latitude": result[9],
        "longitude": result[10],
        "altitude": result[11]
    }

    hw_config = result[7]
    if station_info["hw_type"] == "DAVIS":
        if hw_config is None:
            davis_settings = {
                "hardware_type": "PRO2",
                "broadcast_id": 1
            }
        else:
            davis_settings = json.loads(hw_config)

        station_info["davis_settings"] = davis_settings

    while True:
        station_info = get_updated_station_info(station_info)
        print("""
You entered the following details:
----------------------------------
Name: {name}
Latitude: {latitude}
Longitude: {longitude}
Altitude: {altitude}
Website Title: {site_title}
Sample interval: {interval}
Live data available: {live}
List order: {sort_order}""".format(**station_info))

        if station_info['hw_type'] == "DAVIS":
            davis_info = station_info['davis_settings']
            print("""Weather station model: {hardware_type}
Station ID: {broadcast_id}
""".format(**davis_info))


        print("""
Description:
{description}
----------------------------------
""".format(**station_info))

        if get_boolean("Is the information entered correct? (y/n)",
                       required=True):
            print("Updating station...")

            station_config = None
            if station_info["hw_type"] == "DAVIS":
                station_config = json.dumps(station_info["davis_settings"])

            cur.execute("""
        update station set title=%s, description=%s, sample_interval=%s,
                           live_data_available=%s, sort_order=%s,
                           station_config = %s, site_title = %s, latitude=%s,
                           longitude=%s, altitude=%s
        where station_id = %s""", (
                station_info["name"],
                station_info["description"], station_info["interval"],
                station_info["live"], station_info["sort_order"],
                station_config, station_info["site_title"],
                station_info["latitude"], station_info["longitude"],
                station_info["altitude"], station_info["id"]))
            con.commit()
            cur.close()
            print("Station updated.")
            return
        elif not get_boolean("Do you wish to correct it?", True):
            return


def set_station_message(con):
    """
    Allows the user to set a message for the station.
    :param con: Database connection
    """

    print("\n\Set station message")
    print("------------\n\nThe following stations are available:")
    cur = con.cursor()
    codes = print_station_list(cur)

    selected_station_code = get_code("Station to edit", codes, required=True)

    cur.execute("select station_id, message, message_timestamp "
                "from station where lower(code) = lower(%s)",
                (selected_station_code,))
    result = cur.fetchone()

    print('The current message was set on {0}: {1}\n'.format(
        result[2], result[1]))

    print("This message will appear on all pages in the web interface where "
          "data for this station appears. Press return to clear the current "
          "message.")

    message = get_string("New message", None)

    if message is None:
        cur.execute("update station set message = null, "
                    "message_timestamp = null where station_id = %s",
                    (result[0],))
    else:
        cur.execute("update station set message = %s, message_timestamp = NOW() "
                    "where station_id = %s",
                    (message, result[0]))
    con.commit()
    print("Station message updated.")


# Sorry about this function. Its late and I just want to get something working.
# This whole apps needs an overhaul sometime anyway.
def configure_sensors(con):
    print("\n\nConfigure sensors")
    print("------------\n\nThe following stations are available:")
    cur = con.cursor()
    codes = print_station_list(cur)

    selected_station_code = get_code("Station to edit", codes, required=True)

    cur.execute("""
    select upper(ht.code) as hardware_type,
           station_config
    from station stn
    inner join station_type ht on ht.station_type_id = stn.station_type_id
    where lower(stn.code) = lower(%s)
        """, (selected_station_code,))
    result = cur.fetchone()

    hw_type = result[0]
    hw_config = json.loads(result[1])
    sensor_config = None
    if "sensor_config" in hw_config and len(hw_config["sensor_config"].keys()) > 0:
        sensor_config = hw_config["sensor_config"]
    else:
        sensor_config = {
            "leaf_wetness_1": {
                "enabled": False,
                "name": "Leaf Wetness 1"
            },
            "leaf_wetness_2": {
                "enabled": False,
                "name": "Leaf Wetness 2"
            },
            "leaf_temperature_1": {
                "enabled": False,
                "name": "Leaf Temperature 1"
            },
            "leaf_temperature_2": {
                "enabled": False,
                "name": "Leaf Temperature 2"
            },
            "soil_moisture_1": {
                "enabled": False,
                "name": "Soil Moisture 1"
            },
            "soil_moisture_2": {
                "enabled": False,
                "name": "Soil Moisture 2"
            },
            "soil_moisture_3": {
                "enabled": False,
                "name": "Soil Moisture 3"
            },
            "soil_moisture_4": {
                "enabled": False,
                "name": "Soil Moisture 4"
            },
            "soil_temperature_1": {
                "enabled": False,
                "name": "Soil Temperature 1"
            },
            "soil_temperature_2": {
                "enabled": False,
                "name": "Soil Temperature 2"
            },
            "soil_temperature_3": {
                "enabled": False,
                "name": "Soil Temperature 3"
            },
            "soil_temperature_4": {
                "enabled": False,
                "name": "Soil Temperature 4"
            },
            "extra_humidity_1": {
                "enabled": False,
                "name": "Extra Humidity 1"
            },
            "extra_humidity_2": {
                "enabled": False,
                "name": "Extra Humidity 2"
            },
            "extra_temperature_1": {
                "enabled": False,
                "name": "Extra Temperature 1"
            },
            "extra_temperature_2": {
                "enabled": False,
                "name": "Extra Temperature 2"
            },
            "extra_temperature_3": {
                "enabled": False,
                "name": "Extra Temperature 3"
            },
        }

    if hw_type != "DAVIS":
        print("This station does not support additional sensors")
        return

    option_fmt = "{letter}) {name:<23} - {enabled:<8}"
    option_line = "{option_a:<37}    {option_b:<37}"

    def option(letter, key):
        name = sensor_config[key]["name"]
        enabled = "ENABLED" if sensor_config[key]["enabled"] else "DISABLED"
        return option_fmt.format(letter=letter, name=name, enabled=enabled)

    while True:
        print("""
Choose sensor to configure:
------------------------------------------------------------------------------""")
        print(option_line.format(
            option_a=option("A", "soil_moisture_1"),
            option_b=option("B", "soil_temperature_1")))
        print(option_line.format(
            option_a=option("C", "soil_moisture_2"),
            option_b=option("D", "soil_temperature_2")))
        print(option_line.format(
            option_a=option("E", "soil_moisture_3"),
            option_b=option("F", "soil_temperature_3")))
        print(option_line.format(
            option_a=option("G", "soil_moisture_4"),
            option_b=option("H", "soil_temperature_4")))
        print(option_line.format(
            option_a=option("I", "leaf_wetness_1"),
            option_b=option("J", "leaf_temperature_1")))
        print(option_line.format(
            option_a=option("K", "leaf_wetness_2"),
            option_b=option("L", "leaf_temperature_2")))
        print(option_line.format(
            option_a=option("M", "extra_humidity_1"),
            option_b=option("N", "extra_temperature_1")))
        print(option_line.format(
            option_a=option("O", "extra_humidity_2"),
            option_b=option("P", "extra_temperature_2")))
        print(option_line.format(
            option_a="",
            option_b=option("Q", "extra_temperature_3")))
        print("X) Cancel")
        print("Y) Save and Return")

        choice = get_string("Choice")
        if choice.upper() not in ["A", "B", "C", "D", "E", "F", "G", "H", "I",
                                  "J", "K", "L", "M", "N", "O", "P", "Q", "X",
                                  "Y"]:
            print("Invalid option")
            continue

        choice = choice.upper()

        if choice == "X":
            break
        elif choice == "Y":
            print("Saving configuration...")
            hw_config["sensor_config"] = sensor_config

            cur.execute("update station set station_config=%s where lower(code) = lower(%s)",
                        (json.dumps(hw_config), selected_station_code))
            con.commit()
            cur.close()
            break

        def configure_sensor(sensor):
            system_name = sensor
            if sensor == "soil_moisture_1":
                system_name = "Soil Moisture 1"
            elif sensor == "soil_moisture_2":
                system_name = "Soil Moisture 2"
            elif sensor == "soil_moisture_3":
                system_name = "Soil Moisture 3"
            elif sensor == "soil_moisture_4":
                system_name = "Soil Moisture 4"
            elif sensor == "soil_temperature_1":
                system_name = "Soil Temperature 1"
            elif sensor == "soil_temperature_2":
                system_name = "Soil Temperature 2"
            elif sensor == "soil_temperature_3":
                system_name = "Soil Temperature 3"
            elif sensor == "soil_temperature_4":
                system_name = "Soil Temperature 4"
            elif sensor == "leaf_wetness_1":
                system_name = "Leaf Wetness 1"
            elif sensor == "leaf_wetness_2":
                system_name = "Leaf Wetness 2"
            elif sensor == "leaf_temperature_1":
                system_name = "Leaf Temperature 1"
            elif sensor == "leaf_temperature_2":
                system_name = "Leaf Temperature 2"
            elif sensor == "extra_humidity_1":
                system_name = "Extra Humidity 1"
            elif sensor == "extra_humidity_2":
                system_name = "Extra Humidity 2"
            elif sensor == "extra_temperature_1":
                system_name = "Extra Temperature 1"
            elif sensor == "extra_temperature_2":
                system_name = "Extra Temperature 2"
            elif sensor == "extra_temperature_3":
                system_name = "Extra Temperature 3"

            def show_settings():
                print("\nModify Sensor: {0}\n----------------------------------"
                      "--------------------------------------------\n"
                      "Display Name: {1}\nEnabled: {2}".format(
                    system_name, sensor_config[sensor]["name"], sensor_config[sensor]["enabled"]))

            def toggle_enabled():
                sensor_config[sensor]["enabled"] = not sensor_config[sensor]["enabled"]
                show_settings()

            def set_display_name():
                sensor_config[sensor]["name"] = get_string("Display name", system_name)
                show_settings()
            while True:
                choices = [
                    {
                        "key": "1",
                        "name": "Disable sensor" if sensor_config[sensor]["enabled"] else "Enable sensor",
                        "type": "return",
                    },
                    {
                        "key": "2",
                        "name": "Set display name",
                        "type": "return",
                    },
                    {
                        "key": "0",
                        "name": "Return",
                        "type": "return"
                    }
                ]

                show_settings()
                choice = menu(choices)
                if choice == "1":
                    toggle_enabled()
                elif choice == "2":
                    set_display_name()
                elif choice == "0":
                    break

        if choice == "A":
            configure_sensor("soil_moisture_1")
        elif choice == "B":
            configure_sensor("soil_temperature_1")
        elif choice == "C":
            configure_sensor("soil_moisture_2")
        elif choice == "D":
            configure_sensor("soil_temperature_2")
        elif choice == "E":
            configure_sensor("soil_moisture_3")
        elif choice == "F":
            configure_sensor("soil_temperature_3")
        elif choice == "G":
            configure_sensor("soil_moisture_4")
        elif choice == "H":
            configure_sensor("soil_temperature_4")
        elif choice == "I":
            configure_sensor("leaf_wetness_1")
        elif choice == "J":
            configure_sensor("leaf_temperature_1")
        elif choice == "K":
            configure_sensor("leaf_wetness_2")
        elif choice == "L":
            configure_sensor("leaf_temperature_1")
        elif choice == "M":
            configure_sensor("extra_humidity_1")
        elif choice == "N":
            configure_sensor("extra_temperature_1")
        elif choice == "O":
            configure_sensor("extra_humidity_2")
        elif choice == "P":
            configure_sensor("extra_temperature_2")
        elif choice == "Q":
            configure_sensor("extra_temperature_3")


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
            "func": lambda: list_stations(con.cursor())
        },
        {
            "key": "2",
            "name": "Create station",
            "type": "func",
            "func": lambda: create_station(con)
        },
        {
            "key": "3",
            "name": "Edit station",
            "type": "func",
            "func": lambda: edit_station(con)
        },
        {
            "key": "4",
            "name": "Set station message",
            "type": "func",
            "func": lambda: set_station_message(con)
        },
        {
            "key": "5",
            "name": "Configure extra sensors",
            "type": "func",
            "func": lambda: configure_sensors(con)
        },
        {
            "key": "0",
            "name": "Return",
            "type": "return"
        }
    ]

    menu(choices, prompt="\n\nManage Stations\n---------------\n\n"
                         "Select option")