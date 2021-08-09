# Desktop Client Build Instructions

The Desktop Client is written in C++ and uses few 3rd party libraries beyond the Qt framework.

Version 1.0 supports Qt 4.8 through to Qt 6.1. At the time of writing the Qt 6 port is still very
new and still has some minor UI issues as well as possibly other regressions.

Building with the most recent Qt 5 release available is recommended at this time.

| Qt Version | Required Modules | Required Libraries                 | Notes |
| ---------- | ---------------- | ---------------------------------- | ----- |
| 4.8.x      | QPSQL            | SQLite 3.8.3 or newer, libpq, ECPG | QSQLite must be built with SQLite 3.8.3 or newer for all reports to work. The SDK version is built with SQLite 3.7.7.1. |
| 5.0-5.5    | QtScript, QPSQL  |                                    | |
| 5.6-5.15   | QML, QPSQL       |                                    | |
| 6.0+       | QML, QPSQL       |                                    | Not recommended yet |

The compiler needs to support C++11. The Qt 4.8.7 SDK uses GCC 4.8 which works fine.

## SQL Drivers
At runtime the Qt PostgreSQL SQL driver is required in order to support connecting directly to a database. Some newer Qt SDKs come with this driver pre-built and all you need to supply is a suitable copy of libpq. On older SDKs you may need to build the SQL driver yourself. The Qt documentation provides instructions on how to do this.

On windows assuming you have a prebuilt copy of the SQL driver a copy of libpq is available in the git repo under /desktop/lib. These should be suitable when building zxweather using MinGW. ECPG and the development libraries and headers are also available in here for building with Qt 4.8 under windows.

If a SQL driver and libpq are not available at runtime or ECPG can not be found at build time the Database backend will be disabled. The application will still work but you'll only be able to connect to a weather station via the Web backend.

## Future Support for Qt Versions
Target Qt versions are currently decided based on whats available in the debian and ubuntu repos after the previous release is finished off. Clearly
the last release was a _very_ long time ago given Qt 4.8 is down as a supported platform!

Once 1.0 is finally out of the way support for Qt older than 5.7.1 will likely be dropped as thats the oldest available from
Debian or Ubunutu these days:

| Distro | Version                | Available Qt |
| ------ | ---------------------- | ------------ |
| Debian | stretch (oldstable)    | 5.7.1        |
| Debian | buster (stable)        | 5.11.3       |
| Ubuntu | 18.04LTS (old LTS)     | 5.9.5        |
| Ubuntu | 20.04LTS (current LTS) | 5.12.8       |

Though at this rate it could well be Qt 6 in debian oldstable by the time 1.0 is finished!

## Building on Windows

1. Grab the Qt SDK
2. Clone the repo
3. Open `zxweather\desktop\desktop.pro` in Qt Creator and it run

Or you can produce an easily redistributable build from the command line by doing something like this:

```
set QT_VERSION=5101
set QT_DIR=C:\Qt\5.10.1\mingw53_32\bin
set COMPILER_DIR=C:\Qt\Tools\mingw530_32\bin

set SOURCE_DIR=C:\dev\zxweather
set BUILD_DIR=build-%QT_VERSION%-release
set DIST_DIR=dist-%QT_VERSION%

set PATH=%QT_DIR%;%COMPILER_DIR%
REM ;%PATH%

cd %SOURCE_DIR%

mkdir %BUILD_DIR%
cd %BUILD_DIR%
qmake ../desktop
mingw32-make
cd ..
rmdir /S /Q %DIST_DIR%
mkdir %DIST_DIR%
cd %DIST_DIR%
copy ..\%BUILD_DIR%\release\zxweather.exe
copy ..\desktop\lib\libpq-9.1-win32\libpq.dll
copy ..\desktop\lib\libecpg-9.1-win32\*.dll
copy ..\zxweather-desktop.ini .\zxweather.ini
copy ..\desktop\qtcolorbutton\license.txt LICENSE.qtcolorbutton
copy ..\desktop\qtcolorbutton\lgpl-2.1.txt LICENSE.lgpl-2.1
copy ..\desktop\qtcolorbutton\LGPL_EXCEPTION.txt
copy ..\desktop\charts\qcp\GPL.txt LICENSE.gpl3
copy ..\desktop\reporting\qt-mustache\license.txt LICENSE.qt-mustache
%QT_DIR%\windeployqt zxweather.exe
del sqldrivers\qsqlodbc.dll
del sqldrivers\qsqlmysql.dll
cd ..

```

## Building on Linux
This should be much the same as building on Windows. Grab the desired Qt SDK and use either QtCreator or build it from a terminal using qmake

The one difference is Qt is probably available from your distros repositories so you should be able to skip installing a Qt SDK.


## Building on MacOS X

Never tried. While there is nothing in the application that assumes linux or windows there is sure to be something somewhere that won't work. The procedure ought
to be much the same as Windows and Linux though - the easiest path is likely to grab the Qt SDK and try building it from QtCreator.





