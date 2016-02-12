#include "addgraphdialog.h"
#include "ui_addgraphdialog.h"

AddGraphDialog::AddGraphDialog(SampleColumns availableColumns, bool solarAvailable,
                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddGraphDialog)
{
    ui->setupUi(this);

    if (!solarAvailable) {
        ui->gbSolar->setVisible(false);
    }

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    if ((availableColumns & TEMPERATURE_COLUMNS) == 0) {
        // No temperature columns are available. Turn it off entirely.
        ui->gbTemperature->setEnabled(false);
    } else {
        // At least one is available.
        ui->cbTemperature->setEnabled(
                    availableColumns.testFlag(SC_Temperature));
        ui->cbApparentTemperature->setEnabled(
                    availableColumns.testFlag(SC_ApparentTemperature));
        ui->cbInsideTemperature->setEnabled(
                    availableColumns.testFlag(SC_IndoorTemperature));
        ui->cbWindChill->setEnabled(availableColumns.testFlag(SC_WindChill));
        ui->cbDewPoint->setEnabled(availableColumns.testFlag(SC_DewPoint));
    }

    if ((availableColumns & HUMIDITY_COLUMNS) == 0) {
        // No humidity columns available.
        ui->gbHumidity->setEnabled(false);
    } else {
        ui->cbHumidity->setEnabled(availableColumns.testFlag(SC_Humidity));
        ui->cbInsideHumidity->setEnabled(
                    availableColumns.testFlag(SC_IndoorHumidity));
    }

    if ((availableColumns & OTHER_COLUMNS) == 0) {
        ui->gbOther->setEnabled(false);
    } else {
        ui->cbRainfall->setEnabled(availableColumns.testFlag(SC_Rainfall));
        ui->cbBarometricPressure->setEnabled(
                    availableColumns.testFlag(SC_Pressure));
    }

    if ((availableColumns & WIND_COLUMNS) == 0) {
        ui->gbWind->setEnabled(false);
    } else {
        ui->cbAverageWindSpeed->setEnabled(
                    availableColumns.testFlag(SC_AverageWindSpeed));
        ui->cbGustWindSpeed->setEnabled(
                    availableColumns.testFlag(SC_GustWindSpeed));
        ui->cbWindDirection->setEnabled(
                    availableColumns.testFlag(SC_WindDirection));
    }

    if ((availableColumns & SOLAR_COLUMNS) == 0) {
        ui->gbSolar->setEnabled(false);
    } else {
        ui->cbUVIndex->setEnabled(
                    availableColumns.testFlag(SC_UV_Index));
        ui->cbSolarRadiation->setEnabled(
                    availableColumns.testFlag(SC_SolarRadiation));
    }


    // Set all disabled stuff to 'checked' (as its already in the chart)
    ui->cbApparentTemperature->setChecked(
                !ui->cbApparentTemperature->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbTemperature->setChecked(
                !ui->cbTemperature->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbInsideTemperature->setChecked(
                !ui->cbInsideTemperature->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbWindChill->setChecked(
                !ui->cbWindChill->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbDewPoint->setChecked(
                !ui->cbDewPoint->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbHumidity->setChecked(
                !ui->cbHumidity->isEnabled() ||
                !ui->gbHumidity->isEnabled());
    ui->cbInsideHumidity->setChecked(
                !ui->cbInsideHumidity->isEnabled() ||
                !ui->gbHumidity->isEnabled());
    ui->cbRainfall->setChecked(
                !ui->cbRainfall->isEnabled() ||
                !ui->gbOther->isEnabled());
    ui->cbBarometricPressure->setChecked(
                !ui->cbBarometricPressure->isEnabled() ||
                !ui->gbOther->isEnabled());
    ui->cbAverageWindSpeed->setChecked(
                !ui->cbAverageWindSpeed->isEnabled() ||
                !ui->gbWind->isEnabled());
    ui->cbGustWindSpeed->setChecked(
                !ui->cbGustWindSpeed->isEnabled() ||
                !ui->gbWind->isEnabled());
    ui->cbWindDirection->setChecked(
                !ui->cbWindDirection->isEnabled() ||
                !ui->gbWind->isEnabled());
    ui->cbUVIndex->setChecked(
                !ui->cbUVIndex->isEnabled() ||
                !ui->gbSolar->isEnabled());
    ui->cbSolarRadiation->setChecked(
                !ui->cbSolarRadiation->isEnabled() ||
                !ui->gbSolar->isEnabled());
}

AddGraphDialog::~AddGraphDialog()
{
    delete ui;
}

#define COL_CHECKED(checkbox) checkbox->isChecked() && checkbox->isEnabled()

SampleColumns AddGraphDialog::selectedColumns()
{
    SampleColumns columns;

    if (ui->gbTemperature->isEnabled()) {
        if (COL_CHECKED(ui->cbTemperature))
            columns |= SC_Temperature;
        if (COL_CHECKED(ui->cbInsideTemperature))
            columns |= SC_IndoorTemperature;
        if (COL_CHECKED(ui->cbApparentTemperature))
            columns |= SC_ApparentTemperature;
        if (COL_CHECKED(ui->cbWindChill))
            columns |= SC_WindChill;
        if (COL_CHECKED(ui->cbDewPoint))
            columns |= SC_DewPoint;
    }

    if (ui->gbHumidity->isEnabled()) {
        if (COL_CHECKED(ui->cbHumidity))
            columns |= SC_Humidity;
        if (COL_CHECKED(ui->cbInsideHumidity))
            columns |= SC_IndoorHumidity;
    }

    if (ui->gbWind->isEnabled()) {
        if (COL_CHECKED(ui->cbAverageWindSpeed))
            columns |= SC_AverageWindSpeed;
        if (COL_CHECKED(ui->cbGustWindSpeed))
            columns |= SC_GustWindSpeed;
        if (COL_CHECKED(ui->cbWindDirection))
            columns |= SC_WindDirection;
    }

    if (ui->gbOther->isEnabled()) {
        if (COL_CHECKED(ui->cbBarometricPressure))
            columns |= SC_Pressure;
        if (COL_CHECKED(ui->cbRainfall))
            columns |= SC_Rainfall;
    }
    if (ui->gbSolar->isEnabled()) {
        if (COL_CHECKED(ui->cbUVIndex))
            columns |= SC_UV_Index;
        if (COL_CHECKED(ui->cbSolarRadiation))
            columns |= SC_SolarRadiation;
    }

    return columns;
}

