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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

/**
 * @brief The AboutDialog class displays information about the program,
 * copyright details, etc.
 */
class AboutDialog : public QDialog
{
    Q_OBJECT
    
public:
    /**
     * @brief AboutDialog constructs a new about dialog.
     * @param parent The parent window.
     */
    explicit AboutDialog(QWidget *parent = 0);

    /** Cleans up.
     */
    ~AboutDialog();
    
protected:
    /**
     * @brief changeEvent retranslates the UI if the language changes.
     * @param e Event details.
     */
    void changeEvent(QEvent *e);
    
private:
    Ui::AboutDialog *ui;
};

#endif // ABOUTDIALOG_H
