#-------------------------------------------------
#
# Project created by QtCreator 2012-06-23T00:07:02
#
#-------------------------------------------------

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = desktop
TEMPLATE = app

# For building pgc sources with ECPG. For UNIX systems it assumes ecpg is
# installed in the path.
ecpg.name = Process Embedded SQL sources
ecpg.input = ECPG_SOURCES
ecpg.output = ${QMAKE_FILE_BASE}.cpp
win32:ecpg.commands = ../desktop/tools/ecpg-9.1-win32/ecpg.exe -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
unix: ecpg.commands = ecpg -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
ecpg.variable_out = SOURCES
ecpg.dependency_type = TYPE_CXX
QMAKE_EXTRA_COMPILERS += ecpg

win32 { # Use bundled postgres libraries
    LIBS += -L../desktop/lib/libecpg-9.1-win32 -lecpg
    LIBS += -L../desktop/lib/libpq-9.1-win32 -lpq
    INCLUDEPATH += lib/libecpg-9.1-win32/include
}
unix { # Assumes libecpg and libpq have been installed in /usr/lib
    LIBS += -L/usr/lib -lpq
    LIBS += -L/usr/lib -lecpg
    INCLUDEPATH += /usr/include/postgresql
}

ECPG_SOURCES += database.pgc

SOURCES += main.cpp\
        mainwindow.cpp \
    settingsdialog.cpp \
    dbsignaladapter.cpp \
    aboutdialog.cpp \
    json/json.cpp \
    settings.cpp \
    livedatawidget.cpp \
    qcp/qcustomplot.cpp \
    chartwindow.cpp \
    datasource/abstractdatasource.cpp \
    datasource/webdatasource.cpp \
    chartoptionsdialog.cpp \
    qtcolorbutton/qtcolorbutton.cpp \
    datasource/databasedatasource.cpp \
    datasource/tcplivedatasource.cpp \
    statuswidget.cpp \
    forecastwidget.cpp \
    livemonitor.cpp \
    datasource/webcachedb.cpp \
    datasource/sampleset.cpp \
    exportdialog.cpp

HEADERS  += mainwindow.h \
    database.h \
    settingsdialog.h \
    dbsignaladapter.h \
    aboutdialog.h \
    json/json.h \
    settings.h \
    livedatawidget.h \
    qcp/qcustomplot.h \
    chartwindow.h \
    datasource/abstractdatasource.h \
    datasource/webdatasource.h \
    constants.h \
    chartoptionsdialog.h \
    qtcolorbutton/qtcolorbutton.h \
    qtcolorbutton/qtcolorbutton_p.h \
    datasource/databasedatasource.h \
    datasource/abstractlivedatasource.h \
    datasource/tcplivedatasource.h \
    statuswidget.h \
    forecastwidget.h \
    livemonitor.h \
    datasource/webcachedb.h \
    datasource/sampleset.h \
    exportdialog.h \
    datasource/samplecolumns.h

FORMS    += mainwindow.ui \
    settingsdialog.ui \
    aboutdialog.ui \
    chartwindow.ui \
    chartoptionsdialog.ui \
    livedatawidget.ui \
    statuswidget.ui \
    forecastwidget.ui \
    exportdialog.ui

OTHER_FILES += \
    database.pgc \
    desktop.rc

RESOURCES += \
    resources.qrc

RC_FILE += desktop.rc
