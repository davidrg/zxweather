"""
zxweather data import tool - imports data for a single weather station into
a weather database.
"""
import argparse
import json
import mimetypes
from collections import namedtuple

import os
import psycopg2 as psycopg2
from os.path import isfile

Sample = namedtuple("Sample", [
    "station_code",
    "download_timestamp",
    "time_stamp",
    "indoor_humidity",
    "indoor_temperature",
    "humidity",
    "temperature",
    "absolute_pressure",
    "mean_sea_level_pressure",
    "wind_speed",
    "gust",
    "wind_direction",
    "rainfall",

    # Davis fields
    "record_time",
    "record_date",
    "high_temperature",
    "low_temperature",
    "high_rain_rate",
    "solar_radiation",
    "wind_sample_count",
    "gust_wind_direction",
    "average_uv_index",
    "evapotranspiration",
    "high_solar_radiation",
    "high_uv_index",
    "forecast_rule_id",

    # WH1080 fields
    "sample_interval",
    "record_number",
    "last_in_batch",
    "invalid_data",
    "wh_wind_direction",
    "total_rain",
    "rain_overflow"
])


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

    version_major = 1
    version_minor = 0
    version_revision = 0

    cur.execute("select version_check('export',%s,%s,%s)",
                (version_major, version_minor, version_revision))
    compatible = cur.fetchone()[0]

    if not compatible:
        # This version of zxweather is too old for the database.
        cur.execute("select minimum_version_string('export')")
        version_string = cur.fetchone()[0]

        print("ERROR: This database is of a newer format and requires at least"
              "\nzxweather v{0}. You can not use this tool to export "
              "data.".format(version_string))

    return compatible


def connect_to_db(connection_string):
    con = psycopg2.connect(connection_string)

    cur = con.cursor()

    db_version = get_db_version(cur)
    if db_version < 3:
        print("This database is for zxweather v0.x and is not compatible with "
              "this tool.")
        return None

    if not is_version_compatible(cur):
        return None

    return con


def get_station_id(con, station_code):
    cur = con.cursor()
    cur.execute("select station_id from station where code = %s",
                (station_code,))
    x = cur.fetchone()
    station_id = None
    if x is not None:
        station_id = x[0]
    cur.close()
    return station_id


def get_hw_type(con, type_code):
    cur = con.cursor()
    cur.execute("select station_type_id from station_type where code = %s",
                (type_code,))
    result = cur.fetchone()
    if result is None:
        return None

    cur.close()

    return result[0]


def insert_image_source(con, image_source, station_id):
    with con.cursor() as cur:
        image_source["station_id"] = station_id
        cur.execute("""
insert into image_source(code, source_name, description, station_id)
                  values(%(code)s, %(name)s, %(description)s, 
                         %(station_id)s)""", image_source)


def insert_station(con, station_info, include_images):
    type_id = get_hw_type(con, station_info["type"])
    if type_id is None:
        print("Hardware type is not supported by this database.")
        return None

    station_info["type_id"] = type_id

    cur = con.cursor()
    cur.execute("""
insert into station(code, title, description, sample_interval, 
                    live_data_available, sort_order, message, message_timestamp, 
                    station_config, site_title, latitude, longitude, 
                    altitude, station_type_id)
             values(%(code)s, %(title)s, %(description)s, %(sample_interval)s, 
                    %(live_data_available)s, %(sort_order)s, %(message)s, 
                    %(message_time)s, %(config)s, %(site_title)s, %(latitude)s, 
                    %(longitude)s, %(altitude)s, %(type_id)s)""",
                station_info)
    cur.close()

    station_id = get_station_id(con, station_info["code"])

    if include_images:
        for image_source in station_info["image_sources"]:
            insert_image_source(con, image_source, station_id)

    return station_id


def noneable_int(val):
    if val == "None":
        return None
    return int(val)


def noneable_float(val):
    if val == "None":
        return None
    return float(val)


def noneable_bool(val):
    if val == "None":
        return None
    if val == "False":
        return False
    if val == "True":
        return True
    return None


