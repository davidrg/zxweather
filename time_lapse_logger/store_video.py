# coding=utf-8
"""
A simple program to store a video in the database using data stored in a JSON
file. Allows quick import of videos generated by the time lapse logger that
couldn't be stored in the database for whatever reason.

Example usage - store_video.py "host=localhost port=5432 user=postgres password=password dbname=weather" /var/lib/zxweather/time_lapse_logger/failed_videos/*_info.json
"""
import argparse
import imghdr
import json
import mimetypes
import os

import psycopg2


def main():
    parser = argparse.ArgumentParser(description="Loads an image into a weather database")
    parser.add_argument("dsn",
                        type=str,
                        help="Database connection string")
    parser.add_argument("description_file",
                        type=argparse.FileType('r'),
                        help="Image description JSON file",
                        nargs="*")
    parser.add_argument("--clean", dest="clean", action='store_true',
                        help="Remove input files on success")

    args = parser.parse_args()

    for desc in args.description_file:
        data = json.loads(desc.read())

        if "video" in data:
            image_file = data["video"]
        else:
            image_file = data["image"]

        store_image(args.dsn, data["source"], data["type"], data["time"],
                    data["title"], data["description"], data["mime"],
                    image_file, data["metadata"])

        if args.clean:
            print("Cleaning inputs...")
            print("Image: {0}".format(image_file))
            os.remove(image_file)
            if os.path.excists(data["metadata"]):
                print("Meta: {0}".format(data["metadata"]))
                os.remove(data["metadata"])
            print("Desc: {0}".format(desc.name))
            os.remove(desc.name)
            print("done.")


def store_image(dsn, image_source, image_type_code, time_stamp, title,
                description, mime_type, image_file, metadata_file):

    print("""
----
Source: {0}
Type: {1}
Time: {2}
Title: {3}
Description: {4}
MIME Type: {5}
File: {6}
Metadata: {7}
""".format(image_source, image_type_code, time_stamp, title, description,
           mime_type, image_file, metadata_file))

    # Load metadata if we have any
    metadata_data = None
    if metadata_file is not None:
        with open(metadata_file, 'r') as f:
            metadata_data = f.read()

    # Try to guess the MIME type if the user hasn't specified one
    mime_type = mime_type
    if mime_type is None:
        # noinspection PyBroadException
        try:
            image_type = imghdr.what(image_file)
            mime_type = mimetypes.guess_type("foo.{0}".format(image_type))[0]
        except:
            print("Failed to guess MIME type for image file. Manually specify "
                  "MIME type with --mime-type parameter.")
            return

    image_data = None
    with open(image_file, 'rb') as f:
        image_data = f.read()

    # Connect to database
    conn = psycopg2.connect(dsn=dsn)
    cur = conn.cursor()

    # Get data source ID
    cur.execute("select image_source_id from image_source "
                "where upper(code) = upper(%s)",
                (image_source,))
    result = cur.fetchone()

    if result is None:
        print("ERROR: image source '{0}' was not found".format(image_source))
        return

    source_id = result[0]

    # Get data type ID
    cur.execute("select image_type_id from image_type "
                "where upper(code) = upper(%s)",
                (image_type_code,))
    result = cur.fetchone()
    type_id = result[0]

    data = {
        "type_id": type_id,
        "source_id": source_id,
        "time_stamp": time_stamp,
        "title": title,
        "description": description,
        "data": psycopg2.Binary(image_data),
        "mime_type": mime_type,
        "metadata": metadata_data
    }

    # Insert video
    try:
        cur.execute(
            """insert into image(image_type_id, image_source_id, time_stamp, title,
                                 description, image_data, mime_type, metadata)
               values(%(type_id)s, %(source_id)s, %(time_stamp)s, %(title)s,
                      %(description)s, %(data)s, %(mime_type)s, %(metadata)s)
            """,
            data
        )
    except psycopg2.errors.UniqueViolation:
        print("ERROR: An image with timestamp {0} already exists for image source {1}".format(time_stamp, image_source))
        conn.close()
        return

    conn.commit()
    cur.close()
    conn.close()

if __name__ == "__main__":
    main()
