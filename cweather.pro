TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += main.c \
    deviceconfig.c \
    deviceio.c \
    debug.c

LIBS += -L../cweather/hidapi -lhidapi

HEADERS += \
    deviceconfig.h \
    deviceio.h \
    debug.h
HEADERS += common.h
