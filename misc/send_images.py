import psycopg2
from psycopg2.extras import NamedTupleCursor
import time

# TODO: config option to specify order image type codes should be processed in
# TODO: custom pause length for each type?
#  Or much better for videos: pause until there are none in awaiting_confirmation
#  and some threshold has passed


class Config(object):
    """
    Handles reading the configuration file.
    """

    _config_file_locations = [
        'send_images.cfg',
        '/etc/zxweather/send_images.cfg'
    ]

    def __init__(self):
        import ConfigParser
        config = ConfigParser.ConfigParser()
        config.read(self._config_file_locations)

        # TODO: load from config
        self.type_codes = ['CAM', 'TLVID']
        self.pause = 10

        # Read database config
        hostname = config.get('database', 'host')
        port = config.getint('database', 'port')
        database = config.get('database', 'database')
        user = config.get('database', 'user')
        pw = config.get('database', 'password')

        self._dsn = "host={host} port={port} user={user} password={password} " \
                    "dbname={name}".format(host=hostname,
                                           port=port,
                                           user=user,
                                           password=pw,
                                           name=database)

    def getDatabaseConnection(self):
        con = psycopg2.connect(self._dsn)

        cur = con.cursor()
        cur.execute("select 1 from INFORMATION_SCHEMA.tables where table_name = 'db_info'")
        result = cur.fetchone()

        if result is None:
            db_version = 1
        else:
            cur.execute("select v::integer from db_info where k = 'DB_VERSION'")
            db_version = cur.fetchone()[0]

        if db_version < 3:
            print("*** ERROR: send_images requires at least database "
                  "version 3 (zxweather 1.0.0). Please upgrade your "
                  "database.")
            exit(1)

        cur.execute("select version_check('SENDIMG',1,0,0)")
        result = cur.fetchone()
        if not result[0]:
            cur.execute("select minimum_version_string('SENDIMG')")
            result = cur.fetchone()

            print("*** ERROR: This version of the send_images is "
                  "incompatible with the configured database. The "
                  "minimum send_images (or zxweather) version "
                  "supported by this database is: {0}.".format(
                    result[0]))
            exit(1)

        return con


def reset_statuses(con, type_code):
    cursor = con.cursor()
    cursor.execute("""
update image_replication_status 
   set status = 'pending', retries=0 
 where status='awaiting_confirmation' 
   and retries=5 
   and image_id in (
        select irs.image_id 
        from image_replication_status irs 
        inner join image i on i.image_id = irs.image_id
        inner join image_type it on it.image_type_id = i.image_type_id
        where irs.status <> 'done' 
        and it.code = %s);
        """, (type_code,))
    con.commit()
    cursor.close()


def get_pending_count(con, type_code):
    cursor = con.cursor(cursor_factory=NamedTupleCursor)
    cursor.execute("""
select count(*) as count from image_replication_status 
where image_id in (
    select irs.image_id 
    from image_replication_status irs 
    inner join image i on i.image_id = irs.image_id
    inner join image_type it on it.image_type_id = i.image_type_id
    where irs.status = 'pending'
      and it.code = %s);
    """, (type_code,))
    result = cursor.fetchone().count
    cursor.close()
    return result


def get_unsent_image_id(con):
    cursor = con.cursor(cursor_factory=NamedTupleCursor)
    cursor.execute("""
select irs.image_id from image_replication_status irs 
where irs.status = 'pending' and retries < 5
limit 1;
        """)
    result = cursor.fetchone().image_id
    cursor.close()
    return result


def transmit(con):
    image_id = get_unsent_image_id(con)

    cursor = con.cursor()
    cursor.execute("select pg_notify('new_image', %s);", (str(image_id),))
    con.commit()
    cursor.close()


def main():
    config = Config()

    con = config.getDatabaseConnection()

    for type_code in config.type_codes:
        print("Processing images of type: {0}...".format(type_code))
        reset_statuses(con, type_code)
        while True:
            remaining = get_pending_count(con, type_code)
            if remaining == 0:
                print("type {0} complete!".format(type_code))
                break
            print("{0} remaining of type {1}. Transmit...".format(remaining, type_code))
            transmit(con)
            time.sleep(config.pause)

    con.close()


if __name__ == "__main__":
    main()
