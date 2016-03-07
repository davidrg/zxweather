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
    charts/qcp/qcustomplot.cpp \
    charts/chartwindow.cpp \
    datasource/abstractdatasource.cpp \
    datasource/webdatasource.cpp \
    charts/chartoptionsdialog.cpp \
    qtcolorbutton/qtcolorbutton.cpp \
    datasource/databasedatasource.cpp \
    datasource/tcplivedatasource.cpp \
    statuswidget.cpp \
    forecastwidget.cpp \
    livemonitor.cpp \
    datasource/webcachedb.cpp \
    datasource/sampleset.cpp \
    exportdialog.cpp \
    dbutil.cpp \
    charts/addgraphdialog.cpp \
    charts/basicqcpinteractionmanager.cpp \
    charts/weatherplotter.cpp \
    config_wizard/configwizard.cpp \
    config_wizard/intropage.cpp \
    config_wizard/accesstypepage.cpp \
    config_wizard/databasedetailspage.cpp \
    config_wizard/selectstationpage.cpp \
    config_wizard/internetsiteinfopage.cpp \
    config_wizard/confirmdetailspage.cpp \
    config_wizard/serverstationlister.cpp \
    config_wizard/serverdetailspage.cpp \
    charts/customisechartdialog.cpp \
    charts/graphstyle.cpp \
    charts/cachemanager.cpp \
    datasource/samplecolumns.cpp \
    charts/graphstyledialog.cpp \
    charts/datasettimespandialog.cpp \
    timespanwidget.cpp \
    aggregateoptionswidget.cpp

HEADERS  += mainwindow.h \
    database.h \
    settingsdialog.h \
    dbsignaladapter.h \
    aboutdialog.h \
    json/json.h \
    settings.h \
    livedatawidget.h \
    charts/qcp/qcustomplot.h \
    charts/chartwindow.h \
    datasource/abstractdatasource.h \
    datasource/webdatasource.h \
    constants.h \
    charts/chartoptionsdialog.h \
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
    dbutil.h \
    exportdialog.h \
    datasource/samplecolumns.h \
    charts/addgraphdialog.h \
    charts/basicqcpinteractionmanager.h \
    charts/weatherplotter.h \
    config_wizard/configwizard.h \
    config_wizard/intropage.h \
    config_wizard/configwizard_private.h \
    config_wizard/accesstypepage.h \
    config_wizard/databasedetailspage.h \
    config_wizard/selectstationpage.h \
    config_wizard/internetsiteinfopage.h \
    config_wizard/confirmdetailspage.h \
    config_wizard/serverstationlister.h \
    config_wizard/serverdetailspage.h \
    charts/customisechartdialog.h \
    charts/graphstyle.h \
    charts/chartsettings.h \
    charts/cachemanager.h \
    datasource/aggregate.h \
    charts/graphstyledialog.h \
    charts/datasettimespandialog.h \
    timespanwidget.h \
    aggregateoptionswidget.h

FORMS    += mainwindow.ui \
    settingsdialog.ui \
    aboutdialog.ui \
    charts/chartwindow.ui \
    charts/chartoptionsdialog.ui \
    livedatawidget.ui \
    statuswidget.ui \
    forecastwidget.ui \
    exportdialog.ui \
    charts/addgraphdialog.ui \
    charts/customisechartdialog.ui \
    charts/graphstyledialog.ui \
    charts/datasettimespandialog.ui \
    timespanwidget.ui \
    aggregateoptionswidget.ui

OTHER_FILES += \
    database.pgc \
    desktop.rc

RESOURCES += \
    resources.qrc

RC_FILE += desktop.rc
