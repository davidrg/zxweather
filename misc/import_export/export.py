"""
zxweather data export tool - exports data for one or more weather stations from
a weather database.
"""

import argparse
import json
import mimetypes

import os

import psycopg2 as psycopg2


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


def export_samples(con, dest_dir, station, start, end):
    print("Data export stage")
    data_query = """
    select  stn.code as station_code,
            s.download_timestamp,
            s.time_stamp,
            s.indoor_relative_humidity,
            s.indoor_temperature,
            s.relative_humidity,
            s.temperature,
            s.absolute_pressure,
            s.average_wind_speed,
            s.gust_wind_speed,
            s.wind_direction,
            s.rainfall,
            ds.record_time,
            ds.record_date,
            ds.high_temperature,
            ds.low_temperature,
            ds.high_rain_rate,
            ds.solar_radiation,
            ds.wind_sample_count,
            ds.gust_wind_direction,
            ds.average_uv_index,
            ds.evapotranspiration,
            ds.high_solar_radiation,
            ds.high_uv_index,
            ds.forecast_rule_id,
            wh.sample_interval,
            wh.record_number,
            wh.last_in_batch,
            wh.invalid_data,
            wh.wind_direction,
            wh.total_rain,
            wh.rain_overflow
    from sample s
    left outer join davis_sample ds on ds.sample_id = s.sample_id
    left outer join wh1080_sample wh on wh.sample_id = s.sample_id
    inner join station stn on stn.station_id = s.station_id
    where (%(code)s is null or stn.code = %(code)s)
      and (%(start)s is null or s.time_stamp >= %(start)s)
      and (%(end)s is null or s.time_stamp <= %(end)s)
    order by stn.code, s.time_stamp asc
    """

    row_format = "{station_code}\t{download_timestamp}\t{time_stamp}\t" \
                 "{indoor_relative_humidity}\t{indoor_temperature}\t" \
                 "{relative_humidity}\t{temperature}\t{absolute_pressure}\t" \
                 "{average_wind_speed}\t{gust_wind_speed}\t{wind_direction}\t" \
                 "{rainfall}\t{record_time}\t{record_date}\t" \
                 "{high_temperature}\t{low_temperature}\t{high_rain_rate}\t" \
                 "{solar_radiation}\t{wind_sample_count}\t" \
                 "{gust_wind_direction}\t{average_uv_index}\t" \
                 "{evapotranspiration}\t{high_solar_radiation}\t" \
                 "{high_uv_index}\t{forecast_rule_id}\t" \
                 "{sample_interval}\t{record_number}\t{last_in_batch}\t" \
                 "{invalid_data}\t{wh_wind_direction}\t{total_rain}\t" \
                 "{rain_overflow}\n"

    print("Query...")

    cur = con.cursor()
    cur.execute(data_query, {
        "code": station,
        "start": start,
        "end": end
    })
    results = cur.fetchall()
    cur.close()

    print("Query complete. Writing out to disk...")

    # Now write the data out.
    current_stn = None
    data_file = None
    stations = []
    for row in results:
        if row[0] != current_stn:
            current_stn = row[0]
            stations.append(current_stn)
            stn_dir = os.path.join(dest_dir, current_stn)

            if not os.path.exists(stn_dir):
                os.makedirs(stn_dir)

            if data_file is not None:
                data_file.flush()
                data_file.close()

            data_file = open(os.path.join(stn_dir, 'samples.dat'), "w")
            data_file.write("#exporter v1.0.0\n")
            data_file.write("#station_code\tdownload_timestamp\ttime_stamp\t"
                            "indoor_relative_humidity\tindoor_temperature\t"
                            "relative_humidity\ttemperature\t"
                            "absolute_pressure\taverage_wind_speed\t"
                            "gust_wind_speed\twind_direction\trainfall\t"
                            "record_time\trecord_date\thigh_temperature\t"
                            "low_temperature\thigh_rain_rate\t"
                            "solar_radiation\twind_sample_count\t"
                            "gust_wind_direction\taverage_uv_index\t"
                            "evapotranspiration\thigh_solar_radiation\t"
                            "high_uv_index\torecast_rule_id\t"
                            "sample_interval\trecord_number\tlast_in_batch\t"
                            "invalid_data\twh_wind_direction\ttotal_rain\t"
                            "rain_overflow\n")

            print("Writing station {0}...".format(current_stn))

        data_file.write(row_format.format(
            station_code=row[0],
            download_timestamp=row[1],
            time_stamp=row[2],
            indoor_relative_humidity=row[3],
            indoor_temperature=row[4],
            relative_humidity=row[5],
            temperature=row[6],
            absolute_pressure=row[7],
            average_wind_speed=row[8],
            gust_wind_speed=row[9],
            wind_direction=row[10],
            rainfall=row[11],
            record_time=row[12],
            record_date=row[13],
            high_temperature=row[14],
            low_temperature=row[15],
            high_rain_rate=row[16],
            solar_radiation=row[17],
            wind_sample_count=row[18],
            gust_wind_direction=row[19],
            average_uv_index=row[20],
            evapotranspiration=row[21],
            high_solar_radiation=row[22],
            high_uv_index=row[23],
            forecast_rule_id=row[24],
            sample_interval=row[25],
            record_number=row[26],
            last_in_batch=row[27],
            invalid_data=row[28],
            wh_wind_direction=row[29],
            total_rain=row[30],
            rain_overflow=row[31]))
    cur.close()
    data_file.close()
    return stations


def get_image(con, image_id):
    query = "select image_data from image where image_id = %s"

    cur = con.cursor()
    cur.execute(query, (image_id,))
    result = cur.fetchone()[0]
    cur.close()

    return result


