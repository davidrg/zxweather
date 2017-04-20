# coding=utf-8
"""
Handles storing generated images in the zxweather database

zxweather autosat
(C) Copyright David Goodwin 2017

License: GNU GPL v2+
"""

import imghdr
import json
import mimetypes
import os
import time
from datetime import datetime
from time import strftime

import psycopg2

from settings import Settings

_apt_enhancement_image_type_codes = {
    "BD": "AEBD",
    "CC": "AECC",
    "EC": "AEEC",
    "HE": "AEHE",
    "HF": "AEHF",
    "HVC": "AEHVC",
    "HVC-PRECIP": "AEHCP",
    "HVCT": "AEHVT",
    "HVCT-PRECIP": "AEHVP",
    "JF": "AEJF",
    "JJ": "AEJJ",
    #   "LC":"AELC",        # GOES
    "MB": "AEMB",
    "MCIR": "AEMCI",
    "MD": "AEMD",
    "MSA": "AEMSA",
    "MSA-ANAGLYPH": "AEMAA",
    "MSA-PRECIP": "AEMSP",
    "NO": "AENO",
    "NORMAL": "APTN",
    "TA": "AETA",
    "ZA": "AEZA",
    "ANAGLYPH": "AEANA",
    "BW": "AEBW",
    "CANAGLYPH": "AECAN",
    "CLASS": "AECLS",
    "CONTRAST": "AECNT",
    "HISTEQ": "AEHIS",
    "INVERT": "AEINV",
    "PRISTINE": "APTD",
    "SEA": "AESEA",
    "THERM": "AETHE",
    "VEG": "AEVEG"
}

_recording_image_type_code = "APT"
_spectrogram_image_type_code = "SPEC"


def handle_apt_image(image_file, log_file, metadata_filename):

    if not os.path.isfile(image_file):
        return  # File doesn't exist. Nothing to store.

    settings = Settings()

    if not settings.zxw_store_images:
        return

    with open(metadata_filename, "r") as f:
        metadata_data = f.read()

    metadata = json.loads(metadata_data)

    enhancement = metadata["enhancement"]
    direction = metadata["direction"].title()
    satellite = metadata["satellite"]
    max_elevation = metadata["max_elevation"]
    time_stamp = time.localtime(metadata["aos_time"])

    if enhancement is None:
        enhancement = "normal"

    if enhancement.lower() not in settings.zxw_store_enhancements:
        # Enhancement not in the list of versions to store in the DB.
        return False

    enh_val = enhancement
    if ":" in enh_val:
        # The enhancement could potentially have additional parameters separated
        # by a colon. We want the base name for the enhancement.
        enh_val = enhancement.split(":")[0]

    image_type_code = _apt_enhancement_image_type_codes[enh_val.upper()]

    if enhancement == "pristine":
        enhancement = "unenhanced"
    elif enhancement == "normal":
        enhancement = "{0} enhancement".format(enhancement)
    else:
        enhancement = "{0} false colour enhancement".format(enhancement)

    caption = "{0}, {1} {2} @ {3} elevation, {4}".format(
        enhancement,
        satellite,
        direction,
        max_elevation,
        strftime('%H:%M:%S', time_stamp)
    )

    title = "{0} {1} @ {2}".format(satellite,
                                   enhancement,
                                   strftime('%H:%M:%S', time_stamp))

    store_image(image_file,
                metadata_filename,
                "image/jpeg",
                image_type_code,
                time_stamp,
                title,
                caption)
    return True


def handle_recording(filename, metadata_filename, time_stamp, satellite,
                     frequency):
    settings = Settings()

    if not settings.zxw_store_recording:
        return

    ts = time.localtime(time_stamp)

    frequency = (frequency / 1000.0) / 1000.0  # Convert to MHz

    title = "{0} on {1} MHz at {2}".format(satellite, frequency, strftime('%H:%M:%S', ts))
    description = "{0} recorded broadcast on {1} MHz at {2}".format(
        satellite, frequency, strftime('%H:%M:%S', ts))

    store_image(filename,
                metadata_filename,
                "audio/wav",
                _recording_image_type_code,
                ts,
                title,
                description)


def handle_spectrogram(filename, time_stamp, satellite, metadata_filename):
    settings = Settings()

    if not settings.zxw_store_spectrogram:
        return

    ts = time.localtime(time_stamp)

    title = "Spectrogram for {0} pass at {1}".format(
        satellite, strftime('%H:%M:%S', ts))

    store_image(filename,
                metadata_filename,
                "image/png",
                _spectrogram_image_type_code,
                ts,
                title,
                None)


def store_image(image_file, metadata_file, mime_type, image_type_code,
                time_stamp, title, description, highlight=False):

    settings = Settings()

    dsn = settings.zxw_dsn
    image_source = settings.zxw_image_source_code

    metadata_data = None
    if metadata_file is not None:
        with open(metadata_file, "r") as f:
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

    # Connect to database
    conn = psycopg2.connect(dsn=dsn)
    cur = conn.cursor()

    # Get data source ID
    cur.execute("select image_source_id from image_source "
                "where upper(code) = upper(%s)",
                (image_source,))
    result = cur.fetchone()
    source_id = result[0]

    # Get data type ID
    cur.execute("select image_type_id from image_type where upper(code) = upper(%s)",
                (image_type_code,))
    result = cur.fetchone()
    type_id = result[0]

    with open(image_file, "rb") as f:
        image_data = f.read()

    ts = datetime.fromtimestamp(time.mktime(time_stamp))

    # Check image doesn't already exist. If it does we'll update.


    # (image_source_id, image_type_id, time_stamp)

    cur.execute("select 1 from image where image_source_id = %s "
                "and image_type_id = %s and time_stamp = %s",
                (source_id, type_id, ts))
    exists = cur.fetchone() is not None

    data = {
        "type_id": type_id,
        "source_id": source_id,
        "time_stamp": ts,
        "title": title,
        "description": description,
        "data": psycopg2.Binary(image_data),
        "mime_type": mime_type,
        "metadata": metadata_data
    }

    if exists:
        print("! Image already exists - updating (ts = {0}, typeid = {1}, "
              "srcid = {2}".format(ts, type_id, source_id))

        # Update image
        cur.execute(
            """update image set title = %(title)s,
                                description = %(description)s,
                                image_data = %(data)s,
                                mime_type = %(mime_type)s,
                                 metadata =  %(metadata)s
               where image_type_id = %(type_id)s
                 and image_source_id = %(source_id)s
                 and time_stamp = %(time_stamp)s
            """,
            data
        )
    else:
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

