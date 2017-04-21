# coding=utf-8
"""
Database deployment facility
"""
import psycopg2
from ui import get_boolean, get_string_with_length_indicator, get_string, get_number

__author__ = 'David Goodwin'

# This version number
version_major = 1
version_minor = 0
version_revision = 0

v2_upgrade_script = "database/upgrade_v2.sql"
v3_upgrade_script = "database/upgrade_v3.sql"
create_script = "database/database.sql"


class db_info(object):
    """
    An object for storing database information
    """
    def __init__(self, hostname, port, user, password, database):
        """
        Creates a new db_info instance.,
        :param hostname: Server hostname
        :type hostname: str
        :param port: Server port
        :type port: int
        :param user: Username to login as
        :type user: str
        :param password: Users password
        :type password: str
        :param database: Name of the database to use
        :type database: str
        :returns: A new db_info instance
        :rtype: db_info
        """
        self.hostname = hostname
        self.port = port
        self.user = user
        self.password = password
        self.database = database

    def to_connection_string(self):
        """
        Converts the database info dict to a connection string
        :return: Database connection string
        :rtype: str
        """
        conn_str = "host={host} port={port} user={user} password={password} " \
                   "dbname={name}".format(
            host=self.hostname,
            port=self.port,
            user=self.user,
            password=self.password,
            name=self.database
        )

        return conn_str

    @staticmethod
    def prompt_db_config(new_db=False):
        """
        Asks the user for details for the details of an existing weather database.
        :return: Database connection details
        :rtype: db_info
        """
        print("You will now be prompted for database configuration details.")
        hostname = get_string("Hostname", "localhost")
        port = get_number("Port", 5432)
        if new_db:
            print("The database name must start with a letter and may only "
                  "contain letters,\ndigits or the underscore character ('_'). "
                  "Valid database names include\nweather, weather42, "
                  "my_weather_database, weather_42_database. If your\ndatabase"
                  " name does not meet these restrictions you'll get an error "
                  "later on.")
            name = get_string("New database name", "weather")

        else:
            name = get_string("Database name", "weather")
        user = get_string("Username", "postgres")
        password = get_string("Password", required=True)

        return db_info(hostname, port, user, password, name)


def upgrade_v0_2(con):
    """
    Upgrades the v0.1 database to v0.2.
    :param con: Database connection
    :return: Success (true) or failure (false)
    :rtype: bool
    """
    global v2_upgrade_script

    print("Loading upgrade script...")
    f = open(v2_upgrade_script, 'r')
    script = f.read()
    f.close()

    print("""
zxweather v0.2 database upgrade procedure
-----------------------------------------

This procedure will upgrade your standard zxweather v0.1.x database to v0.2. In
the event of failure all database changes should be rolled back automatically
making this a fairly safe operation. You should still ensure you have a current
good backup of your database however. If you have customised the security on
your database you will likely need to reapply these customisations.""")

    # Make sure the user has proper backups.
    backups = get_boolean("Are you satisfied with the state of your backups? (y/n)",
        required=True)
    if not backups:
        print("Upgrade canceled.")
        return False

    # Proceed with upgrade. We need to gather some information for this:
    #  - station code (5 chars max)
    #  - station name
    #  - description

    print("""
You will now be prompted for some information about your weather station:
  - Station Code
  - Station Name
  - Description

Station Code is the short (up to 5 characters) name for your weather station
that appears in all URLs. For example, if your station was called 'foo' your
URLs would look something like http://example.com/s/foo/. The value you
enter here *must* match the value currently in your zxweather configuration
file (called station_name under the [site] section). This value can not be
changed later without breaking any links within your site.""")
    station_code = get_string_with_length_indicator(
        "Station Code", None, 5, True)

    print("""
Station Name is a longer name for your station that will be displayed to
users.""")
    station_name = get_string("Name", required=True)

    print("""
You may now enter an optional short description for your weather station.""")
    station_description = get_string("Description")

    cur = con.cursor()

    print("Performing upgrade. This may take some time...")
    try:
        cur.execute(script)

        # There will only be a single station in the database at this point. It
        # will be something like:
        #  Code: UNKN
        #  Name: Unknown WH1080-compatible Weather Station
        #  Desc: Unknown WH1080-compatible Weather Station migrated from
        #        zxweather v0.1
        #
        # We will just overwrite these default values with those chosen by the
        # user.
        cur.execute("UPDATE station SET code = upper(%s), title = %s, description = %s;",
            (station_code, station_name, station_description))
        con.commit()

        cur.close()
    except psycopg2.InternalError as inst:
        print("Upgrade failed. Error: {0}".format(inst.message))
        cur.close()
        return False

    print("Upgrade completed successfully.")
    return True


def upgrade_v1_0(con):
    """
    Upgrades the v0.2 database to v1.0.
    :param con: Database connection
    :return: Success (true) or failure (false)
    :rtype: bool
    """
    global v2_upgrade_script

    print("Loading upgrade script...")
    f = open(v3_upgrade_script, 'r')
    script = f.read()
    f.close()

    print("""
zxweather v1.0 database upgrade procedure
-----------------------------------------

This procedure will upgrade your standard zxweather v0.2.x database to v1.0. In
the event of failure all database changes should be rolled back automatically
making this a fairly safe operation. You should still ensure you have a current
good backup of your database however. If you have customised the security on
your database you will likely need to reapply these customisations.""")

    # Make sure the user has proper backups.
    backups = get_boolean("Are you satisfied with the state of your backups? "
                          "(y/n)",
                          required=True)
    if not backups:
        print("Upgrade canceled.")
        return False

    cur = con.cursor()

    print("Performing upgrade. This may take some time...")
    try:
        cur.execute(script)
        con.commit()
        cur.close()
    except psycopg2.InternalError as inst:
        print("Upgrade failed. Error: {0}".format(inst.message))
        cur.close()
        return False

    print("Upgrade completed successfully.")
    return True

