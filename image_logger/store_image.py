# coding=utf-8
"""
A simple program to store an image in the database.
"""
import argparse
import imghdr
import mimetypes
from datetime import datetime

import psycopg2


def main():
    parser = argparse.ArgumentParser(description="Loads an image into a weather database")
    parser.add_argument("image_file",
                        type=argparse.FileType('r+b'),
                        help="Name of file to load")
    parser.add_argument("image_source",
                        type=str,
                        help="Image source code")
    parser.add_argument("dsn",
                        type=str,
                        help="Database connection string")
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

    # Load metadata if we have any
    metadata_file = args.metadata_file
    metadata_data = None
    if metadata_file is not None:
        metadata_data = metadata_file.read()

    # Try to guess the MIME type if the user hasn't specified one
    mime_type = args.mime_type
    if mime_type is None:
        # noinspection PyBroadException
        try:
            image_type = imghdr.what(args.image_file.name)
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
                (args.image_source,))
    result = cur.fetchone()
    source_id = result[0]

    # Get data type ID
    cur.execute("select image_type_id from image_type "
                "where upper(code) = upper(%s)",
                (args.image_type_code,))
    result = cur.fetchone()
    type_id = result[0]

    data = {
        "type_id": type_id,
        "source_id": source_id,
        "time_stamp": args.time_stamp,
        "title": args.title,
        "description": args.description,
        "data": psycopg2.Binary(args.image_file.read()),
        "mime_type": mime_type,
        "metadata": metadata_data
    }

    # Insert image
    cur.execute(
        """insert into image(image_type_id, image_source_id, time_stamp, title,
                             description, image_data, mime_type, metadata)
           values(%(type_id)s, %(source_id)s, %(time_stamp)s, %(title)s,
                  %(description)s, %(data)s, %(mime_type)s, %(metadata)s)
        """,
        data
    )

    conn.commit()
    cur.close()
    conn.close()

if __name__ == "__main__":
    main()
