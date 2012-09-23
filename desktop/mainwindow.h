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

#include "datasource.h"

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
     * @brief Refreshes live data from the database.
     */
    //void db_refresh();


    /**
     * @brief showSettings Shows the settings dialog.
     *
     * If the user accepts the settings dialog (hits OK) then settings are
     * reloaded and db_connect() is called if the program is not currently
     * connected to the database.
     */
    void showSettings();


    /** Called when new live data is available.
     *
     * @param data The new live data.
     */
    void liveDataRefreshed();

    /** For monitoring live data. This (and the associated time ldTimer) is
     * what pops up warnings when live data is late.
     */
    void ld_timeout();

    // Database errors (for use with the database data source)

    /**
     * @brief Called when connecting to the database fails.
     *
     * It displays a popup message from the system tray icon containing details
     * of the problem.
     */
    void connection_failed(QString);

    /**
     * @brief Called whenever a database error occurs that is not a connection
     * failure.
     *
     * @param message The error message from the database layer.
     */
    void unknown_db_error(QString message);

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

    void createDatabaseDataSource();

private:
    Ui::MainWindow *ui;
    QSystemTrayIcon *sysTrayIcon;
    QMenu* trayIconMenu;
    QAction* restoreAction;
    QAction* quitAction;
    QTimer *ldTimer;

    QSettings* settings;
    bool minimise_to_systray;
    bool close_to_systray;

    uint seconds_since_last_refresh;
    uint minutes_late;

    void showWarningPopup(QString message, QString title, QString tooltip="", bool setWarningIcon=false);
    void readSettings();

    AbstractDataSource *dataSource;
};

#endif // MAINWINDOW_H
