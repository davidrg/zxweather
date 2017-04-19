#!/usr/bin/python -u
# -*- coding: utf-8 -*-

"""
Autosat is a component of zxweather for capturing imagery from polar-orbiting
weather satellites. It started out as a rewrite of autowx which itself was a
rewrite of rtlsdr-automated-wxsat-capture. Some autowx (or autowx-based) code
still remains but most has been rewritten from scratch.

Due its origins unlike other parts of zxweather this program generates coloured
log output and does not run as a daemon but rather is intended to be run 
interactively or via something like start-stop-daemon or systemd.

zxweather autosat
(C) Copyright David Goodwin 2017
Portions (C) Chris Lewandowski 2017
Portions (C) Dr Paul Brewer 2014

License: GNU GPL v2+

This module is based on autowx which is (C) Copyright Chris Lewandowski 2017 
and licensed under the MIT License.
https://github.com/cyber-atomus/autowx
"""

import time
import urllib2
from time import strftime

from datetime import datetime

import pypredict
import os
import sys

import apt_decode
from log_util import log, bcolors, Logger, logLineStart
from output_handler import handle_recording, handle_spectrogram
from recording import record_wav
from settings import Settings

settings = Settings()


def write_pid_file():

    if settings.pid_file is None or settings.pid_file == "":
        return  # No PID file

    pid = str(os.getpid())
    if os.path.isfile(settings.pid_file):
        os.unlink(settings.pid_file)

    with open(settings.pid_file, 'w') as pf:
        pf.write(pid)


def writeStatus(freq, aosTime, losTime, losTimeUnix, recordTime, xfName,
                maxElev, status):
    with open(settings.status_file, 'w+') as statFile:
        if status in ('RECORDING'):
            statFile.write("recording;yes;"+str(xfName)+' AOS@'+str(aosTime)+' LOS@'+str(losTime)+' REC@'+str(recordTime)+'s. max el.@'+str(maxElev)+'°')
        elif status in ('DECODING'):
            statFile.write('recording;no;decoding '+str(xfName))
        elif status in ('WAITING'):
            statFile.write('recording;no;'+str(xfName)+' (AOS@'+str(aosTime)+') @'+str(maxElev)+'° elev. max')
        elif status in ('TOOLOW'):
            statFile.write('recording;no;'+str(xfName)+' (AOS@'+str(aosTime)+') too low ('+str(maxElev)+'°), waiting '+str(recordTime)+'s.')


def find_next_pass(max_processing_time):
    satellite_names = [satellite.name for satellite in settings.satellites]

    predictions = [pypredict.aoslos(sat,
                                    settings.minimum_elevation,
                                    settings.latitude,
                                    settings.longitude,
                                    settings.altitude,
                                    settings.tle_file)
                   for sat in satellite_names]

    aos_times = [p[0] for p in predictions]
    next_aos_index = aos_times.index(min(aos_times))

    satellite = settings.get_satellite(satellite_names[next_aos_index])
    local_aos_time = strftime('%H:%M:%S',
                              time.localtime(predictions[next_aos_index][0]))

    log("selected " + bcolors.YELLOW +
        satellite.name + bcolors.OKGREEN + " @ " + bcolors.CYAN +
        local_aos_time + bcolors.OKGREEN + " as next pass.")

    # We've found the next satellite pass. Now we need to check to see if there
    # are any other satellite passes that overlap with this one that may
    # provide a better signal due to a higher maximum elevation.

    for idx, prediction in enumerate(predictions):
        if idx == next_aos_index:
            pass  # Its already our current best choice for the next satellite

        current_prediction = predictions[next_aos_index]
        current_max_elevation = current_prediction[3]
        current_end_time = current_prediction[1] + max_processing_time

        this_max_elevation = prediction[3]
        this_aos_time = prediction[0]
        if this_max_elevation > current_max_elevation \
                and this_aos_time < current_end_time:

            # This satellite will come in range before we've finished dealing
            # with the one we've currently chosen. And, due to its higher max
            # elevation, it will probably give a better signal too. We'll pick
            # this satellite instead.

            next_aos_index = idx

            satellite = settings.get_satellite(satellite_names[next_aos_index])
            local_aos_time = strftime('%H:%M:%S',
                                      time.localtime(
                                          predictions[next_aos_index][0]))

            log("Switching to overlapping " + bcolors.YELLOW +
                satellite.name + bcolors.OKGREEN + " @ " + bcolors.CYAN +
                local_aos_time + bcolors.OKGREEN + " as next pass due to "
                                                   "higher max elevation.")

            # We'll only switch to another satellite once. If a third satellite
            # happens to overlap this one with even better elev, well, tough.
            # If we switch again then we'd have to reconsider our original
            # choice as it may no longer overlap with our nth choice.
            break

    return satellite, predictions[next_aos_index]


