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

#include <QSettings>

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
    QSettings settings("zxnet","zxweather");

    // General tab
    settings.setValue("General/minimise_to_systray", ui->chkMinimiseToSystemTray->isChecked());
    settings.setValue("General/close_to_systray", ui->chkCloseToSystemTray->isChecked());

    // Database tab
    settings.setValue("Database/name", ui->databaseLineEdit->text());
    settings.setValue("Database/hostname", ui->hostnameLineEdit->text());
    settings.setValue("Database/port", ui->portSpinBox->value());
    settings.setValue("Database/username", ui->usernameLineEdit->text());
    settings.setValue("Database/password", ui->passwordLineEdit->text());
}

void SettingsDialog::loadSettings() {
    QSettings settings("zxnet","zxweather");

    // General tab
    ui->chkMinimiseToSystemTray->setChecked(settings.value("General/minimise_to_systray", true).toBool());
    ui->chkCloseToSystemTray->setChecked(settings.value("General/close_to_systray",false).toBool());

    // Database tab
    ui->databaseLineEdit->setText(settings.value("Database/name").toString());
    ui->hostnameLineEdit->setText(settings.value("Database/hostname").toString());
    ui->portSpinBox->setValue(settings.value("Database/port",5432).toInt());
    ui->usernameLineEdit->setText(settings.value("Database/username").toString());
    ui->passwordLineEdit->setText(settings.value("Database/password").toString());
}
