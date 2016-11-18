#!/usr/bin/env bash
# zxweather time lapse service
# ffmpeg encoder script for Linux
# ----------------------------------------------------------------------------
#   This encoder script can be used to encode the time-lapse video on linux
#   using ffmpeg with a resolution of 1280x720. You may wish to alter the
#   encoder settings to target a different resolution or a lower bit rate.
#
#   If your distribution uses libav instead, just replace ffmpeg with avconv
#   below.
#
#   Note for Raspberry Pi users: hardware accelerated encoding is available
#   via omx_mp4.sh and will perform much better than this script.
#

# Parameters:
#  1 - Working directory containing images to convert
#  2 - Output filename. Create this file in the working directory
#  3 - Title for the video
#  4 - Comments/description for the video

# Change into working directory
cd $1

# Generate the video at 30fps
ffmpeg -r 30 -start_number 000000 -i %06d.jpg -s 1280x720 -metadata title="$3" -metadata description="$4" -metadata comment="$4" -vcodec libx264 -b:v 1800k -y $2
