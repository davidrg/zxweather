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

    sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
    sysTrayIcon->setToolTip("No data");
    sysTrayIcon->show();

    notificationTimer = new QTimer(this);
    notificationTimer->setInterval(1000);

    connect(ui->pbConnect, SIGNAL(clicked()), this, SLOT(db_connect()));
    connect(ui->pbRefresh, SIGNAL(clicked()), this, SLOT(db_refresh()));
    connect(notificationTimer, SIGNAL(timeout()), this, SLOT(notification_pump()));
}

MainWindow::~MainWindow()
{
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
    QString target = ui->leServer->text();
    QString username = ui->leUsername->text();
    QString password = ui->lePassword->text();

    wdb_connect(target.toAscii().constData(),
                username.toAscii().constData(),
                password.toAscii().constData());

    seconds_since_last_refresh = 0;

    notificationTimer->start();
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
                + "Humidity: " + QString::number(rec.relative_humidity) + " %";

    sysTrayIcon->setToolTip(ttt);

    seconds_since_last_refresh = 0;
    minutes_late = 0;
}

void MainWindow::notification_pump() {
    seconds_since_last_refresh++;

    if (wdb_live_data_available()) {
        qDebug() << "Live data available";
        db_refresh();
    }

    if (seconds_since_last_refresh == 60) {
        minutes_late++;
        sysTrayIcon->setToolTip("Live data is late");
        sysTrayIcon->setIcon(QIcon(":/icons/systray_icon_warning"));
        sysTrayIcon->showMessage("Live data is late",
                                 "Live data has not been refreshed in over " +
                                 QString::number(minutes_late) +
                                 " minutes. Check data update service.",
                                 QSystemTrayIcon::Warning);
        seconds_since_last_refresh = 0;
    }
}
