# coding=utf-8
"""
zxweather admin tool.
"""
from database_mgr import  create_db, connect_to_db, db_info
from station_mgr import  manage_stations
from ui import menu

__author__ = 'David Goodwin'

def read_db_config():
    """
    Attempts to read database connection details from the zxweather
    configuration file.
    :return: Database connection details
    :rtype: db_info
    """
    import ConfigParser
    config = ConfigParser.ConfigParser()
    config.read(['config.cfg','../zxw_web/config.cfg', 'zxw_web/config.cfg', '/etc/zxweather.cfg'])

    S_DB = 'database'   # Database configuration

    try:
        # Database
        return db_info(
            config.get(S_DB, 'host'),
            config.getint(S_DB, 'port'),
            config.get(S_DB, 'user'),
            config.get(S_DB, 'password'),
            config.get(S_DB, 'database'))

    except ConfigParser.NoSectionError:
        return None

def db_menu():
    """
    Asks the user to choose how to connect to the database and then obtains
    connection details from the user.
    :return: Database connection details
    :rtype: db_info or None
    """
    choices = [
        {
            "key": "1",
            "name": "Create a new weather database",
            "type": "return"
        },
        {
            "key": "2",
            "name": "Connect to an existing weather database",
            "type": "return"
        },
        {
            "key": "0",
            "name": "Exit",
            "type": "func",
            "func": exit
        }
    ]
    result = menu(choices)
    print result
    if result == "1":
        return create_db()
    elif result == "2":
        return db_info.prompt_db_config()

    return None

def get_db_connection():
    """
    Gets a connection to a weather database. If connection details can not be
    obtained from a zxweather configuration file the user will be prompted for
    them.
    """
    dbc = read_db_config()
    db = None

    if dbc is not None:
        db = connect_to_db(dbc)
    else:
        print("Failed to read database configuration from zxweather "
              "configuration file.\n")


    while db is None:
        dbc = db_menu()
        if dbc is not None:
            db = connect_to_db(dbc)
        else:
            print("ERROR: No database details entered")

    return db

def main():
    """
    Program entry point
    """
    print("------------------------------------------------------------------------------")
    print("ZXWeather v0.2 admin tool\n\t(C) Copyright David Goodwin, 2013\n")

    db = get_db_connection()

    choices = [
        {
            "key": "1",
            "name": "Manage stations",
            "type": "return"
        },
        {
            "key": "0",
            "name": "Exit",
            "type": "func",
            "func": exit
        }
    ]

    while True:
        result = menu(choices)
        if result == "1":
            manage_stations(db)

if __name__ == "__main__": main()