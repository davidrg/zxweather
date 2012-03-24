##############################################################################
# cweather QMake project file - WINDOWS ONLY                                 #
##############################################################################
#
# This project file is only intended for use with QtCreator on Windows. The
# include and library paths would have to be changed for it to work anywhere
# else. Additionally, unlike the Makefile it does not build everything.
#
##############################################################################
TEMPLATE = app
CONFIG += console
CONFIG -= qt

# This program is ANSI C.
# Also, be really anal about being ANSI C. No C++ style comments or anything.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra -Werror

# For building pgc sources with ECPG on Windows
ecpg.name = Process Embedded SQL sources
ecpg.input = ECPG_SOURCES
ecpg.output = ${QMAKE_FILE_BASE}.c
ecpg.commands = ../cweather/tools/ecpg-9.1-win32/ecpg.exe -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
ecpg.variable_out = SOURCES
ecpg.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += ecpg

# Because windows has no standard place to store libraries or anything I am
# just going to go and distribute distribute it with the program.
win32:LIBS += -L../cweather/lib/hidapi/windows-binaries -lhidapi
win32:INCLUDEPATH += lib/hidapi/windows-binaries
win32:LIBS += -L../cweather/lib/libecpg-9.1-win32 -lecpg
win32:INCLUDEPATH += lib/libecpg-9.1-win32/include
#win32:LIBS += -L../cweather/lib/libpq-9.0-win32 -lpq
#win32:INCLUDEPATH += lib/libpq-9.0-win32

win32:INCLUDEPATH += src

ECPG_SOURCES += src/pgout.pgc

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
    src/fileout.h \
    src/pgout.h \
    src/version.h
HEADERS += src/common.h