def noneable_str(val):
    if val == "None":
        return None
    return val


def parse_sample(sample):
    if sample is None:
        return None
    parts = sample.split("\t")
    s = Sample(station_code=parts[0],
               download_timestamp=parts[1],
               time_stamp=parts[2],
               indoor_humidity=noneable_int(parts[3]),
               indoor_temperature=noneable_float(parts[4]),
               humidity=noneable_int(parts[5]),
               temperature=noneable_float(parts[6]),
               absolute_pressure=noneable_float(parts[7]),
               mean_sea_level_pressure=noneable_float(parts[8]),
               wind_speed=noneable_float(parts[9]),
               gust=noneable_float(parts[10]),
               wind_direction=noneable_float(parts[11]),
               rainfall=noneable_float(parts[12]),

               # Davis
               record_time=noneable_int(parts[13]),
               record_date=noneable_int(parts[14]),
               high_temperature=noneable_float(parts[15]),
               low_temperature=noneable_float(parts[16]),
               high_rain_rate=noneable_float(parts[17]),
               solar_radiation=noneable_float(parts[18]),
               wind_sample_count=noneable_int(parts[19]),
               gust_wind_direction=noneable_float(parts[20]),
               average_uv_index=noneable_float(parts[21]),
               evapotranspiration=noneable_float(parts[22]),
               high_solar_radiation=noneable_float(parts[23]),
               high_uv_index=noneable_float(parts[24]),
               forecast_rule_id=noneable_int(parts[25]),

               # WH1080
               sample_interval=noneable_int(parts[26]),
               record_number=noneable_int(parts[27]),
               last_in_batch=noneable_bool(parts[28]),
               invalid_data=noneable_bool(parts[29]),
               wh_wind_direction=noneable_str(parts[30]),
               total_rain=noneable_float(parts[31]),
               rain_overflow=noneable_bool(parts[32]))
    return s


def sample_to_dict(sample, station_id):
    return {
        "station_id": station_id,
        "download_timestamp": sample.download_timestamp,
        "time_stamp": sample.time_stamp,
        "indoor_relative_humidity": sample.indoor_humidity,
        "indoor_temperature": sample.indoor_temperature,
        "relative_humidity": sample.humidity,
        "temperature": sample.temperature,
        "absolute_pressure": sample.absolute_pressure,
        "mean_sea_level_pressure": sample.mean_sea_level_pressure,
        "average_wind_speed": sample.wind_speed,
        "gust_wind_speed": sample.gust,
        "wind_direction": sample.wind_direction,
        "rainfall": sample.rainfall,
        "record_time": sample.record_time,
        "record_date": sample.record_date,
        "high_temperature": sample.high_temperature,
        "low_temperature": sample.low_temperature,
        "high_rain_rate": sample.high_rain_rate,
        "solar_radiation": sample.solar_radiation,
        "wind_sample_count": sample.wind_sample_count,
        "gust_wind_direction": sample.gust_wind_direction,
        "average_uv_index": sample.average_uv_index,
        "evapotranspiration": sample.evapotranspiration,
        "high_solar_radiation": sample.high_solar_radiation,
        "high_uv_index": sample.high_uv_index,
        "forecast_rule_id": sample.forecast_rule_id,
        "sample_interval": sample.sample_interval,
        "record_number": sample.record_number,
        "last_in_bach": sample.last_in_batch,
        "invalid_data": sample.invalid_data,
        "wh_wind_direction": sample.wh_wind_direction,
        "total_rain": sample.total_rain,
        "rain_overflow": sample.rain_overflow
    }


