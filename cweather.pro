TEMPLATE = app
CONFIG += console
CONFIG -= qt

# This program is ANSI C.
# Also, be really anal about being ANSI C. No C++ style comments or anything.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra -Werror

# Because windows has no standard place to store libraries or anything I am
# just going to go and distribute distribute it with the program.
win32:LIBS += -L../cweather/hidapi/windows-binaries -lhidapi
win32:INCLUDEPATH += hidapi/windows-binaries

SOURCES += main.c \
    deviceconfig.c \
    deviceio.c \
    debug.c \
    dc_conout.c

HEADERS += \
    deviceconfig.h \
    deviceio.h \
    debug.h \
    conout.h
HEADERS += common.h