def export_images(con, dest_dir, station, start, end):
    print("Image export stage")
    data_query = """
select stn.code as station_code,
       src.code as source_code,
       img_type.code as type_code,
       img.time_stamp,
       img.title,
       img.description,
       img.mime_type,
       img.metadata,
       img.image_id
from image img
inner join image_type img_type on img_type.image_type_id = img.image_type_id
inner join image_source src on src.image_source_id = img.image_source_id
inner join station stn on stn.station_id = src.station_id 
where (%(code)s is null or stn.code = %(code)s)
      and (%(start)s is null or img.time_stamp >= %(start)s)
      and (%(end)s is null or img.time_stamp <= %(end)s)
order by stn.code, img.time_stamp asc
    """

    print("Metadata query...")

    cur = con.cursor()
    cur.execute(data_query, {
        "code": station,
        "start": start,
        "end": end
    })
    results = cur.fetchall()
    cur.close()

    print("Metadata query complete. Writing out to disk...")

    # Now write the data out.
    current_src = None
    src_dir = None
    image_sources = []
    for row in results:
        if row[1] != current_src:
            current_stn = row[0]
            current_src = row[1]
            image_sources.append(current_src)
            src_dir = os.path.join(dest_dir, current_stn, current_src)

            if not os.path.exists(src_dir):
                os.makedirs(src_dir)

        imgd = {
            "station": row[0],
            "source": row[1],
            "type": row[2],
            "time": row[3].isoformat(),
            "title": row[4],
            "description": row[5],
            "mime": row[6],
            "metadata": row[7],
            "id": row[8]
        }

        print("Image {0}".format(imgd["id"]))

        meta_fn = os.path.join(src_dir, "{0}.json".format(imgd["id"]))

        with open(meta_fn, "w") as f:
            f.write(json.dumps(imgd))

        ext = mimetypes.guess_extension(imgd["mime"])
        if ext == ".jpe":
            ext = ".jpeg"

        img_fn = os.path.join(src_dir, "{0}{1}".format(imgd["id"], ext))

        with open(img_fn, "wb") as f:
            f.write(get_image(con, imgd["id"]))


def export_config(con, dest_dir, stations):
    stn_query = """
    select stn.code,
           stn.title,
           stn.description,
           stn.sample_interval,
           stn.live_data_available,
           stn.sort_order,
           stn.message,
           stn.message_timestamp,
           stn.station_config,
           stn.site_title,
           stn.latitude,
           stn.longitude,
           stn.altitude,
           typ.code as type_code,
           typ.title as type_title
    from station stn
    inner join station_type typ on typ.station_type_id = stn.station_type_id
    where stn.code = %s    
    """

    img_src_query = """
    select src.code,
           src.source_name,
           src.description
    from image_source src
    inner join station stn on stn.station_id = src.station_id
    where stn.code = %s
    """

    cur = con.cursor()

    for station in stations:
        dest = os.path.join(dest_dir, station, "station.json")
        cur.execute(stn_query, (station,))
        stn = cur.fetchone()
        cur.execute(img_src_query, (station, ))
        source_rows = cur.fetchall()

        sources = []
        for source in source_rows:
            sources.append({
                "code": source[0],
                "name": source[1],
                "description": source[2]
            })

        info = {
            "code": stn[0],
            "title": stn[1],
            "description": stn[2],
            "sample_interval": stn[3],
            "live_data_available": stn[4],
            "sort_order": stn[5],
            "message": stn[6],
            "message_time": stn[7].isoformat() if stn[7] is not None else None,
            "config": stn[8],
            "site_title": stn[9],
            "latitude": stn[10],
            "longitude": stn[11],
            "altitude": stn[12],
            "type": stn[13],
            "type_title": stn[14],
            "image_sources": sources
        }

        with open(dest, "w") as f:
            f.write(json.dumps(info, indent=4, sort_keys=True))

    cur.close()


def main():
    parser = argparse.ArgumentParser(
        description="Exports data from a weather database"
    )
    parser.add_argument('host', metavar='host', type=str,
                        help="Database server hostname")
    parser.add_argument('database', metavar='database', type=str,
                        help="Database name")
    parser.add_argument('user', metavar='user', type=str,
                        help="Database username")
    parser.add_argument('password', metavar='password', type=str,
                        help="Database password")
    parser.add_argument('destination', metavar='dest', type=str,
                        help="Output directory")
    parser.add_argument('--port', metavar='port', type=int, default=5432,
                        help='Database server TCP port')
    parser.add_argument('--station', metavar='station', type=str,
                        help='Station to export')
    parser.add_argument('--start-time', metavar='start_time', type=str,
                        help='Start time to export')
    parser.add_argument('--end-time', metavar='end_time', type=str,
                        help='End time to export')
    parser.add_argument('--include-images', action='store_true')

    args = parser.parse_args()

    constr = "host={host} port={port} user={user} password={password} " \
        "dbname={name}".format(
            host=args.host,
            port=args.port,
            user=args.user,
            password=args.password,
            name=args.database
        )

    print("Database connect...")
    con = connect_to_db(constr)
    if con is None:
        return  # Incompatible or other error

    print("Database connected")

    # TODO: chunk this? Python uses about 1GB RAM to export all data for
    # weather.zx.net.nz up to the end of march 2018. Not too much of a problem
    # at this time but it could become an issue in a few years.
    stations = export_samples(con, args.destination, args.station,
                              args.start_time, args.end_time)

    export_config(con, args.destination, stations)

    if args.include_images:
        export_images(con, args.destination, args.station, args.start_time,
                      args.end_time)


if __name__ == "__main__":
    main()
