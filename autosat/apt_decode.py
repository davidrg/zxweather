# coding=utf-8
"""
Runs wxtoimg to produce configured outputs for a recorded satellite pass.

This script supports being run directly. Specify a recorded satellite signal
and it will produce the configured output. This allows recordings to be
reprocessed if settings are adjusted.

Note that the FILENAME must be of the form:
    satellitename-aostime.wav
The file must also have the correct timestamp in order for correct output to be
generated.

zxweather autosat
(C) Copyright David Goodwin 2017

License: GNU GPL v2+
"""
import json
import os
import subprocess
import datetime
import re

from log_util import log, bcolors
from output_handler import handle_apt_image, handle_recording, \
    handle_spectrogram
from settings import Settings


def create_overlay(output_filename,
                   aos_time, satellite_name, duration):
    log('Creating Map Overlay...')

    settings = Settings()

    station_coordinates = (
        settings.latitude,
        settings.longitude_negative,
        settings.altitude
    )

    # We need to run wxmap from its directory so it can find wxmap.db
    cwd = os.getcwd()
    os.chdir(settings.wxtoimg_path)

    cmdline = [
        os.path.join(settings.wxtoimg_path, 'wxmap'),
        '-T', satellite_name,  # Satellite to check
        '-G', str(settings.tle_dir),  # TLE file directory
        '-H', str(settings.tle_file_name),  # TLE filename
        '-M', '0',  # Maximum elevation. Passes below this will be ignored
        # '-f', '0',  #  Disable filling of land areas (enabled by default)
        # '-F', '0',  # Disable filling of sea/lake areas (enabled by default)
        '-o',  # Overwrite output file if it already exists
        # '-A', '0',  # Disable anti-aliasing (enabled by default)
        '-O', str(duration),   # Minimum duration after the given start time
                               # during which the satellite is still above the
                               # horizon.
        '-L', "{0}/{1}/{2}".format(*station_coordinates),
        str(int(aos_time) + 1),  # Time of the satellite pass
        output_filename  # Output filename
    ]
    with open(output_filename + '.txt', "w+") as overlay_log:
        subprocess.call(cmdline, stderr=overlay_log, stdout=overlay_log)
        overlay_log.flush()

    os.chdir(cwd)


def make_wxtoimg_cmdline(input_filename, output_filename, enhancement,
                         map_file):
    """
    Generates the wxtoimg command line parameters
    """

    settings = Settings()

    cmdline = [
        os.path.join(settings.wxtoimg_path, 'wxtoimg'),
        ]

    if settings.quiet:
        cmdline.append("-q")

    if settings.text_overlay_enabled:
        cmdline.append("-k {0} - %g %T/%E%p%^%z/e:%e %C".format(
            settings.text_overlay))

    cmdline = cmdline + [
        '-o',  # Overwrite output file if it exists
        '-R1',  # Enable image resync
        '-Q {0}'.format(settings.jpeg_quality)  # JPEG image quality
    ]

    if enhancement is not None:
        if settings.decode_all:
            cmdline.append("-A")

        if enhancement == "pristine":
            # Pristine isn't really an enhancement.
            cmdline.append("-p")
        else:
            # Setup enhancement
            cmdline = cmdline + [
                '-K',  # Use far-IR sensor when no visible sensor is available
                       # for HVC and HVCT
                '-e', enhancement
            ]
    else:
        # For unenhanced images we'll force decoding of a NOAA satellite and
        # try to decode the whole thing (noise included)
        cmdline = cmdline + [
            '-t', 'NOAA',  # Force NOAA satellite
            '-A'  # Try to decode noise
        ]

    if map_file is not None:
        cmdline = cmdline + [
            '-m',
            "{0},{1},{2}".format(map_file, settings.map_overlay_offset_x,
                                 settings.map_overlay_offset_y)
        ]

    cmdline = cmdline + [
        input_filename,
        output_filename
    ]

    return cmdline


def wxtoimg_start_log_msg(enhancement, map_enabled):
    image_type = "basic image"
    if enhancement is not None:
        image_type = "{0} enhancement image".format(enhancement)

    if map_enabled:
        image_type += " with overlay map"

    log('Creating {0}'.format(image_type))


