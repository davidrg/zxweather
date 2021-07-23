#-------------------------------------------------
#
# Project created by QtCreator 2012-06-23T00:07:02
#
#-------------------------------------------------

greaterThan(QT_MAJOR_VERSION, 4): load(configure)

QT       += core gui network sql svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport concurrent uitools

lessThan(QT_MAJOR_VERSION, 5): CONFIG += uitools

# QML is currently used only for QJsEngine support
equals(QT_MAJOR_VERSION, 5):!lessThan(QT_MINOR_VERSION,6) {
    message("Using QJSEngine for scripting")
    QT += qml
    DEFINES += USE_QJSENGINE
} else {
    message("Using QtScript for scripting")
    QT += script
}

TARGET = zxweather
TEMPLATE = app


######################
# Build Settings     #
######################
DEFINES += SINGLE_INSTANCE   # Only allow one instance per station code


######################
# Libraries          #
######################
contains(DEFINES, SINGLE_INSTANCE) {
    include(qtsingleapplication/src/qtsingleapplication.pri)
}

include(qtcompat/qtcompat.pri)

######################
# Multimedia support #
######################
greaterThan(QT_MAJOR_VERSION, 4) {
    qtCompileTest(multimedia)

    config_multimedia {
        QT += multimedia multimediawidgets
        message("Multimedia support enabled")
    }

    !config_multimedia {
        message("Multimedia support disabled")
        DEFINES += NO_MULTIMEDIA
    }
} else {
    QT += phonon
    message("Unable to test for multimedia support under Qt 4 - assuming its available and enabling.")
}

#################################
# ECPG support for DB Live Data #
#################################
# Try to find the ECPG binary. If it can't be found we'll disable database live data support.
win32 {
    #ECPG_BIN = $$system(where ecpg)
    #isEmpty(ECPG_BIN) {
    #    # No system ECPG, use repository version
    #    ECPG_BIN = ../desktop/tools/ecpg-9.1-win32/ecpg.exe
    #}

    ECPG_BIN = ../desktop/tools/ecpg-9.1-win32/ecpg.exe
}
unix|mac {
    ECPG_BIN = $$system(which ecpg)
}

isEmpty(ECPG_BIN) {
    message("ECPG Not found. Database support disabled.")
    DEFINES += NO_ECPG
}

!isEmpty(ECPG_BIN) {
    message("Database support enabled - found ECPG in " $$ECPG_BIN)

    # For building pgc sources with ECPG. For UNIX systems it assumes ecpg is
    # installed in the path.
    ecpg.name = Process Embedded SQL sources
    ecpg.input = ECPG_SOURCES
    ecpg.output = ${QMAKE_FILE_BASE}.cpp
    #win32:ecpg.commands = ../desktop/tools/ecpg-9.1-win32/ecpg.exe -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    #unix: ecpg.commands = ecpg -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    ecpg.commands = $$ECPG_BIN -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    ecpg.variable_out = SOURCES
    ecpg.dependency_type = TYPE_CXX
    QMAKE_EXTRA_COMPILERS += ecpg

    win32 { # Use bundled postgres libraries
        LIBS += -L../desktop/lib/libecpg-9.1-win32 -lecpg
        LIBS += -L../desktop/lib/libpq-9.1-win32 -lpq
        #INCLUDEPATH += $$PWD/lib/libecpg-9.1-win32/include
        INCLUDEPATH += lib/libecpg-9.1-win32/include
    }
    unix { # Assumes libecpg and libpq have been installed in /usr/lib
        LIBS += -L/usr/lib -lpq
        LIBS += -L/usr/lib -lecpg
        INCLUDEPATH += /usr/include/postgresql
    }

    ECPG_SOURCES += database.pgc
}

