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

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
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
    settings.setUrl(ui->UrlLineEdit->text());
    settings.setStationName(ui->stationNameLineEdit->text());

    if (ui->rbDatabase->isChecked())
        settings.setDataSourceType(Settings::DS_TYPE_DATABASE);
    else
        settings.setDataSourceType(Settings::DS_TYPE_WEB_INTERFACE);
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
    ui->stationNameLineEdit->setText(settings.stationName());

    ui->UrlLineEdit->setText(settings.url());

    if (settings.dataSourceType() == Settings::DS_TYPE_DATABASE) {
        ui->rbDatabase->setChecked(true);
        ui->rbWebInterface->setChecked(false);
    } else {
        ui->rbDatabase->setChecked(false);
        ui->rbWebInterface->setChecked(true);
    }
}
