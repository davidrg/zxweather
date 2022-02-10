# coding=utf-8
"""
A simple program to store an image in the database.
"""
import argparse
import imghdr
import mimetypes
import json
from datetime import datetime

import psycopg2
import psycopg2.errors


def main():
    parser = argparse.ArgumentParser(description="Loads an image into a weather database")
    parser.add_argument("dsn",
                        type=str,
                        help="Database connection string")
    parser.add_argument("image_file",
                        type=argparse.FileType('r+b'),
                        help="Name of file to load (or name of image description json file)")
    parser.add_argument("--image-source",
                        type=str,
                        help="Image source code")
    parser.add_argument("--time-stamp",
                        type=str,
                        dest="time_stamp",
                        default=datetime.now().isoformat(),
                        help="Timestamp for the image. The current time will "
                             "be used if not specified.")
    parser.add_argument("--image-type-code",
                        type=str,
                        dest="image_type_code",
                        default='cam',
                        help="Type code for the image. Camera will be used if "
                             "none is specified.")
    parser.add_argument("--title",
                        type=str,
                        dest="title",
                        default=None,
                        help="Title for the image")
    parser.add_argument("--description",
                        type=str,
                        dest="description",
                        help="Description for the image")
    parser.add_argument("--mime-type",
                        type=str,
                        dest="mime_type",
                        default=None,
                        help="MIME type for the image. If not specified an "
                             "attempt will be made to detect it.")
    parser.add_argument("--metadata-file",
                        type=argparse.FileType("r"),
                        dest="metadata_file",
                        default=None,
                        help="JSON Document containing metadata")

    args = parser.parse_args()

    image_file = args.image_file
    image_file_name = image_file.name
    if image_file_name.lower().endswith(".json"):
        # Description file
        desc = json.loads(image_file.read())
        metadata_file = desc["metadata"]
        mime_type = desc["mime"]
        time_stamp = desc["time"]
        title = desc["title"]
        description = desc["description"]
        image_source = desc["source"]
        image_type_code = desc["type"]
        image_file = open(desc["image"], "rb")
    else:
        image_file = args.image_file
        metadata_file = args.metadata_file
        mime_type = args.mime_type
        time_stamp = args.time_stamp
        title = args.title
        description = args.description
        image_source = args.image_source
        image_type_code = args.image_type_code

    if image_source is None:
        print("Error: Image source required (--image-source parameter)")
        return

    # Load metadata if we have any
    metadata_data = None
    if metadata_file is not None:
        metadata_data = metadata_file.read()

    # Try to guess the MIME type if the user hasn't specified one
    if mime_type is None:
        # noinspection PyBroadException
        try:
            image_type = imghdr.what(image_file)
            mime_type = mimetypes.guess_type("foo.{0}".format(image_type))[0]
        except:
            print("Failed to guess MIME type for image file. Manually specify "
                  "MIME type with --mime-type parameter.")
            return

    # Connect to database
    conn = psycopg2.connect(dsn=args.dsn)
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
    print("Image source {0} ID {1}".format(image_source, source_id))

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
        "data": psycopg2.Binary(image_file.read()),
        "mime_type": mime_type,
        "metadata": metadata_data
    }

    # Insert image
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