def insert_samples(con, samples, station_id, hw_type):
    sample_query = """insert into sample(station_id, download_timestamp,
                                         time_stamp, indoor_relative_humidity,
                                         indoor_temperature, relative_humidity,
                                         temperature, absolute_pressure, mean_sea_level_pressure,
                                         average_wind_speed, gust_wind_speed,
                                         wind_direction, rainfall)
                                  values(%(station_id)s, 
                                         %(download_timestamp)s,
                                         %(time_stamp)s, 
                                         %(indoor_relative_humidity)s,
                                         %(indoor_temperature)s, 
                                         %(relative_humidity)s, %(temperature)s, 
                                         %(absolute_pressure)s, $(mean_sea_level_pressure)s,
                                         %(average_wind_speed)s, 
                                         %(gust_wind_speed)s, 
                                         %(wind_direction)s, %(rainfall)s)
                      returning sample_id"""
    davis_query = """insert into davis_sample(sample_id, record_time, 
                                              record_date,
                                              high_temperature, low_temperature,
                                              high_rain_rate, solar_radiation,
                                              wind_sample_count,
                                              gust_wind_direction,
                                              average_uv_index,
                                              evapotranspiration,
                                              high_solar_radiation,
                                              high_uv_index, forecast_rule_id)
                                       values(%(sample_id)s, %(record_time)s, 
                                              %(record_date)s,
                                              %(high_temperature)s, 
                                              %(low_temperature)s,
                                              %(high_rain_rate)s, 
                                              %(solar_radiation)s,
                                              %(wind_sample_count)s,
                                              %(gust_wind_direction)s,
                                              %(average_uv_index)s,
                                              %(evapotranspiration)s,
                                              %(high_solar_radiation)s,
                                              %(high_uv_index)s, 
                                              %(forecast_rule_id)s) """
    wh1080_query = """insert into wh1080_sample(sample_id, sample_interval,
                                                record_number,
                                                last_in_batch,
                                                invalid_data,
                                                wind_direction,
                                                total_rain,
                                                rain_overflow) 
                                         values(%(sample_id)s, 
                                                %(sample_interval)s,
                                                %(record_number)s,
                                                %(last_in_batch)s,
                                                %(invalid_data)s,
                                                %(wh_wind_direction)s,
                                                %(total_rain)s,
                                                %(rain_overflow)s)"""

    print("Inserting {0} samples...".format(len(samples)))

    cur = con.cursor()
    for sample in samples:
        s = sample_to_dict(sample, station_id)

        cur.execute(sample_query, s)
        s["sample_id"] = cur.fetchone()[0]

        if hw_type == "DAVIS":
            cur.execute(davis_query, s)
        elif hw_type == "WH1080":
            cur.execute(wh1080_query, s)
        con.commit()


def load_samples(con, filename, station_id, start, end, hw_type):
    samples = []

    print("Loading samples from disk...")
    with open(filename, "r") as f:
        while True:
            s = parse_sample(f.readline())
            if s is None:
                break
            if (start is None or s.time_stamp >= start) and \
                    (end is None or s.time_stamp <= end):
                samples.append(s)
    print("Loaded {0} samples".format(len(samples)))

    print("Loading samples from database...")
    cur = con.cursor()
    cur.execute("""
    select time_stamp  
    from sample
    where station_id = %(station_id)s
      and (%(start)s is null or time_stamp >= %(start)s)
      and (%(end)s is null or time_stamp <= %(end)s)""",
                {"station_id": station_id,
                 "start": start,
                 "end": end})
    existing_samples = cur.fetchall()
    cur.close()
    print("Found {0} samples in database".format(len(existing_samples)))

    if len(existing_samples) > 0:
        print("Pruning duplicates...")
        original_len = len(samples)
        samples = [s for s in samples if s.time_stamp not in existing_samples]
        pruned = original_len - len(samples)
        print("Pruned {0} samples.".format(pruned))
    else:
        print("No samples found in database within import timespan.")

    insert_samples(con, samples, station_id, hw_type)


def get_image_source_id(con, source_code):
    cur = con.cursor()
    cur.execute("select image_source_id from image_source where code = %s",
                (source_code,))

    result = cur.fetchone()
    cur.close()

    if result is None:
        return None

    return result[0]


def get_image_type_id(con, type_code):
    cur = con.cursor()
    cur.execute("select image_type_id from image_type where code = %s",
                (type_code,))
    result = cur.fetchone()
    cur.close()

    if result is None:
        return None
    return result[0]


