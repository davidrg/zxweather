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

#include "database.h"

#include <QtDebug>
#include <QDateTime>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    seconds_since_last_refresh = 0;
    minutes_late = 0;

    connected = false;

    settings = new QSettings("zxnet","zxweather",this);

    sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    sysTrayIcon->setToolTip("No data");
    sysTrayIcon->show();

    notificationTimer = new QTimer(this);
    notificationTimer->setInterval(1000);

    signalAdapter = new DBSignalAdapter(this);
    wdb_set_signal_adapter(signalAdapter);
    // Signals we won't handle specially:
    connect(signalAdapter, SIGNAL(connection_exception(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(connection_does_not_exist(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(connection_failure(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(server_rejected_connection(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(transaction_resolution_unknown(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(protocol_violation(QString)), this, SLOT(unknown_db_error(QString)));
    connect(signalAdapter, SIGNAL(database_error(QString)), this, SLOT(unknown_db_error(QString)));

    // Signals that we will handle specially:
    connect(signalAdapter, SIGNAL(unable_to_establish_connection(QString)), this, SLOT(connection_failed(QString)));

    // Other UI signals
    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(db_refresh()));
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
    connect(notificationTimer, SIGNAL(timeout()), this, SLOT(notification_pump()));

    db_connect();
}

MainWindow::~MainWindow()
{
    notificationTimer->stop();
    wdb_disconnect();
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
    default:
        break;
    }
}

void MainWindow::db_connect() {

    QString dbName = settings->value("Database/name").toString();
    QString dbHostname = settings->value("Database/hostname").toString();
    QString dbPort = settings->value("Database/port").toString();
    QString username = settings->value("Database/username").toString();
    QString password = settings->value("Database/password").toString();

    QString target = dbName;
    if (!dbHostname.isEmpty()) {
        target += "@" + dbHostname;

        if (!dbPort.isEmpty())
            target += ":" + dbPort;
    }

    qDebug() << "Connecting to target" << target << "as user" << username;

    if (!wdb_connect(target.toAscii().constData(),
                     username.toAscii().constData(),
                     password.toAscii().constData())) {
        // Failed to connect.
        return;
    }

    seconds_since_last_refresh = 0;

    notificationTimer->start();

    connected = true;
    db_refresh();
}

void MainWindow::db_refresh() {
    live_data_record rec = wdb_get_live_data();

    ui->lblRelativeHumidity->setText(QString::number(rec.relative_humidity) + " %");
    ui->lblTemperature->setText(QString::number(rec.temperature) + " °C");
    ui->lblDewPoint->setText(QString::number(rec.dew_point) + " °C");
    ui->lblWindChill->setText(QString::number(rec.wind_chill) + " °C");
    ui->lblApparentTemperature->setText(QString::number(rec.apparent_temperature) + " °C");
    ui->lblAbsolutePressure->setText(QString::number(rec.absolute_pressure) + " hPa");
    ui->lblAverageWindSpeed->setText(QString::number(rec.average_wind_speed) + " m/s");
    ui->lblGustWindSpeed->setText(QString::number(rec.gust_wind_speed) + " m/s");
    ui->lblWindDirection->setText(QString(rec.wind_direction));
    QString timestamp = QDateTime::fromTime_t(rec.download_timestamp).toString();
    ui->lblTimestamp->setText(timestamp);

    if (rec.temperature > 0)
        sysTrayIcon->setIcon(QIcon(":/icons/systray_icon"));
    else
        sysTrayIcon->setIcon(QIcon(":/icons/systray_subzero"));

    QString ttt = "Temperature: " + QString::number(rec.temperature) + " °C\n"
                + "Humidity: " + QString::number(rec.relative_humidity) + " %\n"
                + "Indoor Temperature: " + QString::number(rec.indoor_temperature) + " °C\n"
                + "Indoor Humidity: " + QString::number(rec.indoor_relative_humidity) + " %"
                ;

    sysTrayIcon->setToolTip(ttt);

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void MainWindow::notification_pump() {
    seconds_since_last_refresh++;
    qDebug() << "Checking for notifications...";

    if (wdb_live_data_available()) {
        qDebug() << "Live data available";
        db_refresh();
    }

    if (seconds_since_last_refresh == 60) {
        minutes_late++;

        showWarningPopup("Live data has not been refreshed in over " +
                         QString::number(minutes_late) +
                         " minutes. Check data update service.",
                         "Live data is late",
                         "Live data is late",
                         true);

//        sysTrayIcon->setToolTip("Live data is late");
//        sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
//        sysTrayIcon->showMessage("Live data is late",
//                                 "Live data has not been refreshed in over " +
//                                 QString::number(minutes_late) +
//                                 " minutes. Check data update service.",
//                                 QSystemTrayIcon::Warning);
        seconds_since_last_refresh = 0;
    }
}

void MainWindow::showSettings() {
    SettingsDialog sd;
    int result = sd.exec();

    // Have a go at connecting if required - perhaps the user has fixed what
    // ever was wrong.
    if (result == QDialog::Accepted && !connected)
        db_connect();
}

void MainWindow::connection_failed(QString) {
    showWarningPopup("Failed to connect to the database",
                     "Error",
                     "Database connect failed",
                     true);
    notificationTimer->stop();
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
