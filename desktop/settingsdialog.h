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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

/**
 * @brief The SettingsDialog class runs a dialog allowing the user to change
 * the programs settings.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
     * @brief SettingsDialog constructs a new settings dialog, populates it
     * with the current settings and displays it.
     * @param parent Parent window.
     */
    explicit SettingsDialog(QWidget *parent = 0);

    /** Cleans up.
     */
    ~SettingsDialog();
    
protected:
    /**
     * @brief changeEvent retranslates the user interface if the language has
     * changed. This functionality is not implemented at this time.
     * @param e Event details.
     */
    void changeEvent(QEvent *e);

protected slots:

    /**
     * @brief dialogAccepted is called when the user accepts any changes to the
     * settings. It causes them to be written to the settings file.
     */
    void dialogAccepted();

    /**
     * @brief writeSettings reads settings from the user interface and writes
     * them to the configuration file (or registry on windows).
     */
    void writeSettings();

    /**
     * @brief loadSettings loads program settings into the user interface.
     */
    void loadSettings();
    
private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
