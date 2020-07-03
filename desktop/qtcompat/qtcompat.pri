#
# This contains backports of classes from newer Qt releases
#

lessThan(QT_MAJOR_VERSION,5)|if(equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION,2)) {
    INCLUDEPATH += $$PWD
    DEPENDPATH += $$PWD

    message("Qt v5.1 or earlier: Using bundled QCommandLineParser")
    SOURCES += qtcompat/qcommandlineoption.cpp \
               qtcompat/qcommandlineparser.cpp

    HEADERS += qtcompat/qcommandlineparser.h \
               qtcompat/qcommandlineoption.h
}