def update_keps(tle_file, update_url):
    """
    Updates the TLE set if its more than 24 hours out of date.

    :param tle_file: TLE file to update
    :param update_url: URL to fetch a newer version if the tle file is out of date
    """

    if update_url == "":
        return  # No update url? no update.

    tle_out_of_date = False
    old_tle_file = tle_file + "-old"

    if not os.path.isfile(tle_file):
        tle_out_of_date = True
        log("TLE file missing")
    else:
        mtime = datetime.fromtimestamp(os.path.getmtime(tle_file))
        log("TLE file last updated at " + bcolors.CYAN +
            mtime.strftime("%H:%M:%S %d-%b-%Y") + bcolors.OKGREEN)

        age = datetime.now() - mtime
        if age.days >= 1:
            log("TLE set is {2}{0}{3} days {2}{1}{3} hours old".format(
                age.days, age.seconds / 3600, bcolors.CYAN, bcolors.OKGREEN))
            tle_out_of_date = True

    if tle_out_of_date:
        log("TLE update required!")
        log("Updating TLE from " + update_url)

        # Delete the old TLE file if it exists
        if os.path.isfile(tle_file):
            os.rename(tle_file, old_tle_file)

        success = False

        try:
            response = urllib2.urlopen(update_url)

            http_code = response.getcode()

            if http_code == "200":
                log(bcolors.ENDC + bcolors.RED +
                    "TLE Update failed with HTTP code {0}!".format(http_code))

            tle = response.read()

            with open(tle_file, 'wb') as f:
                f.write(tle)

            success = True
        except Exception as ex:
            log(bcolors.ENDC + bcolors.RED +
                "TLE Update failed with exception!")
            log(ex.message)

        if success:
            log("TLE set updated.")
            if os.path.isfile(old_tle_file):
                os.remove(old_tle_file)
        else:
            log(bcolors.ENDC + bcolors.RED +
                "TLE set has *NOT* been updated. Calculated satellite passes "
                "may not be correct.")
            if os.path.isfile(tle_file):
                os.remove(tle_file)
            if os.path.isfile(old_tle_file):
                os.rename(old_tle_file, tle_file)


def main():
    write_pid_file()

    if settings.enable_logging:
        sys.stdout = Logger(settings.log_file)

    max_processing_time = 0

    while True:
        update_keps(settings.tle_file, settings.tle_update_url)

        (satellite, (aosTime, losTime, duration, maxElev)) = find_next_pass(
            max_processing_time)

        now = time.time()
        towait = aosTime-now

        aosTimeCnv=strftime('%H:%M:%S', time.localtime(aosTime))
        # emergeTimeUtc=strftime('%Y-%m-%dT%H:%M:%S', time.gmtime(aosTime))
        losTimeCnv=strftime('%H:%M:%S', time.localtime(losTime))
        # dimTimeUtc=strftime('%Y-%m-%dT%H:%M:%S', time.gmtime(losTime))

        # Do we need to wait for the satellite to come in range?
        if towait > 0:
            log("waiting " + bcolors.CYAN + str(towait).split(".")[0] +
                bcolors.OKGREEN + " seconds  (" + bcolors.CYAN + aosTimeCnv +
                bcolors.OKGREEN + " to " + bcolors.CYAN + losTimeCnv + ", " +
                str(duration) + bcolors.OKGREEN + "s.) for " + bcolors.YELLOW +
                satellite.name + bcolors.OKGREEN + " @ " + bcolors.CYAN +
                str(maxElev) + bcolors.OKGREEN + "° el. ")

            writeStatus(satellite.frequency, aosTimeCnv, losTimeCnv, aosTime,
                        towait, satellite.name, maxElev, 'WAITING')

            time.sleep(towait)

        if aosTime < now:
            # Satellite is already in range
            recordTime = losTime - now
            if recordTime < 1:
                recordTime = 1
        elif aosTime >= now:
            recordTime = duration
            if recordTime < 1:
                recordTime = 1

        log("Beginning pass of " + bcolors.YELLOW + satellite.name +
            bcolors.OKGREEN + " at " + bcolors.CYAN + str(maxElev) + "°" +
            bcolors.OKGREEN + " elev.\n" + logLineStart +
            "Predicted start " + bcolors.CYAN + aosTimeCnv + bcolors.OKGREEN +
            " and end " + bcolors.CYAN + losTimeCnv + bcolors.OKGREEN +
            ".\n" + logLineStart + "Will record for " +
            bcolors.CYAN + str(recordTime).split(".")[0] + bcolors.OKGREEN +
            " seconds.")

        writeStatus(satellite.frequency, aosTimeCnv, losTimeCnv, str(losTime),
                    str(recordTime).split(".")[0], satellite.name, maxElev,
                    'RECORDING')

        start_time = time.time()

        wav_filename, wav_metadata_filename, spectrogram_filename = record_wav(
            settings.rec_dir,
            satellite.name,
            str(aosTime),
            satellite.frequency,
            duration,
            satellite.bandwidth,
            settings.wav_sample_rate,
            not settings.keep_raw_recording,
            settings.make_spectrogram,
            settings.spectrogram_dir,
            settings.crush_spectrogram)


        # Decode NOAA APT transmissions
        if satellite.decoder == "apt":
            log("Decoding data")
            writeStatus(satellite.frequency, aosTimeCnv, losTimeCnv,
                        str(losTime), str(recordTime).split(".")[0],
                        satellite.name, maxElev, 'DECODING')

            # Hand the recording off to wxtoimg for decoding
            output_generated = apt_decode.decode(aosTime, satellite.name,
                                                 maxElev, recordTime,
                                                 wav_filename)
            if output_generated:
                handle_recording(wav_filename,
                                 wav_metadata_filename,
                                 aosTime,
                                 satellite.name,
                                 satellite.frequency)

                if spectrogram_filename is not None:
                    handle_spectrogram(spectrogram_filename,
                                       aosTime,
                                       satellite.name,
                                       wav_metadata_filename)

        elif satellite.decoder == "none":
            log("Not decoding signal")

        end_time = time.time()
        processing_time = start_time - end_time

        if processing_time < max_processing_time:
            max_processing_time = processing_time

        # Cap the processing time to 10 minutes.
        if max_processing_time > 600:
            max_processing_time = 600

        wait_time = 10.0
        log("Finished pass of " +
            bcolors.YELLOW +satellite.name + bcolors.OKGREEN + " at " +
            bcolors.CYAN + losTimeCnv + bcolors.OKGREEN + ". Sleeping for" +
            bcolors.CYAN + " " + str(int(wait_time)) +
            bcolors.OKGREEN + " seconds")
        time.sleep(wait_time)

if __name__ == "__main__":
    main()