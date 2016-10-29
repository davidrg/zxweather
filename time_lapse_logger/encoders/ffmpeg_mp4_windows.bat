@echo off

REM zxweather time lapse service
REM ffmpeg encoder script for Windows
REM --------------------------------------------------------------------------
REM   This encoder script can be used to encode the time-lapse video on
REM   windows systems using ffmpeg. Download ffmpeg from http://ffmpeg.org/
REM   and ensure its in your PATH or alter this script below to run ffmpeg
REM   from where ever you installed it.

REM Parameters:
REM  1 - Working directory containing images to convert
REM  2 - Output filename. Create this file in the working directory
REM  3 - Title for the video
REM  4 - Comments/description for the video

REM change into the working directory
cd /D %1

REM Generate the video at 30fps
ffmpeg -r 30 -start_number 000000 -i %%06d.jpg -s 1280x720 -vcodec libx264 -metadata title=%3 -metadata description=%4 -metadata comment=%4 -y %2