def run_wxtoimg(input_filename, enhancement, map_file,
                max_elevation, satellite_name, aos_time, recording_length,
                no_map=False):
    """
    Runs wxtoimg to generate an image from the satellite recording. The
    resulting image and log files are optionally SCPd or otherwise processed.
    """

    settings = Settings()

    if not settings.map_enabled or no_map:
        map_file = None

    if enhancement is not None and enhancement == "pristine":
        map_file = None  # Pristine output isn't a real enhancement and doesn't
                         # support a map overlay

    # Write out a status message ("Creating image...")
    wxtoimg_start_log_msg(enhancement, settings.map_enabled and not no_map)

    # Figure out output and log filenames
    image_type = "normal"
    if enhancement is not None:
        image_type = enhancement
    if settings.map_enabled:
        image_type += "-map"

    file_name_c = datetime.datetime.fromtimestamp(aos_time).strftime(
        '%Y%m%d-%H%M')
    file_basename = file_name_c + '-' + image_type + '.jpg'

    output_filename = os.path.join(settings.img_dir, satellite_name,
                                   file_basename)
    log_filename = output_filename + ".txt"
    metadata_filename = output_filename + ".json"

    sat_name_no_sp = satellite_name.replace(" ", "")

    log_file = open(log_filename, "w+")
    if enhancement is not None:
        log_file.write('\nEnhancement: {3}, SAT: {0}, Elevation max: {1}, '
                       'Date: {2}\n'
                       .format(sat_name_no_sp, max_elevation, aos_time,
                               enhancement))
    else:
        log_file.write('\nSAT: {0}, Elevation max: {1}, Date: {2}\n'.format(
            sat_name_no_sp, max_elevation, aos_time))

    azimuth = None
    direction = None

    # Pick out some metadata about the pass from the map generation log file.
    # If we're actually using the map overlay we'll also insert the map log data
    # into the images log file.
    for line in open(os.path.join(settings.map_dir,
                                  '{0}-map.png.txt'.format(aos_time)),
                     "r").readlines():

        if line.startswith("Azimuth"):
            azimuth = int(line.split(":")[1].strip())
        elif line.startswith("Direction"):
            direction = line.split(":")[1].strip()
        elif max_elevation is None and line.startswith("Elevation"):
            max_elevation = int(line.split(":")[1].strip())

        if settings.map_enabled:
            # Only insert the map generation log data into the image log file
            # if we're actually using the map
            res = line.replace("\n", " \n")
            log_file.write(res)

    if not settings.map_enabled:
        map_file = None

    cmdline = make_wxtoimg_cmdline(
        input_filename,
        output_filename,
        enhancement,
        map_file
    )

    subprocess.call(cmdline, stderr=log_file, stdout=log_file)
    log_file.close()

    solar_elevation_out_of_range = False
    no_telemetry = False
    narrow_if_bandwidth = False
    enhancement_ignored = False

    for line in open(log_filename, "r").readlines():
        # If solar elevation is too low the output doesn't get generated
        if line.startswith("wxtoimg: warning: solar elevation") \
                and " was less than " in line:
            solar_elevation_out_of_range = True

        # If telemetry data couldn't be found we probably won't have any useful
        # output
        if line.startswith("wxtoimg: warning: couldn't find telemetry data"):
            no_telemetry = True

        # Not enough signal to do anything useful.
        if line.startswith("wxtoimg: warning: Narrow IF bandwidth (purchase "
                           "upgrade key!), low S/N, or volume too high"):
            narrow_if_bandwidth = True

        # Enhancement ignored for some reason
        if line.startswith("wxtoimg: warning: enhancement ignored: "):
            enhancement_ignored = True

        res = line.replace("\n", "")
        res2 = re.sub(r"(\d)", r"\033[96m\1\033[94m", res)
        log(bcolors.OKBLUE + res2)

    # Check that the output was produced properly
    if solar_elevation_out_of_range or no_telemetry or narrow_if_bandwidth or \
            enhancement_ignored:
        if solar_elevation_out_of_range:
            log(bcolors.ENDC + bcolors.RED + "Solar elevation was out of range "
                "- removing '{0}' enhancement output.".format(enhancement))
        elif no_telemetry:
            log(bcolors.ENDC + bcolors.RED +
                "No telemetry data - removing '{0}' enhancement output".format(
                enhancement))
        elif narrow_if_bandwidth:
            log(bcolors.ENDC + bcolors.RED +
                "Narrow IF bandwidth warning. Output likely garbage - "
                "removing '{0}' enhancement output".format(
                    enhancement))
        elif enhancement_ignored:
            log(bcolors.ENDC + bcolors.RED +
                "Enhancement '{0}' ignored (check log) - removing output".format(
                    enhancement))

        # Generated output is (likely) garbage. Trash it.
        os.remove(output_filename)
        return False

    else:
        with open(metadata_filename, "w") as f:
            f.write(json.dumps({
                         "with_map": map_file is not None,
                         "satellite": satellite_name,
                         "max_elevation": max_elevation,
                         "enhancement": enhancement,
                         "aos_time": aos_time,
                         "rec_len": recording_length,
                         "azimuth": azimuth,
                         "direction": direction
                     }))

        # Process the image
        return handle_apt_image(output_filename,
                                log_filename,
                                metadata_filename)


