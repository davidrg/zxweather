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

#include "databasedatasource.h"

#include "aboutdialog.h"

#include <QtDebug>
#include <QDateTime>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);

    seconds_since_last_refresh = 0;
    minutes_late = 0;

    settings = new QSettings("zxnet","zxweather",this);

    sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    sysTrayIcon->setToolTip("No data");
    sysTrayIcon->show();
    connect(sysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    trayIconMenu = new QMenu(this);
    restoreAction = new QAction("&Restore",trayIconMenu);
    quitAction = new QAction("&Quit",trayIconMenu);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
    sysTrayIcon->setContextMenu(trayIconMenu);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quit()));

    // Other UI signals
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));

    dataSource = NULL;

    ldTimer = new QTimer();
    ldTimer->setInterval(1000);
    connect(ldTimer, SIGNAL(timeout()), this, SLOT(ld_timeout()));

    qDebug() << "Read settings and connect...";
    readSettings();
    createDatabaseDataSource();
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

            if (!settings->value("SingleShot/minimise_to_systray_info",false).toBool()) {
                QMessageBox::information(this, "zxweather",
                                     "zxweather will minimise to the "
                                     "system tray. To restore it, click on the "
                                     "icon. This behaviour can be changed in the "
                                     "settings dialog.");
                settings->setValue("SingleShot/minimise_to_systray_info",true);
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

void MainWindow::createDatabaseDataSource() {
    QString dbName = settings->value("Database/name").toString();
    QString hostname = settings->value("Database/hostname").toString();
    int port = settings->value("Database/port").toInt();
    QString username = settings->value("Database/username").toString();
    QString password = settings->value("Database/password").toString();

    if (dataSource != NULL) {
        delete dataSource;
        dataSource = NULL;
    }

    DatabaseDataSource *dds;

    dds = new DatabaseDataSource(dbName,
                                 hostname,
                                 port,
                                 username,
                                 password,
                                 this);

    if (!dds->isConnected()) {
        delete dds;
        return;
    }

    connect(dds, SIGNAL(connection_failed(QString)),
            this, SLOT(connection_failed(QString)));
    connect(dds, SIGNAL(database_error(QString)),
            this, SLOT(unknown_db_error(QString)));
    connect(dds, SIGNAL(liveDataRefreshed()),
            this, SLOT(liveDataRefreshed()));

    dataSource = dds;
    ldTimer->start();

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
    formatString = "%1% (%2% inside)";
    temp = formatString
            .arg(QString::number(data->getRelativeHumidity()))
            .arg(QString::number(data->getIndoorRelativeHumidity()));
    ui->lblRelativeHumidity->setText(temp);

    // Temperature
    formatString = "%1°C (%2°C inside)";
    temp = formatString
            .arg(QString::number(data->getTemperature(),'f',1))
            .arg(QString::number(data->getIndoorTemperature(), 'f', 1));
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
    formatString = "Temperature: %1°C (%2°C inside)\nHumidity: %3% (%4% inside)";
    temp = formatString
            .arg(QString::number(data->getTemperature(), 'f', 1),
                 QString::number(data->getIndoorTemperature(), 'f', 1),
                 QString::number(data->getRelativeHumidity(), 'f', 1),
                 QString::number(data->getIndoorRelativeHumidity(), 'f', 1));
    sysTrayIcon->setToolTip(temp);

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void MainWindow::showSettings() {
    SettingsDialog sd;
    int result = sd.exec();

    if (result == QDialog::Accepted) {
        readSettings();

        // Have a go at connecting if required - perhaps the user has fixed
        // what ever was wrong.
        if (dataSource == NULL || !dataSource->isConnected())
            createDatabaseDataSource();
    }
}

void MainWindow::connection_failed(QString) {
    showWarningPopup("Failed to connect to the database",
                     "Error",
                     "Database connect failed",
                     true);
    ldTimer->stop();
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
    minimise_to_systray = settings->value("General/minimise_to_systray", true).toBool();
    close_to_systray = settings->value("General/close_to_systray", false).toBool();
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
            if (!settings->value("SingleShot/close_to_systray_info",false).toBool()) {
                QMessageBox::information(this, "zxweather",
                                     "zxweather will keep running in the "
                                        "system tray. To restore it, click on the "
                                        "icon. To exit, right-click on the system tray "
                                        "icon and choose <b>Exit</b>. This behaviour "
                                        "can be changed from the settings dialog.");
                settings->setValue("SingleShot/close_to_systray_info",true);
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
