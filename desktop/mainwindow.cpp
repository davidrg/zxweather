/*****************************************************************************
 *            Created: 23/06/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"


#include "aboutdialog.h"
#include "settings.h"
#include "charts/chartwindow.h"
#include "charts/chartoptionsdialog.h"
#include "charts/liveplotwindow.h"
#include "exportdialog.h"
#include "viewdataoptionsdialog.h"
#include "viewdatasetwindow.h"
#include "viewimageswindow.h"

#include "dbutil.h"

#include "reporting/runreportdialog.h"

#include <QtDebug>
#include <QDateTime>
#include <QMessageBox>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "datasource/datasourceproxy.h"
#include "datasource/dialogprogresslistener.h"

#include "config_wizard/configwizard.h"

#include "imagewidget.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);

    // The UI is configured for a Davis Vantage Pro 2 Plus by default
    last_hw_type = HW_DAVIS;
    solarDataAvailable = true;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    sysTrayIcon.reset(new QSystemTrayIcon(this));
    sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    sysTrayIcon->setToolTip("No data");
    sysTrayIcon->show();
    connect(sysTrayIcon.data(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    trayIconMenu.reset(new QMenu(this));
    restoreAction.reset(new QAction("&Restore",trayIconMenu.data()));
    quitAction.reset(new QAction("&Quit",trayIconMenu.data()));
    trayIconMenu->addAction(restoreAction.data());
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction.data());
    sysTrayIcon->setContextMenu(trayIconMenu.data());
    connect(restoreAction.data(), SIGNAL(triggered()), this, SLOT(showNormal()));
    connect(quitAction.data(), SIGNAL(triggered()), this, SLOT(quit()));

    // Toolbar
    connect(ui->actionCharts, SIGNAL(triggered()), this, SLOT(showChartWindow()));
    connect(ui->actionLive_Chart, SIGNAL(triggered()), this, SLOT(showLiveChartWindow()));
    connect(ui->actionExport_Data, SIGNAL(triggered()), this, SLOT(showExportDialog()));
    connect(ui->actionImages, SIGNAL(triggered(bool)), this, SLOT(showImagesWindow()));
    connect(ui->actionView_Data, SIGNAL(triggered(bool)), this, SLOT(viewData()));
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
    connect(ui->action_Reports, SIGNAL(triggered()), this, SLOT(showReports()));

    // Live Data Widget
    connect(ui->liveData, SIGNAL(sysTrayIconChanged(QIcon)),
            this, SLOT(updateSysTrayIcon(QIcon)));
    connect(ui->liveData, SIGNAL(sysTrayTextChanged(QString)),
            this, SLOT(updateSysTrayText(QString)));
    connect(ui->liveData, SIGNAL(plotRequested(DataSet)),
            this, SLOT(chartRequested(DataSet)));

    // Rainfall widget
    connect(ui->rainfall, SIGNAL(chartRequested(DataSet)),
            this, SLOT(chartRequested(DataSet)));
    connect(ui->rainfall, SIGNAL(refreshRequested()),
            this, SLOT(refreshRainWidget()));

    // Live data monitor.
    liveMonitor.reset(new LiveMonitor());
    connect(liveMonitor.data(),
            SIGNAL(showWarningPopup(QString,QString,QString,bool)),
            this,
            SLOT(showWarningPopup(QString,QString,QString,bool)));

    // Data Source
    dataSource = new DataSourceProxy(new DialogProgressListener(this), this);
    connect(dataSource, SIGNAL(newSample(Sample)),
            ui->rainfall, SLOT(newSample(Sample)));
    connect(dataSource, SIGNAL(rainTotalsReady(QDate,double,double,double)),
            ui->rainfall, SLOT(setRain(QDate,double,double,double)));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            ui->liveData, SLOT(refreshLiveData(LiveDataSet)));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            ui->rainfall, SLOT(liveData(LiveDataSet)));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            ui->forecast, SLOT(refreshLiveData(LiveDataSet)));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            ui->status, SLOT(refreshLiveData(LiveDataSet)));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            liveMonitor.data(), SLOT(LiveDataRefreshed()));
    connect(dataSource, SIGNAL(liveData(LiveDataSet)),
            this, SLOT(liveDataRefreshed(LiveDataSet)));
    connect(dataSource, SIGNAL(stationName(QString)),
            this, SLOT(setStationName(QString)));
    connect(dataSource, SIGNAL(isSolarDataEnabled(bool)),
            this, SLOT(setSolarDataAvailable(bool)));
    connect(dataSource, SIGNAL(error(QString)),
            this, SLOT(dataSourceError(QString)));
    connect(dataSource, SIGNAL(newImage(NewImageInfo)),
            this, SLOT(newImage(NewImageInfo)));
    connect(dataSource, SIGNAL(activeImageSourcesAvailable()),
            this, SLOT(activeImageSourcesAvailable()));
    connect(dataSource, SIGNAL(archivedImagesAvailable()),
            this, SLOT(archivedImagesAvailable()));
    connect(dataSource, SIGNAL(imageReady(ImageInfo,QImage,QString)),
            ui->latestImages, SLOT(imageReady(ImageInfo,QImage,QString)));

    Settings& settings = Settings::getInstance();

    // Show the configuration wizard on the first run.
    if (!settings.singleShotFirstRun()) {
        ConfigWizard wiz;
        if (wiz.exec() != QDialog::Accepted) {
            // Config wizard was canceled. Show the settings dialog instead.
            showSettings();
        }
        settings.setSingleShotFirstRun();
    }

    qDebug() << "Read settings and connect...";
    readSettings();



    if (settings.stationCode().isEmpty()) {
        // We're probably migrating settings from v0.1.
        QMessageBox::information(this, "Bad configuration", "The station name has not been configured. You will now be shown the settings dialog.");
        showSettings();
    }
    else {
        // This will call reconfigureDataSource on successful connect if the live
        // data source is the database.
        QTimer::singleShot(1, this, SLOT(reconnectDatabase()));

        if (settings.liveDataSourceType() != Settings::DS_TYPE_DATABASE &&
                settings.sampleDataSourceType() != Settings::DS_TYPE_DATABASE) {
            reconfigureDataSource();
        }
    }

    // This will be turned on later if the data source reports there are archived
    // images available.
    ui->actionImages->setVisible(false);

    restoreState(settings.mainWindowState());
    restoreGeometry(settings.mainWindowGeometry());

    // Ensure the latest images panel resizes correctly and maintains
    // aspect ratio.
    QSizePolicy policy(QSizePolicy::MinimumExpanding,
                       QSizePolicy::MinimumExpanding);
    policy.setHeightForWidth(true);
    policy.setVerticalStretch(1);
    ui->latestImages->setSizePolicy(policy);
}


/* -----------------------------------------------------------------------------------
 * The following should be in the DatabaseDataSource!
 * But it isn't because on database connect failure we've got to turn off a bunch of
 * UI bits. So really we need a connect failed signal on AbstractDataSource. This will
 * probably be done as part of a larger refactoring of the data sources.
 */