SOURCES += main.cpp\
    applock.cpp \
    charts/axistickformatdialog.cpp \
    charts/datetimeformathelpdialog.cpp \
    charts/plotwidget/abstractaxistag.cpp \
    #charts/plotwidget/axisrangeannotation.cpp \
    charts/plotwidget/basicaxistag.cpp \
    charts/plotwidget/chartmousetracker.cpp \
    charts/plotwidget/pluscursor.cpp \
    charts/plotwidget/tracingaxistag.cpp \
    columnpickerwidget.cpp \
    livecolumnpickerwidget.cpp \
        mainwindow.cpp \
    reporting/jsconsole.cpp \
    samplecolumnpickerwidget.cpp \
    settingsdialog.cpp \
    dbsignaladapter.cpp \
    aboutdialog.cpp \
    json/json.cpp \
    settings.cpp \
    dash_widgets/livedatawidget.cpp \
    charts/qcp/qcustomplot.cpp \
    charts/chartwindow.cpp \
    datasource/abstractdatasource.cpp \
    datasource/webdatasource.cpp \
    charts/chartoptionsdialog.cpp \
    qtcolorbutton/qtcolorbutton.cpp \
    datasource/databasedatasource.cpp \
    datasource/tcplivedatasource.cpp \
    dash_widgets/statuswidget.cpp \
    dash_widgets/forecastwidget.cpp \
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
    charts/graphstyle.cpp \
    charts/cachemanager.cpp \
    datasource/samplecolumns.cpp \
    charts/graphstyledialog.cpp \
    charts/datasettimespandialog.cpp \
    timespanwidget.cpp \
    aggregateoptionswidget.cpp \
    urlhandler.cpp \
    viewdataoptionsdialog.cpp \
    viewdatasetwindow.cpp \
    datasetmodel.cpp \
    viewimageswindow.cpp \
    imagemodel.cpp \
    imagewidget.cpp \
    datasource/webtasks/fetchsampleswebtask.cpp \
    datasource/webtasks/activeimagesourceswebtask.cpp \
    datasource/webtasks/abstractwebtask.cpp \
    datasource/webtasks/rangerequestwebtask.cpp \
    datasource/webtasks/datafilewebtask.cpp \
    datasource/webtasks/selectsampleswebtask.cpp \
    datasource/webtasks/latestimageswebtask.cpp \
    datasource/webtasks/fetchimagewebtask.cpp \
    datasource/webtasks/fetchthumbnailwebtask.cpp \
    datasource/webtasks/fetchimagedatelistwebtask.cpp \
    datasource/webtasks/listdayimageswebtask.cpp \
    video/abstractvideoplayer.cpp \
    dash_widgets/imagestabwidget.cpp \
    dash_widgets/rainfallwidget.cpp \
    datasource/webtasks/fetchraintotalswebtask.cpp \
    datasource/datasourceproxy.cpp \
    weatherimagewindow.cpp \
    imagepropertiesdialog.cpp \
    datasource/imageset.cpp \
    datasource/abstractprogresslistener.cpp \
    datasource/dialogprogresslistener.cpp \
    datasource/nullprogresslistener.cpp \
    dash_widgets/weathervaluewidget.cpp \
    unit_conversions.cpp \
    charts/liveplot.cpp \
    charts/liveplotwindow.cpp \
    charts/addlivegraphdialog.cpp \
    charts/abstractliveaggregator.cpp \
    charts/nonaggregatingliveaggregator.cpp \
    charts/averagedliveaggregator.cpp \
    charts/livechartoptionsdialog.cpp \
    charts/livedatarepeater.cpp \
    charts/axistag.cpp \
    reporting/report.cpp \
    reporting/qt-mustache/mustache.cpp \
    reporting/runreportdialog.cpp \
    reporting/reportdisplaywindow.cpp \
    datasource/webtasks/fetchstationinfo.cpp \
    charts/datasetsdialog.cpp \
    datasource/webtasks/cachingfinishedwebtask.cpp \
    sortproxymodel.cpp \
    charts/plotwidget.cpp \
    reporting/scriptrenderwrapper.cpp \
    reporting/scriptvalue.cpp \
    reporting/reportcontext.cpp \
    reporting/queryresultmodel.cpp \
    reporting/reportfinisher.cpp \
    reporting/reportpartialresolver.cpp \
    reporting/scriptingengine.cpp \
    datasource/livebuffer.cpp

