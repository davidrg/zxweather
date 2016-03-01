from station_mgr import print_station_list
from ui import get_string_with_length_indicator, get_string, get_code, \
    get_boolean, menu


def get_edit_image_source_info(defaults):
    if defaults["name"] is None:
        required = True
        default = None
    else:
        required = False
        default = defaults["name"]

    name = get_string("Name", default, required)

    if defaults["description"] is not None:
        print("Description: " + defaults["description"])

        # A default is present. Ask the user if they'd like to accept the
        # default or enter a new value.
        if get_boolean("Would you like to enter a different description?",
                       default=False):
            description = get_string("Description")
        else:
            description = defaults["description"]
    else:
        # No default. Ask user for description
        description = get_string("Description")

    return {
        'name': name,
        'description': description
    }


def get_new_image_source_info(cur, defaults):
    print("""
The Image Source Code is a short (up to 5 characters) unique identifier for
your new image source. As it appears in URLs it can not be changed later.
    """)

    if defaults["code"] is None:
        required = True
        default = None
    else:
        required = False
        default = defaults["code"]

    code = get_string_with_length_indicator(
        "Image Source Code", default, 5, required)

    print("\nThe following stations are available:")
    codes = print_station_list(cur)

    print("Which station should this image source be assigned to?")
    station_code = get_code("Station Code", codes, required=True)

    cur.execute("select title, station_id from station "
                "where lower(code) = lower(%s)",
                (station_code,))
    result = cur.fetchone()

    station_name = result[0]
    station_id = result[1]

    edit_result = get_edit_image_source_info(defaults)

    return {
        "code": code,
        "station_code": station_code,
        "station_name": station_name,
        "station_id": station_id,
        "name": edit_result['name'],
        "description": edit_result['description']
    }


def create_image_source(con):

    print("""
Create Image Source
-------------------
This procedure allows you to create a new image source for an existing weather
station. An image source represents a particular image capture device. If you
have multiple cameras pointing in different directions you would have one for
each camera.

You will now be prompted for the following information:
  - A unique code up to five characters long to identify the image source
  - Weather station the image source is associated with
  - Name for the image source
  - A description for the image source
Once you have answered the questions you will be given the opportunity to
review your responses and either edit them, create the image source or cancel.
""")

    source_info = {
        'code': None,
        'station_code': None,
        'name': None,
        'description': None
    }

    cur = con.cursor()

    while True:
        source_info = get_new_image_source_info(cur, source_info)

        print("""
You entered the following details:
----------------------------------
Code: {code}
Station Code: {station_code} ({station_id}: {station_name})
Name: {name}
Description: {description}
        """.format(**source_info))

        print("If you no longer wish to create this image source answer no "
              "to the following\ntwo questions.")

        if get_boolean("Is the information entered correct? (y/n)",
                       required=True):
            print("Creating image source...")

            cur.execute("insert into image_source(code, station_id, "
                        "source_name, description) values(%s,%s,%s,%s)", (
                            source_info['code'],
                            source_info['station_id'],
                            source_info['name'],
                            source_info['description']
            ))

            con.commit()
            cur.close()
            print("Image source created.")
            return

        elif not get_boolean("Do you wish to correct it? (Answering no will "
                             "cancel)", True):
            return

    return


def edit_image_source(con):

    print("""

Edit image source
-----------------
The follwoing image sources are available for editing:
""")
    cur = con.cursor()
    codes = list_image_sources(cur)
    selected_source_code = get_code("Image source to edit", codes,
                                    required=True)

    cur.execute("select source_name, description "
                "from image_source "
                "where lower(code) = lower(%s)", (selected_source_code,))

    result = cur.fetchone()

    source_info = {
        'name': result[0],
        'description': result[1]
    }

    while True:
        source_info = get_edit_image_source_info(source_info)

        print("""
You entered the following details:
----------------------------------
Name: {name}
Description: {description}
        """.format(**source_info))

        if get_boolean("Is the information entered correct? (y/n)",
                       required=True):
            print("Updating image source...")

            cur.execute("update image_source set source_name = %s, "
                        "description = %s where lower(code) = lower(%s) ", (
                            source_info['name'],
                            source_info['description'],
                            selected_source_code
                        ))

            con.commit()
            cur.close()
            print("Image source updated.")
            return
        elif not get_boolean("Do you wish to correct it? (Answering no will "
                             "cancel)", True):
            return

    return


def list_image_sources(cur):
    """
    Prints out a list of all image sources
    :param cur: Database cursor
    :returns: A list of valid image source codes
    :rtype: list
    """

    cur.execute("""
select lower(i.code),
       lower(stn.code) as station_code,
       i.source_name
from image_source i
inner join station stn on stn.station_id = i.station_id
order by i.code asc
    """)

    results = cur.fetchall()

    print("Code  Station  Name")
    print("----- -------- -----------------------------------------------"
          "---------------")

    station_codes = []

    for result in results:
        station_codes.append(result[0])
        print("{0:<5} {1:<8} {2}".format(result[0], result[1], result[2]))

    print("----- -------- -----------------------------------------------"
          "---------------")

    return station_codes


def manage_image_sources(con):
    """
    Runs a menu allowing the user to select various image source management
    options.
    :param con: Database connection
    """

    choices = [
        {
            "key": "1",
            "name": "List image sources",
            "type": "func",
            "func": lambda: list_image_sources(con.cursor())
        },
        {
            "key": "2",
            "name": "Create image source",
            "type": "func",
            "func": lambda: create_image_source(con)
        },
        {
            "key": "3",
            "name": "Edit image source",
            "type": "func",
            "func": lambda: edit_image_source(con)
        },
        {
            "key": "0",
            "name": "Return",
            "type": "return"
        }
    ]

    menu(choices, prompt="\n\nManage Image Sources\n---------------\n\n"
                         "Select option")
