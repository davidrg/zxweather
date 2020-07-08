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
#include <QFutureWatcher>

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
    explicit SettingsDialog(bool solarDataAvailable, QWidget *parent = 0);

    /** Cleans up.
     */
    ~SettingsDialog();
    
signals:
    void stationCodeChanging(QString newStationCode);

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
    
    /** Called when one of the data source radio buttons is clicked.
     * It updates the data sources which are enabled.
     */
    void dataSourceChanged();

    /** For async calculation of the images cache
     */
    void imagesSizeCalculated();

    /** Clear image cache
     */
    void clearImages();

    /** Clear sample cache
     */
    void clearSamples();

    void imagesCleared();


    void setChartTitleFont();
    void setChartLegendFont();
    void setChartAxisLabelFont();
    void setChartTickLabelFont();
    void resetFontsToDefaults();
private:
    void getCacheInfo();

    QFont chartTitleFont;
    QFont chartLegendFont;
    QFont chartAxisLabelFont;
    QFont chartTickLabelFont;

    bool resetFonts;

    bool saveChartTitleFont;
    bool saveChartLegendFont;
    bool saveChartAxisLabelFont;
    bool saveChartTickLabelFont;


    Ui::SettingsDialog *ui;
    QFutureWatcher<qint64> imagesDirWatcher;
    QFutureWatcher<void> clearImagesWatcher;
};

#endif // SETTINGSDIALOG_H
