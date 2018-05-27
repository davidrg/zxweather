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
#include "datasource/webcachedb.h"

#include <QSqlDatabase>
#include <QDebug>
#include <QDir>
#include <QfileInfo>
#include <QDesktopServices>

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QtConcurrent>
#else
#include <QtCore>
#endif

SettingsDialog::SettingsDialog(bool solarDataAvailable, QWidget *parent) :
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
    connect(ui->pbClearData, SIGNAL(clicked()), this, SLOT(clearSamples()));
    connect(ui->pbClearImages, SIGNAL(clicked()), this, SLOT(clearImages()));
    connect(&imagesDirWatcher, SIGNAL(finished()), this, SLOT(imagesSizeCalculated()));
    connect(&clearImagesWatcher, SIGNAL(finished()), this, SLOT(imagesCleared()));

    // Disable the samples database option if the Postgres driver isn't present.
    if (!QSqlDatabase::drivers().contains("QPSQL")) {
        ui->rbSampleDatabase->setEnabled(false);
        ui->rbSampleDatabase->setText("Database (driver not found)");
        ui->rbLiveDatabase->setEnabled(false);
        ui->rbLiveDatabase->setText("Database (driver not found)");
        qDebug() << "PostgreSQL database driver unavailable. Database functionality disabled.";
        qDebug() << QSqlDatabase::drivers();
    }

    if (!solarDataAvailable) {
        ui->lblUVIndex->setVisible(false);
        ui->qcpUVIndex->setVisible(false);
        ui->lblSolarRadiation->setVisible(false);
        ui->qcpSolarRadiation->setVisible(false);
    }

    loadSettings();

    getCacheInfo();
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
    settings.setLiveTimeoutEnabled(ui->gbLiveDataWarning->isChecked());
    settings.setLiveTimeoutInterval(ui->sbLiveDataWarningInterval->value() * 1000);

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
    colours.averageWindSpeed = ui->qcpAverageWindSpeed->color();
    colours.gustWindSpeed = ui->qcpGustWindSpeed->color();
    colours.windDirection = ui->qcpWindDirection->color();
    colours.uvIndex = ui->qcpUVIndex->color();
    colours.solarRadiation = ui->qcpSolarRadiation->color();
    colours.evapotranspiration = ui->qcpEvapotranspiration->color();
    colours.reception = ui->qcpReception->color();
    colours.title = ui->qcpTitle->color();
    colours.background = ui->qcpBackground->color();

    settings.setChartColours(colours);
}

void SettingsDialog::loadSettings() {
    Settings& settings = Settings::getInstance();

    // General tab
    ui->chkMinimiseToSystemTray->setChecked(settings.miniseToSysTray());
    ui->chkCloseToSystemTray->setChecked(settings.closeToSysTray());
    ui->gbLiveDataWarning->setChecked(settings.liveTimeoutEnabled());
    ui->sbLiveDataWarningInterval->setValue(settings.liveTimeoutInterval() / 1000);

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
    ui->qcpAverageWindSpeed->setColor(colours.averageWindSpeed);
    ui->qcpGustWindSpeed->setColor(colours.gustWindSpeed);
    ui->qcpWindDirection->setColor(colours.windDirection);
    ui->qcpUVIndex->setColor(colours.uvIndex);
    ui->qcpSolarRadiation->setColor(colours.solarRadiation);
    ui->qcpReception->setColor(colours.reception);
    ui->qcpEvapotranspiration->setColor(colours.evapotranspiration);

    ui->qcpTitle->setColor(colours.title);
    ui->qcpBackground->setColor(colours.background);

    dataSourceChanged();
}

qint64 getDirectorySize(QString dirname) {
    qint64 size = 0;

    QDir dir(dirname);

    foreach (QString file, dir.entryList(QDir::Files|QDir::System|QDir::Hidden)) {
        size += QFileInfo(dir, file).size();
    }

    foreach (QString child, dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden)) {
        size += getDirectorySize(dirname + QDir::separator() + child);
    }

    return size;
}

QString sizeToString(qint64 size) {
    QStringList units;
    units << "Bytes" << "KB" << "MB" << "GB";

    int i;
    double result = size;

    for (i=0; i<units.size()-1; i++) {
        if (result < 1024) {
            break;
        }

        result = result / 1024;
    }

    return QString("%0 %1").arg(result, 0, 'f', 2).arg(units[i]);
}

void SettingsDialog::getCacheInfo() {
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QString cacheDir = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString cacheDir = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    cacheDir += QDir::separator();

    QString imagesDir = cacheDir + QString("images");
    QString databaseFile = cacheDir + "sample-cache.db";

    ui->lblImagesSize->setText(tr("calculating..."));
    ui->lblDataSize->setText(tr("calculating..."));

    imagesDirWatcher.setFuture(QtConcurrent::run(getDirectorySize, imagesDir));

    QFileInfo db(databaseFile);

    if (db.exists()) {
        ui->lblDataSize->setText(sizeToString(db.size()));
    } else {
        ui->lblDataSize->setText("0 Bytes");
    }
}

void SettingsDialog::imagesSizeCalculated() {
    ui->lblImagesSize->setText(sizeToString(imagesDirWatcher.result()));
}

#if QT_VERSION < 0x050000
void rmdir(const QString dirName)
{
    QDir dir(dirName);

    if (dir.exists(dirName)) {
        foreach (QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                rmdir(info.absoluteFilePath());
            }
            else {
                QFile::remove(info.absoluteFilePath());
            }
        }
        dir.rmdir(dirName);
    }
}
#endif

void clearImagesDir() {
#if QT_VERSION >= 0x050000
    QString cacheDir = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString cacheDir = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    cacheDir += QDir::separator();

    QString imagesDir = cacheDir + QString("images");

#if QT_VERSION >= 0x050000
    QDir dir(imagesDir);
    dir.removeRecursively();
#else
    rmdir(imagesDir);
#endif
}

void SettingsDialog::clearImages() {
    ui->lblImagesSize->setText(tr("clearing..."));

    clearImagesWatcher.setFuture(QtConcurrent::run(clearImagesDir));
}

void SettingsDialog::imagesCleared() {
    WebCacheDB::getInstance().clearImages();
    getCacheInfo();
}

void SettingsDialog::clearSamples() {
    WebCacheDB::getInstance().clearSamples();
    getCacheInfo();
}
