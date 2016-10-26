#!/usr/bin/env bash
# zxweather time lapse service
# Hardware-accelerated encoder script for Linux/Raspberry Pi
# ----------------------------------------------------------------------------
#   This encoder script makes use of the hardware H.264 encoder on the
#   Raspberry Pi allowing all video processing to be done directly on the Pi in
#	under a minute.  It requires the following packages installed:
#		- gstreamer1.0-tools
#		- gstreamer1.0-plugins-base
#		- gstreamer1.0-plugins-good
#		- gstreamer1.0-plugins-bad
#		- gstreamer1.0-omx
#		- libav-tools or ffmpeg
#		- atomicparsley

# Parameters:
#  1 - Working directory containing images to convert
#  2 - Output filename. Create this file in the working directory
#  3 - Title for the video
#  4 - Comments/description for the video

cd $1

# Generate the video in MPEG Transport Stream format using the hardware H.264
# encoder.
gst-launch-1.0 multifilesrc location=%06d.jpg index=1 caps="image/jpeg,framerate=30/1" ! jpegdec ! omxh264enc ! mpegtsmux ! filesink location=$2.ts

# Convert the TS file to an MP4 file. We have to do this as a separate step
# using ffmpeg/avconv instead of using mp4mux above as mp4mux puts a negative 
# start timestamp on which prevents the video from playing in Chrome and
# Firefox.
avconv -i $2.ts -vcodec copy $2

# If your system uses ffmpeg instead of libav, uncomment the following line and
# comment out the avconv line above
#ffmpeg -i $2.ts -vcodec copy $2

# Set file metadata
AtomicParsley $3 --title "$4" --comment "$5" --description "$5" --overWrite

