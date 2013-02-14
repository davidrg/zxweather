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
#include "chartwindow.h"

#include <QtDebug>
#include <QDateTime>
#include <QMessageBox>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);

    // Make the window a fixed size.
    setFixedSize(width(),height());
    statusBar()->setSizeGripEnabled(false);

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
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));

    // Live Data Widget
    connect(ui->liveData, SIGNAL(warning(QString,QString,QString,bool)),
            this, SLOT(showWarningPopup(QString,QString,QString,bool)));
    connect(ui->liveData, SIGNAL(sysTrayIconChanged(QIcon)),
            this, SLOT(updateSysTrayIcon(QIcon)));
    connect(ui->liveData, SIGNAL(sysTrayTextChanged(QString)),
            this, SLOT(updateSysTrayText(QString)));

    // Show the settings dialog on the first run.
    if (!Settings::getInstance().singleShotFirstRun()) {
        showSettings();
        Settings::getInstance().setSingleShotFirstRun();
    }
    qDebug() << "Read settings and connect...";
    readSettings();

    if (Settings::getInstance().stationCode().isEmpty()) {
        // We're probably migrating settings from v0.1.
        QMessageBox::information(this, "Bad configuration", "The station name has not been configured. You will now be shown the settings dialog.");
        showSettings();
    }
    else {
        reconnectDatabase();
        ui->liveData->reconfigureDataSource();
    }
}

int MainWindow::getDatabaseVersion() {
    QSqlQuery query("select * "
                    "from information_schema.tables "
                    "where table_schema = 'public' "
                    "  and table_name = 'db_info'");
    if (!query.isActive()) {
        return -1;
    } else if (query.size() == 1){
        // It is at least a v2 (zxweather 0.2) schema.
        query.exec("select v::integer "
                   "from DB_INFO "
                   "where k = 'DB_VERSION'");
        if (!query.isActive() || query.size() != 1) {
            return -1;
        } else {
            query.first();
            return query.value(0).toInt();
        }
    }

    return 1;
}

void MainWindow::databaseCompatibilityChecks() {
    int version = getDatabaseVersion();
    qDebug() << "Schema version:" << version;
    if (version == -1) {
        qDebug() << "Bad schema version.";
        QMessageBox::warning(this, "Database Error",
                             "Unable to determine database version. "
                             "Charting functions will not be available.");
        ui->actionCharts->setEnabled(false);
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    } else if (version > 1) {
        qDebug() << "V2+ database.";

        // Check that this version of the desktop client hasn't been
        // blacklisted by the database.
        QSqlQuery query("select version_check('desktop',1,0,0)");
        if (!query.isActive()) {
            QMessageBox::warning(this, "Warning",
                                 "Unable to determine database compatibility."
                                 " This application may not function "
                                 "correctly with the configured database.");
        } else {
            query.first();
            if (!query.value(0).toBool()) {

                QString version = "";
                query.exec("select minimum_version_string('desktop')");
                if (query.isActive()) {
                    query.first();
                    version = " Please upgrade to at least version " +
                            query.value(0).toString() + ".";
                }

                QMessageBox::warning(this, "Database Incompatible",
                                     "The configured database is incompatible "
                                     "with this version of the zxweather "
                                     "desktop client." + version + " Database"
                                     " functionality will be disabled.");
                QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
                ui->actionCharts->setEnabled(false);
            }
        }
    }
}

void MainWindow::reconnectDatabase() {
    Settings& settings = Settings::getInstance();

    // Just in case the database connection failed (causing it to be disabled)
    // and then the user switched to the web data source:
    ui->actionCharts->setEnabled(true);

    // Now check if we actually need to connect to a database.
    if (settings.sampleDataSourceType() != Settings::DS_TYPE_DATABASE) {
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
        } else {
            qDebug() << "Connect succeeded. Checking compatibility...";
            databaseCompatibilityChecks();
        }
    } else {
        qDebug() << QSqlDatabase::drivers();
        QMessageBox::warning(this, "Driver not found",
                             "The Qt PostgreSQL database driver was not found."
                             " Unable to connect to database. Charting "
                             " functions will not be available.");
        ui->actionCharts->setEnabled(false);
    }
}


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
                Settings::getInstance().singleShotMinimiseToSysTray();
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
    SettingsDialog sd;
    int result = sd.exec();

    if (result == QDialog::Accepted) {
        readSettings();

        ui->liveData->reconfigureDataSource();
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
}

void MainWindow::quit() {
    QCoreApplication::quit();
}

void MainWindow::showAbout() {
    AboutDialog ad;
    ad.exec();
}

void MainWindow::showChartWindow() {
    ChartWindow *cw = new ChartWindow();
    cw->setAttribute(Qt::WA_DeleteOnClose);
    cw->show();
}

void MainWindow::updateSysTrayText(QString text) {
    sysTrayIcon->setToolTip(text);
}

void MainWindow::updateSysTrayIcon(QIcon icon) {
    sysTrayIcon->setIcon(icon);
}
