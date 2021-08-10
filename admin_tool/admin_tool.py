# coding=utf-8
"""
zxweather admin tool.
"""
from __future__ import print_function
from about_mgr import upgrade_about
from image_sources import manage_image_sources
from database_mgr import create_db, connect_to_db, DbInfo
from station_mgr import manage_stations
from ui import menu

__author__ = 'David Goodwin'

S_DB = 'database'   # Database configuration


def read_db_config():
    """
    Attempts to read database connection details from the zxweather
    configuration file.
    :return: Database connection details
    :rtype: DbInfo
    """
    try:
        from ConfigParser import ConfigParser, NoSectionError
    except ImportError:
        from configparser import ConfigParser, NoSectionError

    config = ConfigParser()
    config.read(['config.cfg', '../zxw_web/config.cfg', 'zxw_web/config.cfg',
                 '/etc/zxweather.cfg'])

    try:
        # Database
        return DbInfo(
            config.get(S_DB, 'host'),
            config.getint(S_DB, 'port'),
            config.get(S_DB, 'user'),
            config.get(S_DB, 'password'),
            config.get(S_DB, 'database'))

    except NoSectionError:
        return None


def db_menu():
    """
    Asks the user to choose how to connect to the database and then obtains
    connection details from the user.
    :return: Database connection details
    :rtype: DbInfo or None
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
    print(result)
    if result == "1":
        return create_db()
    elif result == "2":
        return DbInfo.prompt_db_config()

    return None


def get_db_connection():
    """
    Gets a connection to a weather database. If connection details can not be
    obtained from a zxweather configuration file the user will be prompted for
    them.
    """

    db = None

    # Disabled as the database config in the main configuration file probably
    # won't have enough rights to perform database upgrades, etc.
#    dbc = read_db_config()
#    if dbc is not None:
#        db = connect_to_db(dbc)
#    else:
#        print("Failed to read database configuration from zxweather "
#              "configuration file.\n")

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
    print("ZXWeather v1.0 admin tool\n\t(C) Copyright David Goodwin, 2013-2021\n")

    db = get_db_connection()

    choices = [
        {
            "key": "1",
            "name": "Manage stations",
            "type": "return"
        },
        {
            "key": "2",
            "name": "Manage image sources",
            "type": "return"
        },
        {
            "key": "3",
            "name": "Upgrade about.html",
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
        elif result == "2":
            manage_image_sources(db)
        elif result == "3":
            upgrade_about(db.cursor())


if __name__ == "__main__":
    main()