def insert_images(con, base_dir, start, end, image_source_codes):
    image_type_cache = {}

    cur = con.cursor()
    for src_code in image_source_codes:
        src_dir = os.path.join(base_dir, src_code)
        src_id = get_image_source_id(con, src_code)

        if not os.path.exists(src_dir):
            print("No images available for {0} (looked in {1})".format(
                src_code, src_dir))
            continue

        files = [f for f in os.listdir(src_dir) if isfile(os.path.join(src_dir, f))]
        for fn in files:
            if fn.endswith(".json"):
                with open(os.path.join(src_dir, fn), "r") as f:
                    img = json.loads(f.read())

                    if (start is None or img["time"] < start) and \
                            (end is None or img["time"] > end):
                        # Skip - image its outside the import time span.
                        continue

                    ext = mimetypes.guess_extension(img["mime"])
                    if ext == ".jpe":
                        ext = ".jpeg"

                    data_filename = os.path.basename(fn) + ext
                    data_pathname = os.path.join(src_dir, data_filename)
                    if os.path.exists(data_pathname):
                        with open(data_pathname, "rb") as df:
                            img["image_data"] = psycopg2.Binary(df.read())
                    else:
                        print("Skip file {0} - matching data file {1} could "
                              "not be found.".format(fn, data_filename))
                        continue

                img["image_source_id"] = src_id

                if img["type"] in image_type_cache:
                    img["image_type_id"] = image_type_cache[img["type"]]
                else:
                    image_type_cache[img["type"]] = get_image_source_id(con, img["type"])
                    img["image_type_id"] = image_type_cache[img["type"]]

                if img["image_type_id"] is None:
                    print("Image type {0} used by image {1} is not "
                          "supported by this database.".format(img["type"],
                                                               fn))
                    continue

                cur.execute("""
insert into image(image_source_id, 
                  image_type_id, 
                  time_stamp, 
                  title, 
                  description, 
                  mime_type, 
                  metadata, 
                  image_data)
values(%(image_source_id)s, 
       %(image_type_id)s, 
       %(time)s, 
       %(title)s, 
       %(description)s, 
       %(mime)s, 
       %(metadata)s, 
       %(image_data)s)
""", )
                con.commit()


def main():
    parser = argparse.ArgumentParser(
        description="Imports data to a weather database"
    )
    parser.add_argument('host', metavar='host', type=str,
                        help="Database server hostname")
    parser.add_argument('database', metavar='database', type=str,
                        help="Database name")
    parser.add_argument('user', metavar='user', type=str,
                        help="Database username")
    parser.add_argument('password', metavar='password', type=str,
                        help="Database password")
    parser.add_argument('source', metavar='src', type=str,
                        help="Station data to import (station.json file)")
    parser.add_argument('--port', metavar='port', type=int, default=5432,
                        help='Database server TCP port')
    parser.add_argument('--start-time', metavar='start_time', type=str,
                        help='Start time to import')
    parser.add_argument('--end-time', metavar='end_time', type=str,
                        help='End time to import')
    parser.add_argument('--include-images', action='store_true')

    args = parser.parse_args()

    constr = "host={host} port={port} user={user} password={password} " \
             "dbname={name}".format(
                host=args.host,
                port=args.port,
                user=args.user,
                password=args.password,
                name=args.database)

    print("Database connect...")
    con = connect_to_db(constr)
    if con is None:
        return  # Incompatible or other error

    print("Database connected")

    with open(args.source, "r") as f:
        station = json.loads(f.read())

    station_id = get_station_id(con, station["code"])
    if station_id is None:
        station_id = insert_station(con, station, args.include_images)

    if station_id is None:
        print("Failed to find or insert station.")
        return

    load_samples(con, args.source, station_id, args.start, args.end,
                 station["type"])

    if args.include_images:
        base_dir = os.path.dirname(os.path.abspath(args.source))
        insert_images(con, base_dir, args.start, args.end,
                      [x["code"] for x in station["image_sources"]])


if __name__ == "__main__":
    main()
