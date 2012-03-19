TEMPLATE = app
CONFIG += console
CONFIG -= qt

# This program is ANSI C.
# Also, be really anal about being ANSI C. No C++ style comments or anything.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra -Werror

# Because windows has no standard place to store libraries or anything I am
# just going to go and distribute distribute it with the program.
win32:LIBS += -L../cweather/lib/hidapi/windows-binaries -lhidapi
win32:INCLUDEPATH += lib/hidapi/windows-binaries

SOURCES += src/main.c \
    src/deviceconfig.c \
    src/deviceio.c \
    src/debug.c \
    src/history.c \
    src/conout.c \
    src/fileout.c

HEADERS += \
    src/deviceconfig.h \
    src/deviceio.h \
    src/debug.h \
    src/conout.h \
    src/history.h \
    src/fileout.h
HEADERS += src/common.h
