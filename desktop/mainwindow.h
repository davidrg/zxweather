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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QCloseEvent>
#include <QScopedPointer>
#include <QLayoutItem>

#include "datasource/abstractlivedatasource.h"

namespace Ui {
class MainWindow;
}

/**
 * @brief zxweather Main Window. Displays current conditions and allows the
 * user to access the settings dialog.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    /**
     * @brief MainWindow Constructs a new MainWindow. Only one should exist
     * during the lifetime of the application.
     *
     * This function will cause a connection to the currently configured
     * database to be made.
     *
     * @param parent Parent widget. Should always be 0.
     */
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
public slots:
    /**
     * @brief showSettings Shows the settings dialog.
     *
     * If the user accepts the settings dialog (hits OK) then settings are
     * reloaded and db_connect() is called if the program is not currently
     * connected to the database.
     */
    void showSettings();

    /**
     * @brief showChartWindow shows the chart selection window.
     */
    void showChartWindow();

    // System tray
    /**
     * @brief trayIconActivated is called when the user clicks on the system
     * tray icon. It raises the window and gives it focus.
     * @param reason Reason for activation (click, double click, etc).
     */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

    /**
     * @brief quit quits the application.
     */
    void quit();

    /**
     * @brief showAbout shows the about dialog.
     */
    void showAbout();

    void showWarningPopup(QString message, QString title, QString tooltip="", bool setWarningIcon=false);
    void dataSourceError(QString message);

    void updateSysTrayText(QString text);
    void updateSysTrayIcon(QIcon icon);

private slots:
    /** For monitoring live data. This (and the associated time ldTimer) is
     * what pops up warnings when live data is late.
     */
    void liveTimeout();

    /** Mostly used to check for late live data.
     */
    void liveDataRefreshed(LiveDataSet lds);

protected:
    /**
     * @brief changeEvent handles minimising the window to the system tray if
     * that setting is turned on.
     * @param e details about the event that has occurred.
     */
    void changeEvent(QEvent *e);

    /**
     * @brief closeEvent handles hiding the program to the system tray when the
     * window is closed if the close to system tray option is turned on.
     * @param event details about the event that has occurred.
     */
    void closeEvent(QCloseEvent *event);

private:
    Ui::MainWindow *ui;
    QScopedPointer<QSystemTrayIcon> sysTrayIcon;
    QScopedPointer<QMenu> trayIconMenu;
    QScopedPointer<QAction> restoreAction;
    QScopedPointer<QAction> quitAction;

    bool minimise_to_systray;
    bool close_to_systray;

    void readSettings();

    int getDatabaseVersion();
    void databaseCompatibilityChecks();
    void reconnectDatabase();

    /** Reconnects to the datasource. Call this when ever data source
     * settings are changed.
     */
    void reconfigureDataSource();

    QScopedPointer<AbstractLiveDataSource> dataSource;

    uint seconds_since_last_refresh;
    uint minutes_late;
    QTimer* ldTimer;

    hardware_type_t last_hw_type;
    QLayoutItem* statusItem;
    QLayoutItem* forecastItem;
    QLayoutItem* spacerItem;
};

#endif // MAINWINDOW_H
