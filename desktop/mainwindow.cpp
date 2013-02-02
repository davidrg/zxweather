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

#include "livedata/databaselivedatasource.h"
#include "livedata/jsonlivedatasource.h"
#include "aboutdialog.h"
#include "settings.h"

#include <QtDebug>
#include <QDateTime>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);

    // If we don't do this then there is a heap of empty space at the bottom
    // of the window.
    resize(width(), minimumHeight());

    seconds_since_last_refresh = 0;
    minutes_late = 0;

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

    // Other UI signals
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));

    ldTimer.reset(new QTimer());
    ldTimer->setInterval(1000);
    connect(ldTimer.data(), SIGNAL(timeout()), this, SLOT(ld_timeout()));

    // Show the settings dialog on the first run.
    if (!Settings::getInstance().singleShotFirstRun()) {
        showSettings();
        Settings::getInstance().setSingleShotFirstRun();
    }
    qDebug() << "Read settings and connect...";
    readSettings();
    createDataSource();
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

void MainWindow::createDataSource() {
    if (Settings::getInstance().dataSourceType() == Settings::DS_TYPE_DATABASE)
        createDatabaseDataSource();
    else
        createJsonDataSource();
}

void MainWindow::createJsonDataSource() {
    QString url = Settings::getInstance().url();

    JsonLiveDataSource *jds = new JsonLiveDataSource(url, this);
    connect(jds, SIGNAL(networkError(QString)),
            this, SLOT(networkError(QString)));
    connect(jds, SIGNAL(liveDataRefreshed()),
            this, SLOT(liveDataRefreshed()));

    dataSource.reset(jds);
    ldTimer->start();
}

void MainWindow::createDatabaseDataSource() {
    Settings& settings = Settings::getInstance();
    QString dbName = settings.databaseName();
    QString hostname = settings.databaseHostName();
    int port = settings.databasePort();
    QString username = settings.databaseUsername();
    QString password = settings.databasePassword();
    QString station = settings.stationName();

    if (station.isEmpty()) {
        // We're probably migrating settings from v0.1.
        QMessageBox::information(this, "Bad configuration", "The station name has not been configured. You will now be shown the settings dialog.");
        showSettings();
        return;
    }

    // Kill the old datasource. The DatabaseDataSource uses named connections
    // so we can't have two overlaping.
    if (!dataSource.isNull())
        delete dataSource.take();

    QScopedPointer<DatabaseLiveDataSource> dds;

    dds.reset(new DatabaseLiveDataSource(dbName,
                                 hostname,
                                 port,
                                 username,
                                 password,
                                 station,
                                 this));

    if (!dds->isConnected()) {
        return;
    }

    connect(dds.data(), SIGNAL(connection_failed(QString)),
            this, SLOT(connection_failed(QString)));
    connect(dds.data(), SIGNAL(database_error(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(dds.data(), SIGNAL(liveDataRefreshed()),
            this, SLOT(liveDataRefreshed()));

    dataSource.reset(dds.take());
    seconds_since_last_refresh = 0;
    ldTimer->start();

    setWindowTitle("zxweather - " + station);

    // Do an initial refresh so we're not waiting forever with nothing to show
    liveDataRefreshed();
}



void MainWindow::ld_timeout() {
    seconds_since_last_refresh++; // this is reset when ever live data arrives.

    if (seconds_since_last_refresh == 60) {
        minutes_late++;

        showWarningPopup("Live data has not been refreshed in over " +
                         QString::number(minutes_late) +
                         " minutes. Check data update service.",
                         "Live data is late",
                         "Live data is late",
                         true);

        seconds_since_last_refresh = 0;
    }
}

void MainWindow::liveDataRefreshed() {
    QScopedPointer<AbstractLiveData> data(dataSource->getLiveData());

    QString formatString;
    QString temp;

    // Relative Humidity
    if (data->indoorDataAvailable()) {
        formatString = "%1% (%2% inside)";
        temp = formatString
                .arg(QString::number(data->getRelativeHumidity()))
                .arg(QString::number(data->getIndoorRelativeHumidity()));
    } else {
        formatString = "%1%";
        temp = formatString
                .arg(QString::number(data->getRelativeHumidity()));
    }
    ui->lblRelativeHumidity->setText(temp);

    // Temperature
    if (data->indoorDataAvailable()) {
        formatString = "%1°C (%2°C inside)";
        temp = formatString
                .arg(QString::number(data->getTemperature(),'f',1))
                .arg(QString::number(data->getIndoorTemperature(), 'f', 1));
    } else {
        formatString = "%1°C";
        temp = formatString
                .arg(QString::number(data->getTemperature(),'f',1));
    }
    ui->lblTemperature->setText(temp);

    ui->lblDewPoint->setText(QString::number(data->getDewPoint(), 'f', 1) + "°C");
    ui->lblWindChill->setText(QString::number(data->getWindChill(), 'f', 1) + "°C");
    ui->lblApparentTemperature->setText(
                QString::number(data->getApparentTemperature(), 'f', 1) + "°C");
    ui->lblAbsolutePressure->setText(
                QString::number(data->getAbsolutePressure(), 'f', 1) + " hPa");
    ui->lblAverageWindSpeed->setText(
                QString::number(data->getAverageWindSpeed(), 'f', 1) + " m/s");
    ui->lblGustWindSpeed->setText(
                QString::number(data->getGustWindSpeed(), 'f', 1) + " m/s");
    ui->lblWindDirection->setText(data->getWindDirection());
    ui->lblTimestamp->setText(data->getTimestamp().toString("h:mm AP"));

    if (data->getTemperature() > 0)
        sysTrayIcon->setIcon(QIcon(":/icons/systray_icon"));
    else
        sysTrayIcon->setIcon(QIcon(":/icons/systray_subzero"));

    // Tool Tip Text
    if (data->indoorDataAvailable()) {
        formatString = "Temperature: %1°C (%2°C inside)\nHumidity: %3% (%4% inside)";
        temp = formatString
                .arg(QString::number(data->getTemperature(), 'f', 1),
                     QString::number(data->getIndoorTemperature(), 'f', 1),
                     QString::number(data->getRelativeHumidity(), 'f', 1),
                     QString::number(data->getIndoorRelativeHumidity(), 'f', 1));
    } else {
        formatString = "Temperature: %1°C\nHumidity: %3%";
        temp = formatString
                .arg(QString::number(data->getTemperature(), 'f', 1),
                     QString::number(data->getRelativeHumidity(), 'f', 1));
    }
    sysTrayIcon->setToolTip(temp);

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void MainWindow::showSettings() {
    SettingsDialog sd;
    int result = sd.exec();

    if (result == QDialog::Accepted) {
        readSettings();

        // Reconenct (incase the user has changed connection details)
        createDataSource();
    }
}

void MainWindow::connection_failed(QString) {
    showWarningPopup("Failed to connect to the database",
                     "Error",
                     "Database connect failed",
                     true);
    ldTimer->stop();
}

void MainWindow::networkError(QString message) {
    showWarningPopup(message, "Error", "Network Error", true);
}

void MainWindow::unknown_db_error(QString message) {
    showWarningPopup(message, "Database Error");
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

