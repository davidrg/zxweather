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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"

#include <QSqlDatabase>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
    connect(ui->rbLiveDatabase, SIGNAL(clicked()), this, SLOT(dataSourceChanged()));
    connect(ui->rbLiveServer, SIGNAL(clicked()), this, SLOT(dataSourceChanged()));
    connect(ui->rbLiveWeb, SIGNAL(clicked()), this, SLOT(dataSourceChanged()));
    connect(ui->rbSampleDatabase, SIGNAL(clicked()), this, SLOT(dataSourceChanged()));
    connect(ui->rbSampleWeb, SIGNAL(clicked()), this, SLOT(dataSourceChanged()));

    // Disable the samples database option if the Postgres driver isn't present.
    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        ui->rbSampleDatabase->setEnabled(false);
        ui->rbSampleDatabase->setText("Database (driver not found)");
    }

    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void SettingsDialog::dialogAccepted() {
    writeSettings();
    accept();
}

void SettingsDialog::dataSourceChanged() {
    ui->gbDatabase->setEnabled(false);
    ui->gbWeb->setEnabled(false);
    ui->gbServer->setEnabled(false);

    if (ui->rbLiveDatabase->isChecked() || ui->rbSampleDatabase->isChecked())
        ui->gbDatabase->setEnabled(true);
    if (ui->rbLiveWeb->isChecked() || ui->rbSampleWeb->isChecked())
        ui->gbWeb->setEnabled(true);
    if (ui->rbLiveServer->isChecked())
        ui->gbServer->setEnabled(true);
}

void SettingsDialog::writeSettings() {

    Settings& settings = Settings::getInstance();

    // General tab
    settings.setMinimiseToSysTray(ui->chkMinimiseToSystemTray->isChecked());
    settings.setCloseToSysTray(ui->chkCloseToSystemTray->isChecked());

    // Data source tab
    settings.setDatabaseName(ui->databaseLineEdit->text());
    settings.setDatabaseHostname(ui->hostnameLineEdit->text());
    settings.setDatabasePort(ui->portSpinBox->value());
    settings.setDatabaseUsername(ui->usernameLineEdit->text());
    settings.setDatabasePassword(ui->passwordLineEdit->text());
    settings.setWebInterfaceUrl(ui->UrlLineEdit->text());
    settings.setStationCode(ui->stationNameLineEdit->text());
    settings.setServerHostname(ui->serverHostnameLineEdit->text());
    settings.setServerPort(ui->serverPortSpinBox->value());

    if (ui->rbLiveDatabase->isChecked())
        settings.setLiveDataSourceType(Settings::DS_TYPE_DATABASE);
    else if (ui->rbLiveWeb->isChecked())
        settings.setLiveDataSourceType(Settings::DS_TYPE_WEB_INTERFACE);
    else if (ui->rbLiveServer->isChecked())
        settings.setLiveDataSourceType(Settings::DS_TYPE_SERVER);

    if (ui->rbSampleDatabase->isChecked())
        settings.setSampleDataSourceType(Settings::DS_TYPE_DATABASE);
    else
        settings.setSampleDataSourceType(Settings::DS_TYPE_WEB_INTERFACE);

    // Charts tab
    ChartColours colours;
    colours.apparentTemperature = ui->qcpApparentTemperature->color();
    colours.dewPoint = ui->qcpDewPoint->color();
    colours.humidity = ui->qcpHumidity->color();
    colours.indoorHumidity = ui->qcpIndoorHumidity->color();
    colours.indoorTemperature = ui->qcpIndoorTemperature->color();
    colours.pressure = ui->qcpPressure->color();
    colours.temperature = ui->qcpTemperature->color();
    colours.windChill = ui->qcpWindChill->color();
    colours.rainfall = ui->qcpRainfall->color();
    settings.setChartColours(colours);
}

void SettingsDialog::loadSettings() {
    Settings& settings = Settings::getInstance();

    // General tab
    ui->chkMinimiseToSystemTray->setChecked(settings.miniseToSysTray());
    ui->chkCloseToSystemTray->setChecked(settings.closeToSysTray());

    // Data source tab
    ui->databaseLineEdit->setText(settings.databaseName());
    ui->hostnameLineEdit->setText(settings.databaseHostName());
    ui->portSpinBox->setValue(settings.databasePort());
    ui->usernameLineEdit->setText(settings.databaseUsername());
    ui->passwordLineEdit->setText(settings.databasePassword());
    ui->stationNameLineEdit->setText(settings.stationCode());
    ui->serverPortSpinBox->setValue(settings.serverPort());
    ui->serverHostnameLineEdit->setText(settings.serverHostname());

    ui->UrlLineEdit->setText(settings.webInterfaceUrl());

    if (settings.liveDataSourceType() == Settings::DS_TYPE_DATABASE) {
        ui->rbLiveDatabase->setChecked(true);
    } else if (settings.liveDataSourceType() == Settings::DS_TYPE_WEB_INTERFACE) {
        ui->rbLiveWeb->setChecked(true);
    } else {
        ui->rbLiveServer->setChecked(true);
    }

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE) {
        ui->rbSampleDatabase->setChecked(true);
    } else {
        ui->rbSampleWeb->setChecked(true);
    }

    // Charts tab
    ChartColours colours = settings.getChartColours();
    ui->qcpApparentTemperature->setColor(colours.apparentTemperature);
    ui->qcpDewPoint->setColor(colours.dewPoint);
    ui->qcpHumidity->setColor(colours.humidity);
    ui->qcpIndoorHumidity->setColor(colours.indoorHumidity);
    ui->qcpIndoorTemperature->setColor(colours.indoorTemperature);
    ui->qcpPressure->setColor(colours.pressure);
    ui->qcpTemperature->setColor(colours.temperature);
    ui->qcpWindChill->setColor(colours.windChill);
    ui->qcpRainfall->setColor(colours.rainfall);

    dataSourceChanged();
}
