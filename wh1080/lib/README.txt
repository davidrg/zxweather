This directory contains 3rd-party libraries and code required to build this
software. Some of it is only used on Windows as that platform has no standard
way of handling libraries.

hidapi\linux-0.7
   v0.7 of HID-API for Linux. This is used to communicate with the weather
   station using libusb.
   
hidapi\windows-binaries
   A version of HID-API from 8/22/2009. Included binaries were built with
   MinGW. The source code may have been modified at some point to work properly
   with MinGW (I believe this is not the unmodified 8/22/2009 release).
   Heritage unknown.
   
libecpg-9.1-win32
   libecpg libraries and headers for Win32 compiled with MinGW. Built from the
   PostgreSQL 9.1.2 sources. Used only for windows builds as there is no
   standard place for these to live.
   