bool MainWindow::databaseCompatibilityChecks() {

    using namespace DbUtil;

    QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection);

    DatabaseCompatibility compatibility = checkDatabaseCompatibility(db);

    if (compatibility == DC_BadSchemaVersion) {
        qDebug() << "Bad schema version.";
        QMessageBox::warning(this, tr("Database Error"),
                             tr("Unable to determine database version. "
                             "Archive functions will not be available."));
        ui->actionCharts->setEnabled(false);
        ui->actionExport_Data->setEnabled(false);
        ui->actionView_Data->setEnabled(false);
        ui->actionImages->setEnabled(false);
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        return false;
    } else if (compatibility == DC_Unknown) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("Unable to determine database compatibility."
                             " This application may not function "
                             "correctly with the configured database."));
        return false;
    } else if (compatibility == DC_Incompatible) {
        QString version = getMinimumAppVersion(db);
        if (!version.isNull()) {
            // This will only work on a v2+ schema (zxweather v0.2+)
            version = tr(" Please upgrade to at least version ")
                    + version + ".";
        }

        QMessageBox::warning(this, tr("Database Incompatible"),
                             tr("The configured database is incompatible "
                             "with this version of the zxweather "
                             "desktop client.") + version + tr(" Database"
                             " functionality will be disabled."));
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        ui->actionCharts->setEnabled(false);
        return false;
    }
    return true;
}

