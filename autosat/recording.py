# coding=utf-8
"""
Handles running rtl_fm to produce recordings of satellite passes. Uses sox to
produce spectrograms.

zxweather autosat
(C) Copyright David Goodwin, 2017
Portions (C) Chris Lewandowski 2017

License: GNU GPL v2+

This module is loosely based on autowx which is 
(C) Copyright Chris Lewandowski 2017 and licensed under the MIT License.
https://github.com/cyber-atomus/autowx
"""

import json
import os
import subprocess

import time

from log_util import bcolors, logLineStart, log
from settings import Settings


def _run_for_duration(cmdline, duration):
    try:
        child = subprocess.Popen(cmdline)
        time.sleep(duration)
        child.terminate()
    except OSError as e:
        print("OS Error during command: "+" ".join(cmdline))
        print("OS Error: " + str(e.strerror))


def record_fm(freq, duration, output_file, bandwidth):
    print(bcolors.GRAY)

    settings = Settings()

    cmdline = [
        '/usr/bin/rtl_fm',
        '-f', str(freq),  # Frequency
        '-s', str(bandwidth),  # Sample rate
        '-g', str(settings.gain),  # Gain
        '-F', '9',  # Experimental: enable low-leakage downsample filter
        '-l', '0',  # Squelch level (0 = off)
        '-t', '900',  # Experimental: squelch delay (positive to mute/scan)
        '-A', 'fast',  # Experimental: use fast atan math
        '-E', 'offset',  # Enable offset tuning
        '-p', str(settings.ppm_error),  # ppm error
        output_file
    ]
    _run_for_duration(cmdline, duration)


def resample(input_filename, output_filename, bandwidth, output_sample_rate):

    print(logLineStart+'Transcoding...'+bcolors.YELLOW)
    cmdlinesox = [
        'sox',
        '-t', 'raw',  # File type
        '-r', str(bandwidth),
        '-es',
        '-b', '16',  # 16 bit samples
        '-c', '1',  # Single channel (mono)
        '-V1',  # Verbosity
        input_filename,
        output_filename,
        'rate', str(output_sample_rate)  # Set output sample rate
    ]
    subprocess.call(cmdlinesox)

    cmdlinetouch = [
        'touch',
        '-r' + input_filename,
        output_filename
    ]
    subprocess.call(cmdlinetouch)


def spectrogram(wav_file, satellite_name, timestamp, output_directory, crush):
    log('Creating flight spectrogram')

    # The file we're going to return
    filename = os.path.join(output_directory,
                            "{0}-{1}.png".format(
                               satellite_name.replace(" ", ""),
                               timestamp
                            ))

    # Figure out the name of the file to generate.
    if crush:
        input_filename = os.path.join(output_directory,
                                      "{0}-{1}-original.png".format(
                                          satellite_name.replace(" ", ""),
                                          timestamp
                                      ))
    else:
        input_filename = filename

    cmdline = [
        'sox',
        wav_file,
        '-n', 'spectrogram',
        '-o', filename
    ]
    subprocess.call(cmdline)

    if crush:
        cmdline = [
            'pngcrush',
            input_filename,
            filename
        ]
        subprocess.call(cmdline)
        os.remove(input_filename)

    return filename


def record_wav(output_directory, satellite_name, time_stamp, freq, duration,
               bandwidth, output_sample_rate, remove_raw,
               create_spectro, spectro_output_directory, crush_spectro):
    base_name = os.path.join(output_directory,
                             satellite_name.replace(" ", "") + '-' + time_stamp)
    raw_filename = base_name + ".raw"
    wav_filename = base_name + ".wav"
    metadata_filename = base_name + ".json"
    with open(metadata_filename, "w") as f:
        f.write(json.dumps(
            {
                "satellite": satellite_name,
                "timestamp": time_stamp,
                "frequency": freq,
                "duration": duration,
                "bandwidth": bandwidth
            }
        ))

    record_fm(freq, duration, raw_filename, bandwidth)
    resample(raw_filename, wav_filename, bandwidth, output_sample_rate)

    if remove_raw:
        log(bcolors.ENDC+bcolors.RED+'Removing RAW data')
        os.remove(raw_filename)

    spectro_file = None

    if create_spectro:
        spectro_file = spectrogram(wav_filename, satellite_name, time_stamp,
                                   spectro_output_directory, crush_spectro)

    return wav_filename, metadata_filename, spectro_file
