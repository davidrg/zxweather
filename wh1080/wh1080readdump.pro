TEMPLATE = app
CONFIG += console
CONFIG -= qt

# This program is ANSI C.
# Also, be really anal about being ANSI C. No C++ style comments or anything.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra -Werror

SOURCES += \
    src/readdump.c \
    src/history.c \
    src/deviceconfig.c \
    src/conout.c \
    src/fileio.c

HEADERS += \
    src/history.h \
    src/deviceconfig.h \
    src/conout.h \
    src/deviceio.h
HEADERS += src/common.h

RC_FILE = src/rc/wh1080readdump.rc