void MainWindow::reconnectDatabase() {
    Settings& settings = Settings::getInstance();

    // Just in case the database connection failed (causing it to be disabled)
    // and then the user switched to the web data source:
    ui->actionCharts->setEnabled(true);

    // Now check if we actually need to connect to a database.
    if (settings.sampleDataSourceType() != Settings::DS_TYPE_DATABASE &&
            settings.liveDataSourceType() != Settings::DS_TYPE_DATABASE) {
        qDebug() << "Database disabled.";

        // Disconnect from the database if it was connected.
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

        return;
    }

    qDebug() << "Primary database connect...";

    if (QSqlDatabase::drivers().contains("QPSQL")) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
        db.setHostName(settings.databaseHostName());
        db.setPort(settings.databasePort());
        db.setDatabaseName(settings.databaseName());
        db.setUserName(settings.databaseUsername());
        db.setPassword(settings.databasePassword());

        bool ok = db.open();
        if (!ok) {
            qDebug() << "Connect failed: " + db.lastError().driverText();
            QMessageBox::warning(this, "Connect error",
                                 "Failed to connect to the database. Charting "
                                 "functions will not be available. The error "
                                 "was: " + db.lastError().driverText());
            ui->actionCharts->setEnabled(false);
            ui->actionExport_Data->setEnabled(false);
            ui->actionView_Data->setEnabled(false);
            ui->actionImages->setEnabled(false);
        } else {
            qDebug() << "Connect succeeded. Checking compatibility...";
            bool result = databaseCompatibilityChecks();
            if (result) {
                reconfigureDataSource();
            }
        }
    } else {
        qDebug() << QSqlDatabase::drivers();
        QMessageBox::warning(this, "Driver not found",
                             "The Qt PostgreSQL database driver was not found."
                             " Unable to connect to database. Charting "
                             " functions will not be available.");
        ui->actionCharts->setEnabled(false);
        ui->actionExport_Data->setEnabled(false);
        ui->actionView_Data->setEnabled(false);
        ui->actionImages->setEnabled(false);
    }
}
// ------------------ ^^^ This should be in DatabaseDataSource ^^^ ------------------


MainWindow::~MainWindow()
{
    sysTrayIcon->hide();
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    case QEvent::WindowStateChange:
        if (windowState().testFlag(Qt::WindowMinimized) &&
                minimise_to_systray) {

            if (!Settings::getInstance().singleShotMinimiseToSysTray()) {
                QMessageBox::information(this, "zxweather",
                                     "zxweather will minimise to the "
                                     "system tray. To restore it, click on the "
                                     "icon. This behaviour can be changed in the "
                                     "settings dialog.");
                Settings::getInstance().setSingleShotMinimiseToSysTray();
            }

            // We can't call hide from the event handler. So we get the
            // timer to dispatch the hide request for us.
            QTimer::singleShot(0, this, SLOT(hide()));
        }
        break;
    default:
        break;
    }
}

void MainWindow::showSettings() {
    SettingsDialog sd(solarDataAvailable);
    int result = sd.exec();

    if (result == QDialog::Accepted) {
        readSettings();

        liveMonitor->reconfigure();

        Settings& settings = Settings::getInstance();

        if (settings.liveDataSourceType() != Settings::DS_TYPE_DATABASE
                && settings.sampleDataSourceType() != Settings::DS_TYPE_DATABASE) {
            // For the database live data source reconnectDatabase() will handle
            // calling reconfigureDataSource() once the database is ready.
            reconfigureDataSource();
        }

        if (settings.liveDataSourceType() == Settings::DS_TYPE_DATABASE
                || settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE) {
            reconnectDatabase();
        }
    }
}

void MainWindow::showWarningPopup(QString message, QString title, QString tooltip, bool setWarningIcon) {
    if (!tooltip.isEmpty())
        sysTrayIcon->setToolTip(tooltip);
    if (setWarningIcon)
        sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    if (!message.isEmpty())
        sysTrayIcon->showMessage(title, message, QSystemTrayIcon::Warning);
}

void MainWindow::clearWarningPopup() {
    sysTrayIcon->setIcon(normalSysTrayIcon);
}