HEADERS  += mainwindow.h \
    abstracturlhandler.h \
    applock.h \
    charts/axistickformatdialog.h \
    charts/datetimeformathelpdialog.h \
    charts/plotwidget/abstractaxistag.h \
    #charts/plotwidget/axisrangeannotation.h \
    charts/plotwidget/axistype.h \
    charts/plotwidget/basicaxistag.h \
    charts/plotwidget/chartmousetracker.h \
    charts/plotwidget/pluscursor.h \
    charts/plotwidget/tracingaxistag.h \
    columnpickerwidget.h \
    database.h \
    datasource/hardwaretype.h \
    livecolumnpickerwidget.h \
    reporting/jsconsole.h \
    samplecolumnpickerwidget.h \
    settingsdialog.h \
    dbsignaladapter.h \
    aboutdialog.h \
    json/json.h \
    settings.h \
    dash_widgets/livedatawidget.h \
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
    dash_widgets/statuswidget.h \
    dash_widgets/forecastwidget.h \
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
    charts/graphstyle.h \
    charts/chartsettings.h \
    charts/cachemanager.h \
    datasource/aggregate.h \
    charts/graphstyledialog.h \
    charts/datasettimespandialog.h \
    timespanwidget.h \
    aggregateoptionswidget.h \
    urlhandler.h \
    viewdataoptionsdialog.h \
    viewdatasetwindow.h \
    datasetmodel.h \
    viewimageswindow.h \
    version.h \
    imagemodel.h \
    imagewidget.h \
    datasource/webtasks/activeimagesourceswebtask.h \
    datasource/webtasks/abstractwebtask.h \
    datasource/webtasks/fetchsampleswebtask.h \
    datasource/webtasks/rangerequestwebtask.h \
    datasource/webtasks/request_data.h \
    datasource/webtasks/datafilewebtask.h \
    datasource/webtasks/selectsampleswebtask.h \
    datasource/webtasks/latestimageswebtask.h \
    datasource/imageset.h \
    datasource/webtasks/fetchimagewebtask.h \
    datasource/webtasks/fetchthumbnailwebtask.h \
    datasource/webtasks/fetchimagedatelistwebtask.h \
    datasource/webtasks/listdayimageswebtask.h \
    video/abstractvideoplayer.h \
    dash_widgets/imagestabwidget.h \
    dash_widgets/rainfallwidget.h \
    datasource/webtasks/fetchraintotalswebtask.h \
    datasource/datasourceproxy.h \
    weatherimagewindow.h \
    imagepropertiesdialog.h \
    datasource/abstractprogresslistener.h \
    datasource/dialogprogresslistener.h \
    datasource/nullprogresslistener.h \
    dash_widgets/weathervaluewidget.h \
    unit_conversions.h \
    charts/liveplot.h \
    charts/liveplotwindow.h \
    charts/addlivegraphdialog.h \
    charts/abstractliveaggregator.h \
    charts/nonaggregatingliveaggregator.h \
    charts/averagedliveaggregator.h \
    charts/livechartoptionsdialog.h \
    charts/livedatarepeater.h \
    charts/axistag.h \
    reporting/report.h \
    reporting/qt-mustache/mustache.h \
    reporting/runreportdialog.h \
    reporting/reportdisplaywindow.h \
    datasource/webtasks/fetchstationinfo.h \
    datasource/station_info.h \
    charts/datasetsdialog.h \
    datasource/webtasks/cachingfinishedwebtask.h \
    reporting/reportfinisher.h \
    sortproxymodel.h \
    charts/plotwidget.h \
    reporting/scriptrenderwrapper.h \
    reporting/scriptvalue.h \
    reporting/reportcontext.h \
    reporting/queryresultmodel.h \
    reporting/reportpartialresolver.h \
    reporting/scriptingengine.h \
    datasource/livebuffer.h

FORMS    += mainwindow.ui \
    charts/axistickformatdialog.ui \
    charts/datetimeformathelpdialog.ui \
    columnpickerwidget.ui \
    settingsdialog.ui \
    aboutdialog.ui \
    charts/chartwindow.ui \
    charts/chartoptionsdialog.ui \
    dash_widgets/livedatawidget.ui \
    dash_widgets/statuswidget.ui \
    dash_widgets/forecastwidget.ui \
    exportdialog.ui \
    charts/addgraphdialog.ui \
    charts/graphstyledialog.ui \
    charts/datasettimespandialog.ui \
    timespanwidget.ui \
    aggregateoptionswidget.ui \
    viewdataoptionsdialog.ui \
    viewdatasetwindow.ui \
    viewimageswindow.ui \
    weatherimagewindow.ui \
    imagepropertiesdialog.ui \
    charts/liveplotwindow.ui \
    charts/addlivegraphdialog.ui \
    charts/livechartoptionsdialog.ui \
    reporting/runreportdialog.ui \
    charts/datasetsdialog.ui

config_multimedia {
    HEADERS += video/videoplayer.h
    SOURCES += video/videoplayer.cpp
    FORMS += video/videoplayer.ui
} else {
    lessThan(QT_MAJOR_VERSION, 5) {
        # Qt 4.8 doesn't support any sort of config testing thats useful
        # here so we just always enable multimedia support and hop
        HEADERS += video/phononvideoplayer.h
        SOURCES += video/phononvideoplayer.cpp
        FORMS += video/phononvideoplayer.ui
    }
}


OTHER_FILES += \
    database.pgc \
    desktop.rc

RESOURCES += \
    resources.qrc \
    reporting/reports/reports.qrc

RC_FILE += desktop.rc

# For translating, do something like this:
# TRANSLATIONS = zxweather.mi_NZ.ts \       # Maori, New Zealand
#                zxweather.fr_FR.ts \       # French, France
#                zxweather.de_DE.ts         # German, Germany
# etc.
