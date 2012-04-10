TEMPLATE = app
CONFIG += console
CONFIG -= qt

# This program is ANSI C.
# Also, be really anal about being ANSI C. No C++ style comments or anything.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra -Werror

# Because windows has no standard place to store libraries or anything I am
# just going to go and distribute distribute it with the program.
win32:LIBS += -L../wh1080/lib/hidapi/windows-binaries -lhidapi
win32:INCLUDEPATH += lib/hidapi/windows-binaries

SOURCES += \
    src/deviceio.c \
    src/debug.c \
    src/memdump.c

HEADERS += \
    src/deviceio.h \
    src/debug.h
HEADERS += src/common.h
