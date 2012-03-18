TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += main.c \
    deviceconfig.c \
    deviceio.c \
    debug.c \
    dc_conout.c

LIBS += -L../cweather/hidapi -lhidapi

HEADERS += \
    deviceconfig.h \
    deviceio.h \
    debug.h \
    conout.h
HEADERS += common.h
