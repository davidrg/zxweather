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
#include "charts/addlivegraphdialog.h"
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
#include <QVariant>

#include "datasource/datasourceproxy.h"
#include "datasource/dialogprogresslistener.h"
#include "datasource/livebuffer.h"

#include "imagewidget.h"

#include "urlhandler.h"
#include "json/json.h"

#ifdef SINGLE_INSTANCE
#include "constants.h"
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);
    ready = false;
    processingMessages = false;

    // The UI is configured for a Davis Vantage Pro 2 Plus by default
    last_hw_type = HW_DAVIS;
    solarDataAvailable = true;
    indoorDataAvailable = true;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    sysTrayIcon.reset(new QSystemTrayIcon(this));
    sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    sysTrayIcon->setToolTip(tr("No data"));
    sysTrayIcon->show();
    connect(sysTrayIcon.data(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    trayIconMenu.reset(new QMenu(this));
    restoreAction.reset(new QAction(tr("&Restore"),trayIconMenu.data()));
    quitAction.reset(new QAction(tr("&Quit"),trayIconMenu.data()));
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
            &LiveBuffer::getInstance(), SLOT(liveData(LiveDataSet)));
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
    connect(dataSource, SIGNAL(liveConnectFailed(QString)),
            this, SLOT(liveDataSourceConnectFailed(QString)));
    connect(dataSource, SIGNAL(liveConnected()),
            this, SLOT(liveConnected()));
    connect(dataSource, SIGNAL(newImage(NewImageInfo)),
            this, SLOT(newImage(NewImageInfo)));
    connect(dataSource, SIGNAL(activeImageSourcesAvailable()),
            this, SLOT(activeImageSourcesAvailable()));
    connect(dataSource, SIGNAL(archivedImagesAvailable()),
            this, SLOT(archivedImagesAvailable()));
    connect(dataSource, SIGNAL(imageReady(ImageInfo,QImage,QString)),
            ui->latestImages, SLOT(imageReady(ImageInfo,QImage,QString)));
    connect(dataSource, SIGNAL(samplesConnectFailed(QString)),
            this, SLOT(samplesDataSourceConnectFailed(QString)));

    Settings& settings = Settings::getInstance();

    connect(&settings, SIGNAL(dataSourceChanged(Settings::DataSourceConfiguration)),
            this, SLOT(dataSourceChanged(Settings::DataSourceConfiguration)));

    qDebug() << "Read settings and connect...";
    readSettings();

    // Timer to check for dropped database connections. This is likely if the
    // PC goes to sleep or wifi is turned off.
    databaseChecker.setInterval(30000); // Check DB Connection every 30 seconds.
    connect(&databaseChecker, SIGNAL(timeout()), this, SLOT(checkDatabase()));

    if (settings.stationCode().isEmpty()) {
        // We're probably migrating settings from v0.1.
        QMessageBox::information(this, tr("Bad configuration"),
                                 tr("The station name has not been configured. You will now be shown the settings dialog."));
        showSettings();
    }
    else {
        LiveBuffer::getInstance().connectStation(settings.stationCode());

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

    urlHandler.reset(new UrlHandler());
}


/* -----------------------------------------------------------------------------------
 * The following should be in the DatabaseDataSource!
 * But it isn't because on database connect failure we've got to turn off a bunch of
 * UI bits. So really we need a connect failed signal on AbstractDataSource. This will
 * probably be done as part of a larger refactoring of the data sources.
 */
bool MainWindow::databaseCompatibilityChecks(bool samples, bool live) {

    using namespace DbUtil;

    QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection);

    DatabaseCompatibility compatibility = checkDatabaseCompatibility(db);

    if (compatibility == DC_BadSchemaVersion) {
        qDebug() << "Bad schema version.";
        QMessageBox::warning(this, tr("Database Error"),
                             tr("Unable to determine database version. "
                             "Archive functions will not be available."));
        disableDataSourceFunctionality(samples, live);
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
            version = QString(tr(" Please upgrade to at least version %1.")).arg(version);
        }

        QMessageBox::warning(this, tr("Database Incompatible"),
                             QString(tr("The configured database is incompatible "
                             "with this version of the zxweather "
                             "desktop client.%1 Database"
                             " functionality will be disabled.")).arg(version));
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        disableDataSourceFunctionality(samples, live);
        return false;
    }
    return true;
}

