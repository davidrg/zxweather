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


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow::MainWindow...";
    ui->setupUi(this);

    // If we don't do this then there is a heap of empty space at the bottom
    // of the window.
    resize(width(), minimumHeight());

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
        ui->liveData->reconfigureDataSource();
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
