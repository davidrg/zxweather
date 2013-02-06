#-------------------------------------------------
#
# Project created by QtCreator 2012-06-23T00:07:02
#
#-------------------------------------------------

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = desktop
TEMPLATE = app

# For building pgc sources with ECPG on Windows
ecpg.name = Process Embedded SQL sources
ecpg.input = ECPG_SOURCES
ecpg.output = ${QMAKE_FILE_BASE}.cpp
ecpg.commands = ../desktop/tools/ecpg-9.1-win32/ecpg.exe -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
ecpg.variable_out = SOURCES
ecpg.dependency_type = TYPE_CXX
QMAKE_EXTRA_COMPILERS += ecpg

win32:LIBS += -L../desktop/lib/libecpg-9.1-win32 -lecpg
win32:LIBS += -L../desktop/lib/libpq-9.1-win32 -lpq
win32:INCLUDEPATH += lib/libecpg-9.1-win32/include

ECPG_SOURCES += database.pgc

SOURCES += main.cpp\
        mainwindow.cpp \
    settingsdialog.cpp \
    dbsignaladapter.cpp \
    aboutdialog.cpp \
    json/json.cpp \
    settings.cpp \
    livedata/livedatawidget.cpp \
    qcp/qcustomplot.cpp \
    chartwindow.cpp \
    datasource/abstractdatasource.cpp \
    datasource/webdatasource.cpp \
    chartoptionsdialog.cpp \
    qtcolorbutton/qtcolorbutton.cpp \
    datasource/databasedatasource.cpp

HEADERS  += mainwindow.h \
    database.h \
    settingsdialog.h \
    dbsignaladapter.h \
    aboutdialog.h \
    json/json.h \
    settings.h \
    livedata/livedatawidget.h \
    qcp/qcustomplot.h \
    chartwindow.h \
    datasource/abstractdatasource.h \
    datasource/webdatasource.h \
    constants.h \
    chartoptionsdialog.h \
    qtcolorbutton/qtcolorbutton.h \
    qtcolorbutton/qtcolorbutton_p.h \
    datasource/databasedatasource.h \
    datasource/abstractlivedatasource.h

FORMS    += mainwindow.ui \
    settingsdialog.ui \
    aboutdialog.ui \
    chartwindow.ui \
    chartoptionsdialog.ui

OTHER_FILES += \
    database.pgc \
    desktop.rc

RESOURCES += \
    resources.qrc

RC_FILE += desktop.rc
