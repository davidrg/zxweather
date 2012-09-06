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
# The getopt code has some warnings so we can't treat warnings as errors though.
QMAKE_CFLAGS += -ansi -pedantic -Wall -Wextra

# For building pgc sources with ECPG on Windows
ecpg.name = Process Embedded SQL sources
ecpg.input = ECPG_SOURCES
ecpg.output = ${QMAKE_FILE_BASE}.c
ecpg.commands = ../wh1080/tools/ecpg-9.1-win32/ecpg.exe -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
ecpg.variable_out = SOURCES
ecpg.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += ecpg

# Because windows has no standard place to store libraries or anything I am
# just going to go and distribute distribute it with the program.
win32:LIBS += -L../wh1080/lib/hidapi/windows-binaries -lhidapi
win32:INCLUDEPATH += lib/hidapi/windows-binaries
win32:LIBS += -L../wh1080/lib/libecpg-9.1-win32 -lecpg
win32:INCLUDEPATH += lib/libecpg-9.1-win32/include

win32:INCLUDEPATH += src

ECPG_SOURCES += src/pgout.pgc

SOURCES += src/deviceconfig.c \
    src/deviceio.c \
    src/history.c \
    src/daemon.c \
    src/daemon_test.c

HEADERS += \
    src/deviceconfig.h \
    src/deviceio.h \
    src/history.h \
    src/pgout.h \
    src/version.h \
    src/daemon.h
HEADERS += src/common.h

# Windows dosn't have getopt so we bundle a copy of the glibc version.
win32:SOURCES += src/getopt/getopt.c
win32: HEADERS += src/getopt/getopt.h

OTHER_FILES += \
    ../todo.txt \
    Makefile \
    ../database/database.sql \
    src/pgout.pgc

RC_FILE = src/rc/wh1080dtest.rc