void MainWindow::checkDatabase() {
    QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);

    qDebug() << "Database check";

    if (!db.isOpen()) {
        qDebug() << "Lost database connection. Beginning reconnect...";
        if (!db.open()) {
            dataSourceError(tr("Failed to reconnect database"));
            return;
        }
        qDebug() << "Reconnected!";

        // Now re-enable live data
        dataSource->enableLiveData();
        dataSource->fetchRainTotals();
    }

    QSqlQuery q;
    if (!q.exec("select 1")) {
        qDebug() << "Lost database connection. Beginning reconnect...";
        if (!db.open()) {
            dataSourceError(tr("Failed to reconnect database"));
            return;
        }
        qDebug() << "Reconnected!";

        // Now re-enable live data
        dataSource->enableLiveData();
        dataSource->fetchRainTotals();
    }
}

void MainWindow::disableDataSourceFunctionality(bool samples, bool live) {
    if (samples) {
        ui->actionCharts->setEnabled(false);
        ui->actionExport_Data->setEnabled(false);
        ui->actionView_Data->setEnabled(false);
        ui->actionImages->setEnabled(false);
        ui->action_Reports->setEnabled(false);
    }

    if (live) {
        ui->actionLive_Chart->setEnabled(false);
    }
}

void MainWindow::enableDataSourceFunctionality(bool samples, bool live) {
    if (samples) {
        ui->actionCharts->setEnabled(true);
        ui->actionExport_Data->setEnabled(true);
        ui->actionView_Data->setEnabled(true);
        ui->actionImages->setEnabled(true);
        ui->action_Reports->setEnabled(true);
    }

    if (live) {
        ui->actionLive_Chart->setEnabled(true);
    }
}