def get_db_version(cur):
    """
    Gets the database version number
    :param cur: database cursor
    :return:
    """
    cur.execute("select * from information_schema.tables where "
                "table_schema = 'public' and table_name = 'db_info'")
    results = cur.fetchall()

    # The db_info table doesn't exist. Only zxweather v0.1 lacked this table.
    if len(results) == 0:
        return 1

    cur.execute("select v from db_info where k = 'DB_VERSION'")
    result = cur.fetchone()
    return int(result[0])

def is_version_compatible(cur):
    """
    Checks to see if the database claims to be compatible with this version
    of zxweather.
    :param cur: Database cursor
    :return: Returns if the database claims to be compatible with this version
             of zxweather.
    :rtype: bool
    """
    global version_major, version_minor, version_revision

    cur.execute("select version_check('admin_tool',%s,%s,%s)",
                 (version_major, version_minor, version_revision))
    compatible = cur.fetchone()[0]

    if not compatible:
        # This version of zxweather is too old for the database.
        cur.execute("select minimum_version_string('admin_tool')")
        version_string = cur.fetchone()[0]

        print("ERROR: This database is of a newer format and requires at least"
              "\nzxweather v{0}. You can not use this tool to manage the "
              "database.".format(version_string))

    return compatible

def connect_to_db(dbc):
    """
    Attempts to connect to the specified database.
    :param dbc: Database connection information
    :type dbc: db_info
    :return: A database connection or None if there was an error
    :rtype: psycopg2._psycopg2
    """

    conn_str = dbc.to_connection_string()

    try:
        con = psycopg2.connect(conn_str)
        cur = con.cursor()

        cur.execute("select version()")
        data = cur.fetchone()
        print("Connect succeeded. Server version: {0}".format(data[0]))

        # Check that the version is compatible.
        db_version = get_db_version(cur)

        # Database is for an older version of zxweather and must be upgraded.
        if db_version in (1,2):
            print("This database is for zxweather v0.x. It must be upgraded "
                  "to v1.0 to be\ncompatible with this tool.")

            print("If you have added new tables or columns to your database "
                  "you can not upgrade\nit with this tool and should not "
                  "proceed.")

            if not get_boolean("Do you wish to upgrade this database?", False):
                print("Upgrade canceled.")
                return None  # We can't proceed with a v0.1 database.

            if db_version == 1:
                # Try to perform the database upgrade
                if not upgrade_v0_2(con):
                    print("Database upgrade failed.")
                    return None  # Its still a v0.1 database.

                print("Your database has been upgraded to the v0.2 format. The"
                      "v1.0 format upgrade will now start.")

            # It must be a v2 database (or a v1 that has just been upgraded to
            # schema v2) so upgrading is still required.
            if not upgrade_v1_0(con):
                print("Database upgrade failed.")
                return None  # Its still a v0.2 database


        # The database is for some newer version of zxweather and cant be used.
        elif db_version > 2 and not is_version_compatible(cur):
            # Database claims to require a newer version of zxweather. The
            # is_version_compatible function will have already told the user
            # this.
            return None

    # Connect failed for some reason.
    except psycopg2.OperationalError as oe:
        print("Failed to connect to database: {0}".format(oe.message))
        return None

    # Database claims to be compatible with this version of zxweather or we've
    # upgraded it to something that is compatible.
    return con

def create_db():
    """
    Creates a new database.
    :return: Success (true) or failure (false)
    :rtype: bool
    """

    global create_script

    con = None
    new_db_name = None
    dbc = None

    while con is None:
        print("Enter the details for the database you wish to create")
        dbc = db_info.prompt_db_config(True)

        new_db_name = dbc.database
        dbc.database = "postgres"
        conn_str = dbc.to_connection_string()

        try:
            con = psycopg2.connect(conn_str)
            cur = con.cursor()

            cur.execute("select version()")
            data = cur.fetchone()
            print("Connect succeeded. Server version: {0}".format(data[0]))

        # Connect failed for some reason.
        except psycopg2.OperationalError as oe:
            print("Failed to connect to database: {0}".format(oe.message))

            if not get_boolean("Do you wish to try to connect to another database server?", False):
                # User canceled
                return None
            else:
                con = None

    print("Loading create script...")
    f = open(create_script, 'r')
    script = f.read()
    f.close()

    cur = con.cursor()
    print("Creating new database...")
    con.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
    cur.execute("CREATE DATABASE {0};".format(new_db_name))
    con.commit()
    cur.close()
    con.close()

    try:
        dbc.database = new_db_name
        conn_str = dbc.to_connection_string()

        con = psycopg2.connect(conn_str)

        cur = con.cursor()
        cur.execute("select version()")
        data = cur.fetchone()
        print("Reconnect succeeded. Server version: {0}".format(data[0]))

        print("Creating database structure. This may take a little while...")
        cur.execute(script)
        con.commit()
        cur.close()
        con.close()

        print("New database created successfully.")

        return dbc

    except psycopg2.OperationalError as oe:
        print("Failed to connect to new database: {0}".format(oe.message))
        return None