void MainWindow::readSettings() {
    Settings& settings = Settings::getInstance();
    minimise_to_systray = settings.miniseToSysTray();
    close_to_systray = settings.closeToSysTray();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    switch(reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::MiddleClick:
    case QSystemTrayIcon::DoubleClick:
        showNormal();
        activateWindow();
    default:
        ;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (sysTrayIcon->isVisible()) {
        if (close_to_systray) {
            if (!Settings::getInstance().singleShotCloseToSysTray()) {
                QMessageBox::information(this, "zxweather",
                                     "zxweather will keep running in the "
                                        "system tray. To restore it, click on the "
                                        "icon. To exit, right-click on the system tray "
                                        "icon and choose <b>Exit</b>. This behaviour "
                                        "can be changed from the settings dialog.");
                Settings::getInstance().setSingleShotCloseToSysTray();
            }
            hide();
            event->ignore();
        } else {
            QCoreApplication::quit();
        }
    }
    Settings::getInstance().saveMainWindowState(saveState());
    Settings::getInstance().saveMainWindowGeometry(saveGeometry());
}

void MainWindow::quit() {
    QCoreApplication::quit();
}

void MainWindow::showAbout() {
    AboutDialog ad;
    ad.exec();
}

void MainWindow::showChartWindow() {

    station_info_t info = dataSource->getStationInfo();
    bool wirelessAvailable = false;
    if (info.isValid) {
        wirelessAvailable = info.isWireless;
    }

    ChartOptionsDialog options(solarDataAvailable, last_hw_type, wirelessAvailable);
    int result = options.exec();
    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    SampleColumns columns = options.getColumns();

    DataSet ds;
    ds.columns = columns;
    ds.startTime = options.getStartTime();
    ds.endTime = options.getEndTime();
    ds.aggregateFunction = options.getAggregateFunction();
    ds.groupType = options.getAggregateGroupType();
    ds.customGroupMinutes = options.getCustomMinutes();
    QList<DataSet> dataSets;
    dataSets << ds;

    qDebug() << "DS Columns:"<< (int)ds.columns;
    qDebug() << "Start" << ds.startTime;
    qDebug() << "End" << ds.endTime;
    qDebug() << "AGFunc" << ds.aggregateFunction;
    qDebug() << "AGGrp" << ds.groupType;
    qDebug() << "AGMin" << ds.customGroupMinutes;

    ChartWindow *cw = new ChartWindow(dataSets, solarDataAvailable, wirelessAvailable);
    cw->setAttribute(Qt::WA_DeleteOnClose);
    cw->show();
}

void MainWindow::showLiveChartWindow() {
    LivePlotWindow *lpt = new LivePlotWindow(solarDataAvailable,
                                             last_hw_type);
    lpt->setAttribute(Qt::WA_DeleteOnClose);
    lpt->show();
}

void MainWindow::chartRequested(DataSet dataSet) {
    QList<DataSet> ds;
    ds << dataSet;

    qDebug() << "DS Columns:"<< (int)dataSet.columns;
    qDebug() << "Start" << dataSet.startTime;
    qDebug() << "End" << dataSet.endTime;
    qDebug() << "AGFunc" << dataSet.aggregateFunction;
    qDebug() << "AGGrp" << dataSet.groupType;
    qDebug() << "AGMin" << dataSet.customGroupMinutes;


    station_info_t info = dataSource->getStationInfo();
    bool wirelessAvailable = false;
    if (info.isValid) {
        wirelessAvailable = info.isWireless;
    }

    ChartWindow *cw = new ChartWindow(ds, solarDataAvailable, wirelessAvailable);
    cw->setAttribute(Qt::WA_DeleteOnClose);
    cw->show();
}

void MainWindow::showExportDialog() {
    station_info_t info = dataSource->getStationInfo();
    bool wirelessAvailable = false;
    if (info.isValid) {
        wirelessAvailable = info.isWireless;
    }

    ExportDialog dialog(solarDataAvailable, wirelessAvailable, last_hw_type);
    dialog.exec();
}

void MainWindow::showImagesWindow() {
    ViewImagesWindow *imagesWindow = new ViewImagesWindow();
    imagesWindow->setAttribute(Qt::WA_DeleteOnClose);
    imagesWindow->show();
}

void MainWindow::viewData() {
    ViewDataOptionsDialog options(this);
    int result = options.exec();

    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    // Always show all columns in the view data screen.
    SampleColumns columns = ALL_SAMPLE_COLUMNS;

    if (last_hw_type != HW_DAVIS) {
        columns = columns & ~DAVIS_COLUMNS;
    }

    if (!solarDataAvailable) {
        // Except the solar ones if the current station doesn't support it
        columns = columns & ~SOLAR_COLUMNS;
    }

    DataSet dataSource;
    dataSource.columns = columns;
    dataSource.startTime = options.getStartTime();
    dataSource.endTime = options.getEndTime();
    dataSource.aggregateFunction = options.getAggregateFunction();
    dataSource.groupType = options.getAggregateGroupType();
    dataSource.customGroupMinutes = options.getCustomMinutes();

    ViewDataSetWindow *window = new ViewDataSetWindow(dataSource);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->show();
}

void MainWindow::updateSysTrayText(QString text) {
    sysTrayIcon->setToolTip(text);
}

void MainWindow::updateSysTrayIcon(QIcon icon) {
    normalSysTrayIcon = icon;
    sysTrayIcon->setIcon(icon);
}

void MainWindow::dataSourceError(QString message) {
    showWarningPopup(message, "Error", "", false);
}

void MainWindow::refreshRainWidget() {
    ui->rainfall->reset();
    dataSource->fetchRainTotals();
}

void MainWindow::reconfigureDataSource() {   

    ui->actionImages->setVisible(false);
    Settings& settings = Settings::getInstance();
    setWindowTitle("zxweather - " + settings.stationCode());

    // hide image tabs
    ui->latestImages->hideImagery();
    adjustSize();
    setFixedSize(size());

    ui->rainfall->reset();

    // Reset late data timer.
    ui->status->reset();

    DataSourceProxy::LiveDataSourceType liveType;
    switch(settings.liveDataSourceType()) {
    case Settings::DS_TYPE_DATABASE:
        liveType = DataSourceProxy::LDST_DATABASE;
        break;
    case Settings::DS_TYPE_SERVER:
        liveType = DataSourceProxy::LDST_TCP;
        break;
    case Settings::DS_TYPE_WEB_INTERFACE:
    default:
        liveType = DataSourceProxy::LDST_WEB;
        break;
    }

    DataSourceProxy::DataSourceType samplesType;
    switch(settings.sampleDataSourceType()) {
    case Settings::DS_TYPE_DATABASE:
        samplesType = DataSourceProxy::DST_DATABASE;
        break;
    case Settings::DS_TYPE_WEB_INTERFACE:
    default:
        samplesType = DataSourceProxy::DST_WEB;
        break;
    }
    dataSource->setDataSourceTypes(liveType, samplesType);
    dataSource->connectDataSources();

    // TODO: This won't work the first time we use the WebDataSource against a station.
    station_info_t info = dataSource->getStationInfo();
    if (info.isValid) {
        ui->status->setTransmitterBatteryVisible(info.isWireless);
        adjustSize();
        setFixedSize(size());
    }

    dataSource->enableLiveData();
    dataSource->fetchRainTotals();
    dataSource->hasActiveImageSources();

    if (settings.liveTimeoutEnabled()) {
        liveMonitor->enable();
    } else {
        liveMonitor->disable();
    }
}

void MainWindow::setStationName(QString name) {
    if (!name.isEmpty()) {
        setWindowTitle("zxweather - " + name);
    }
}

void MainWindow::setSolarDataAvailable(bool available) {
    solarDataAvailable = available;

    ui->liveData->setSolarDataAvailable(solarDataAvailable);
    //QTimer::singleShot(10, this, SLOT(adjustSizeSlot()));
}

void MainWindow::liveDataRefreshed(LiveDataSet lds) {

    clearWarningPopup();

    // If the hardware type hasn't changed then there isn't anything to do.
    if (lds.hw_type == last_hw_type) return;

    ui->forecast->setVisible(lds.hw_type == HW_DAVIS);
    ui->status->setVisible(lds.hw_type == HW_DAVIS);
    ui->rainfall->setStormRateEnabled(lds.hw_type == HW_DAVIS);
    ui->latestImages->setFixedWidth(width());

    // Adjust the size after a short delay to give the other widgets time to
    // adjust their size. This is required otherwise extra blank space in
    // the main window from widgets being hidden stays around. An immediate
    // adjustSize() here doesn't do the job.
    QTimer::singleShot(500, this, SLOT(adjustSizeSlot()));

    last_hw_type = lds.hw_type;
}

void MainWindow::activeImageSourcesAvailable() {
    dataSource->fetchLatestImages();
    setFixedSize(width(), QWIDGETSIZE_MAX);
    adjustSize();
}

void MainWindow::archivedImagesAvailable() {
    ui->actionImages->setVisible(true);
}

void MainWindow::newImage(NewImageInfo imageInfo) {
    qDebug() << "newImage available" << imageInfo.imageId;
    dataSource->fetchImage(imageInfo.imageId);
}

void MainWindow::adjustSizeSlot() {
    adjustSize();
    if (!ui->latestImages->isVisible()) {
        setFixedSize(size());
    }
}

void MainWindow::showReports() {
    RunReportDialog *rrd = new RunReportDialog();
    rrd->setAttribute(Qt::WA_DeleteOnClose);
    rrd->show();
}