def decode(aos_time, satellite_name, max_elevation, recording_length,
           input_filename):

    settings = Settings()

    log(bcolors.OKBLUE + 'Creating overlay map')
    map_filename = os.path.join(settings.map_dir,
                                '{0}-map.png'.format(aos_time))
    create_overlay(map_filename, aos_time, satellite_name,
                   recording_length)

    # This is created without the overlay map
    output_generated = run_wxtoimg(input_filename, None, None,  max_elevation,
                                   satellite_name, aos_time,
                                   recording_length, True)

    if settings.enhancements_enabled:
        # Handle enhancements
        for enhancement in settings.enhancements:
            result = run_wxtoimg(input_filename, enhancement, map_filename,
                                 max_elevation, satellite_name, aos_time,
                                 recording_length)
            output_generated = output_generated or result

    return output_generated


def main():
    import argparse

    parser = argparse.ArgumentParser()

    parser.add_argument("filename")

    args = parser.parse_args()

    filename = args.filename

    if os.path.isfile(filename):
        process_file(filename)
    elif os.path.isdir(filename):
        # Search the directory for wav files and process them
        for entry in os.listdir(filename):
            pathname = os.path.join(filename, entry)
            if os.path.isfile(pathname) and pathname.endswith(".wav"):
                print(pathname)
                try:
                    process_file(pathname)
                except Exception as e:
                    print("Failed to process {0}".format(pathname))
                    print(e.message)


def process_file(filename):
    import wave
    import contextlib
    # Filename should be SateliteName-AOSTime.wav

    basename = os.path.basename(filename)
    fn = os.path.splitext(basename)[0]

    bits = fn.split("-")
    aos = int(bits[-1])
    satellite = "-".join(bits[0:-1])

    if satellite == "NOAA15":
        satellite = "NOAA 15"
    elif satellite == "NOAA18":
        satellite = "NOAA 18"
    elif satellite == "NOAA19":
        satellite = "NOAA 19"

    with contextlib.closing(wave.open(filename, 'r')) as f:
        frames = f.getnframes()
        rate = f.getframerate()
        duration = frames / float(rate)

    print("Filename: {0}".format(filename))
    print("Satellite: {0}".format(satellite))
    print("AOS Time: {0}".format(aos))
    print("Duration: {0}".format(duration))

    max_elevation = None  # TODO: Where do we get this?

    output_generated = decode(aos, satellite, max_elevation, duration, filename)

    # Only store the recording and spectrogram if there are satellite images
    # to go with them
    if output_generated:
        settings = Settings()
        sat = settings.get_satellite(satellite)
        handle_recording(filename, None, aos, satellite, sat.frequency)

        if settings.make_spectrogram:
            spectro_file = os.path.join(settings.spectrogram_dir,
                                        "{0}-{1}.png".format(
                                            satellite.replace(" ", ""),
                                            aos
                                        ))
            if os.path.exists(spectro_file):
                handle_spectrogram(spectro_file, aos, satellite, None)


if __name__ == "__main__":
    main()