void MainWindow::reconnectDatabase() {
    Settings& settings = Settings::getInstance();

    bool dbLive = settings.liveDataSourceType() == Settings::DS_TYPE_DATABASE;
    bool dbSamples = settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE;

    // Now check if we actually need to connect to a database.
    if (!dbSamples && !dbLive) {
        qDebug() << "Database disabled.";

        // Disconnect from the database if it was connected.
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

        return;
    }

    qDebug() << "Primary database connect...";

    if (QSqlDatabase::drivers().contains("QPSQL")) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");

        if (db.isValid()) {
            db.setHostName(settings.databaseHostName());
            db.setPort(settings.databasePort());
            db.setDatabaseName(settings.databaseName());
            db.setUserName(settings.databaseUsername());
            db.setPassword(settings.databasePassword());

            bool ok = db.open();
            if (!ok) {
                qDebug() << "Connect failed: " + db.lastError().driverText();
                QMessageBox::warning(this, tr("Connect error"),
                                     QString(tr("Failed to connect to the database. Charting and Reporting "
                                     "functions will not be available. The error "
                                     "was: %1").arg(db.lastError().driverText())));
                disableDataSourceFunctionality(dbSamples, dbLive);
            } else {
                qDebug() << "Connect succeeded. Checking compatibility...";
                bool result = databaseCompatibilityChecks(dbSamples, dbLive);
                if (result) {
                    reconfigureDataSource();
                    databaseChecker.start();
                }
            }
        } else {
            QMessageBox::warning(this, tr("Database Driver Error"),
                                 QString(tr("The database driver failed to load. "
                                            "The last error was: %1").arg(
                                             db.lastError().driverText())));
            disableDataSourceFunctionality(dbSamples, dbLive);
        }
    } else {
        qDebug() << QSqlDatabase::drivers();
        QMessageBox::warning(this, tr("Driver not found"),
                             tr("The Qt PostgreSQL database driver was not found."
                             " Unable to connect to database. Charting "
                             " functions will not be available."));
        disableDataSourceFunctionality(dbSamples, dbLive);
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
                QMessageBox::information(this, tr("zxweather"),
                                     tr("zxweather will minimise to the "
                                     "system tray. To restore it, click on the "
                                     "icon. This behaviour can be changed in the "
                                     "settings dialog."));
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

bool MainWindow::showSettings() {
    SettingsDialog sd(solarDataAvailable);

    connect(&sd, SIGNAL(stationCodeChanging(QString)),
            this, SLOT(stationCodeChanging(QString)));

    int result = sd.exec();

    if (result == QDialog::Accepted) {
        readSettings();
        enableDataSourceFunctionality();

        liveMonitor->reconfigure();

        return true;
    }

    return false;
}

void MainWindow::dataSourceChanged(Settings::DataSourceConfiguration newConfig) {
    if (newConfig.liveDataSource != Settings::DS_TYPE_DATABASE
            && newConfig.sampleDataSource != Settings::DS_TYPE_DATABASE) {
        // For the database live data source reconnectDatabase() will handle
        // calling reconfigureDataSource() once the database is ready.
        databaseChecker.stop();
        reconfigureDataSource();
    }

    if (newConfig.liveDataSource == Settings::DS_TYPE_DATABASE
            || newConfig.sampleDataSource == Settings::DS_TYPE_DATABASE) {
        reconnectDatabase();
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
                QMessageBox::information(this, tr("zxweather"),
                                     tr("zxweather will keep running in the "
                                        "system tray. To restore it, click on the "
                                        "icon. To exit, right-click on the system tray "
                                        "icon and choose <b>Exit</b>. This behaviour "
                                        "can be changed from the settings dialog."));
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

void MainWindow::fail() {
    QCoreApplication::exit(1);
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

    ChartOptionsDialog options(solarDataAvailable, last_hw_type, wirelessAvailable,
                               dataSource->extraColumnsAvailable(),
                               dataSource->extraColumnNames());
    int result = options.exec();
    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    SampleColumns columns = options.getColumns();

    DataSet ds;
    ds.columns = columns;
    ds.extraColumnNames = dataSource->extraColumnNames();
    ds.startTime = options.getStartTime();
    ds.endTime = options.getEndTime();
    ds.aggregateFunction = options.getAggregateFunction();
    ds.groupType = options.getAggregateGroupType();
    ds.customGroupMinutes = options.getCustomMinutes();
    QList<DataSet> dataSets;
    dataSets << ds;

    qDebug() << "DS Columns:"<< (int)ds.columns.standard << (int)ds.columns.extra;
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
    AddLiveGraphDialog algd(~(LiveValues)LV_NoColumns,
                            solarDataAvailable,
                            indoorDataAvailable,
                            last_hw_type,
                            this->dataSource->extraColumnsAvailable(),
                            this->dataSource->extraColumnNames(),
                            tr("Select the values to display in the live chart. More can be added "
                               "later."),
                            this);
    algd.setWindowTitle(tr("Choose graphs"));

    if (algd.exec() == QDialog::Accepted) {
        LiveValues selectedGraphs = algd.selectedColumns();
        LivePlotWindow *lpt = new LivePlotWindow(selectedGraphs,
                                                 solarDataAvailable,
                                                 indoorDataAvailable,
                                                 last_hw_type,
                                                 this->dataSource->extraColumnsAvailable(),
                                                 this->dataSource->extraColumnNames());
        lpt->setAttribute(Qt::WA_DeleteOnClose);
        lpt->show();
    }
}

void MainWindow::chartRequested(DataSet dataSet) {
    QList<DataSet> ds;
    ds << dataSet;

    qDebug() << "DS Columns:"<< (int)dataSet.columns.standard << (int)dataSet.columns.extra;
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
    connect(dataSource, SIGNAL(newImage(NewImageInfo)),
            imagesWindow, SLOT(newImage(NewImageInfo)));
}

void MainWindow::viewData() {
    station_info_t info = dataSource->getStationInfo();
    bool wirelessAvailable = false;
    if (info.isValid) {
        wirelessAvailable = info.isWireless;
    }

    QMap<ExtraColumn, QString> extraColumnNames = dataSource->extraColumnNames();

    ViewDataOptionsDialog options(solarDataAvailable, last_hw_type, wirelessAvailable,
                                  dataSource->extraColumnsAvailable(),
                                  extraColumnNames, this);
    int result = options.exec();

    if (result != QDialog::Accepted)
        return; // User canceled. Nothing to do.

    // Always show all columns in the view data screen.
    SampleColumns columns = options.getColumns();

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
    showWarningPopup(message, tr("Error"), "", false);
}

void MainWindow::refreshRainWidget() {
    ui->rainfall->reset();
    dataSource->fetchRainTotals();
}

void MainWindow::reconfigureDataSource() {   
    qDebug() << "Reconfigure data source...";

    ui->actionImages->setVisible(false);
    Settings& settings = Settings::getInstance();
    setWindowTitle(tr("%1 - zxweather").arg(settings.stationCode()));

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

    qDebug() << "Connect data sources";
    dataSource->setDataSourceTypes(liveType, samplesType);
    dataSource->connectDataSources();

    // TODO: This won't work the first time we use the WebDataSource against a station.
    station_info_t info = dataSource->getStationInfo();
    if (info.isValid) {
        ui->status->setTransmitterBatteryVisible(info.isWireless);
        adjustSize();
        setFixedSize(size());
    }

    qDebug() << "Refresh Main UI stuff";
    dataSource->enableLiveData();
    dataSource->fetchRainTotals();
    dataSource->hasActiveImageSources();

    if (settings.liveTimeoutEnabled()) {
        liveMonitor->enable();
    } else {
        liveMonitor->disable();
    }
    ready = true;
    qDebug() << "Ready!";
}

void MainWindow::setStationName(QString name) {
    if (!name.isEmpty()) {
        setWindowTitle(tr("%1 - zxweather").arg(name));
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
    if (lds.hw_type == last_hw_type) {
        return;
    } else {
        qDebug() << "Hardware type changed from" << last_hw_type << "to" << lds.hw_type;
    }

    indoorDataAvailable = lds.indoorDataAvailable;

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
    RunReportDialog *rrd = new RunReportDialog(urlHandler.data());
    rrd->setAttribute(Qt::WA_DeleteOnClose);
    rrd->show();
}

void MainWindow::messageReceived(const QString &parameters) {
    using namespace QtJson;

    QVariantMap args = Json::parse(parameters).toMap();

    QVariantList argsList = args.value("args", QVariantList()).toList();
    foreach(QVariant arg, argsList) {
        QVariantMap argMap = arg.toMap();
        QString name = argMap.value("name").toString();
        QString value = argMap.value("value").toString();

        if (name == "reportPath+") {
            Settings::getInstance().temporarilyAddReportSearchPath(value);
        } else if (name == "reportPath-") {
            Settings::getInstance().removeReportSearchPath(value);
        }
    }

    QVariantList messages = args.value("positional", QVariantList()).toList();

    foreach(QVariant messageV, messages) {
        QString message = messageV.toString();
        qDebug() << "Open request:" << message;
        waitingMessages.append(message);

        if (!processingMessages) {
            processMessages();
        }
    }
}

void MainWindow::processMessages() {

    if (!ready) {
        qDebug() << "Not ready to process messages yet. Retrying soon...";
        QTimer::singleShot(1000, this, SLOT(processMessages()));
        return;
    }

    processingMessages = true;
    while(!waitingMessages.isEmpty()) {
        QString message = waitingMessages.takeFirst();
        // TODO: wait until we're actually ready and connected to the datasource.
        if (!message.isNull()) {
            QUrl url(message);
            if (url.isValid() && url.scheme() == "zxw") {
                station_info_t info = dataSource->getStationInfo();
                bool wirelessAvailable = false;
                if (info.isValid) {
                    wirelessAvailable = info.isWireless;
                }

                urlHandler->handleUrl(url, solarDataAvailable, wirelessAvailable);
            }
        }
    }
    processingMessages = false;
}


void MainWindow::liveDataSourceConnectFailed(QString errorMessage) {
    QMessageBox::warning(
                this, tr("Live data connect failed"),
                QString(tr("An error occurred connecting to the live data source. "
                           "Current conditions and live charts will not be "
                           "available. The error was: %1")).arg(errorMessage));
    disableDataSourceFunctionality(false, true);
}

void MainWindow::samplesDataSourceConnectFailed(QString errorMessage) {
    disableDataSourceFunctionality(true, false);
    QMessageBox::warning(
                this, tr("Sample data connect failed"),
                QString(tr("An error occurred connecting to the sample data source. "
                           "Features relying archive data (charts, export, "
                           "view data, reports) will be unavailable. The error "
                           "was: %1")).arg(errorMessage));
}

void MainWindow::liveConnected() {
    enableDataSourceFunctionality(false, true);
}

#ifdef SINGLE_INSTANCE
void MainWindow::stationCodeChanging(QString newCode) {
    qDebug() << "Station code is changing! Relocking single instance.";
    QString newAppId = Constants::SINGLE_INSTANCE_LOCK_PREFIX + newCode.toLower();
    emit relockSingleInstance(newAppId);

    // Switch the live buffer to the new station code
    LiveBuffer::getInstance().connectStation(newCode);
}
#endif
