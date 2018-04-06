#include "addlivegraphdialog.h"
#include "ui_addlivegraphdialog.h"

AddLiveGraphDialog::AddLiveGraphDialog(LiveValues availableColumns, bool solarAvailable, hardware_type_t hw_type, QString message, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddLiveGraphDialog)
{
    ui->setupUi(this);

    if (!message.isNull()) {
        ui->lblMessage->setText(message);
    }

    // These columns are all Davis-specific
    ui->gbSolar->setVisible(solarAvailable && hw_type == HW_DAVIS);
    ui->gbRain->setVisible(hw_type == HW_DAVIS);
    ui->cbConsoleBattery->setVisible(hw_type == HW_DAVIS);

    if ((availableColumns & LIVE_TEMPERATURE_COLUMNS) == 0) {
        ui->gbTemperature->setEnabled(false);
    } else {
        ui->cbTemperature->setEnabled(availableColumns.testFlag(LV_Temperature));
        ui->cbApparentTemperature->setEnabled(availableColumns.testFlag(LV_ApparentTemperature));
        ui->cbInsideTemperature->setEnabled(availableColumns.testFlag(LV_IndoorTemperature));
        ui->cbWindChill->setEnabled(availableColumns.testFlag(LV_WindChill));
        ui->cbDewPoint->setEnabled(availableColumns.testFlag(LV_DewPoint));
    }

    if ((availableColumns & LIVE_HUMIDITY_COLUMNS) == 0) {
        ui->gbHumidity->setEnabled(false);
    } else {
        ui->cbHumidity->setEnabled(availableColumns.testFlag(LV_Humidity));
        ui->cbInsideHumidity->setEnabled(availableColumns.testFlag(LV_IndoorHumidity));
    }

    if ((availableColumns & LIVE_OTHER_COLUNS) == 0) {
        ui->gbOther->setEnabled(false);
    } else {
        ui->cbConsoleBattery->setEnabled(availableColumns.testFlag(LV_BatteryVoltage));
        ui->cbPressure->setEnabled(availableColumns.testFlag(LV_Pressure));
    }

    if ((availableColumns & LIVE_WIND_COLUMNS) == 0) {
        ui->gbWind->setEnabled(false);
    } else {
        ui->cbWindSpeed->setEnabled(availableColumns.testFlag(LV_WindSpeed));
        ui->cbWindDirection->setEnabled(availableColumns.testFlag(LV_WindDirection));
    }

    if ((availableColumns & LIVE_RAIN_COLUMNS) == 0) {
        ui->gbRain->setEnabled(false);
    } else {
        ui->cbRainRate->setEnabled(availableColumns.testFlag(LV_RainRate));
        ui->cbStormRain->setEnabled(availableColumns.testFlag(LV_StormRain));
    }

    if ((availableColumns & LIVE_SOLAR_COLUMNS) == 0) {
        ui->gbSolar->setEnabled(false);
    } else {
        ui->cbUVIndex->setEnabled(availableColumns.testFlag(LV_UVIndex));
        ui->cbSolarRadiation->setEnabled(availableColumns.testFlag(LV_SolarRadiation));
    }

    // Check all disabled stuff as its already in the chart
    ui->cbTemperature->setChecked(!ui->cbTemperature->isEnabled() ||
                                  !ui->gbTemperature->isEnabled());
    ui->cbApparentTemperature->setChecked(!ui->cbApparentTemperature->isEnabled() ||
                                          !ui->gbTemperature->isEnabled());
    ui->cbInsideTemperature->setChecked(!ui->cbInsideTemperature->isEnabled() ||
                                        !ui->gbTemperature->isEnabled());
    ui->cbWindChill->setChecked(!ui->cbWindChill->isEnabled() ||
                                !ui->gbTemperature->isEnabled());
    ui->cbDewPoint->setChecked(!ui->cbDewPoint->isEnabled() ||
                               !ui->gbTemperature->isEnabled());
    ui->cbHumidity->setChecked(!ui->cbHumidity->isEnabled() ||
                               !ui->gbHumidity->isEnabled());
    ui->cbInsideHumidity->setChecked(!ui->cbInsideHumidity->isEnabled() ||
                                     !ui->gbHumidity->isEnabled());
    ui->cbConsoleBattery->setChecked(!ui->cbConsoleBattery->isEnabled() ||
                                     !ui->gbOther->isEnabled());
    ui->cbPressure->setChecked(!ui->cbPressure->isEnabled() ||
                               !ui->gbOther->isEnabled());
    ui->cbWindSpeed->setChecked(!ui->cbWindSpeed->isEnabled() ||
                                !ui->gbWind->isEnabled());
    ui->cbWindDirection->setChecked(!ui->cbWindDirection->isEnabled() ||
                                    !ui->gbWind->isEnabled());
    ui->cbRainRate->setChecked(!ui->cbRainRate->isEnabled() ||
                               !ui->gbRain->isEnabled());
    ui->cbStormRain->setChecked(!ui->cbStormRain->isEnabled() ||
                                !ui->gbRain->isEnabled());
    ui->cbUVIndex->setChecked(!ui->cbUVIndex->isEnabled() ||
                              !ui->gbSolar->isEnabled());
    ui->cbSolarRadiation->setChecked(!ui->cbSolarRadiation->isEnabled() ||
                                     !ui->gbSolar->isEnabled());
}

AddLiveGraphDialog::~AddLiveGraphDialog()
{
    delete ui;
}

#define COL_CHECKED(checkbox) checkbox->isChecked() && checkbox->isEnabled()

LiveValues AddLiveGraphDialog::selectedColumns() {
    LiveValues columns;
    if (ui->gbTemperature->isEnabled()) {
        if (COL_CHECKED(ui->cbTemperature))
            columns |= LV_Temperature;
        if (COL_CHECKED(ui->cbApparentTemperature))
            columns |= LV_ApparentTemperature;
        if (COL_CHECKED(ui->cbInsideTemperature))
            columns |= LV_IndoorTemperature;
        if (COL_CHECKED(ui->cbWindChill))
            columns |= LV_WindChill;
        if (COL_CHECKED(ui->cbDewPoint))
            columns |= LV_DewPoint;
    }

    if (ui->gbHumidity->isEnabled()) {
        if (COL_CHECKED(ui->cbHumidity))
            columns |= LV_Humidity;
        if (COL_CHECKED(ui->cbInsideHumidity))
            columns |= LV_IndoorHumidity;
    }

    if (ui->gbOther->isEnabled()) {
        if (COL_CHECKED(ui->cbPressure))
            columns |= LV_Pressure;
        if (COL_CHECKED(ui->cbConsoleBattery))
            columns |= LV_BatteryVoltage;
    }

    if (ui->gbWind->isEnabled()) {
        if (COL_CHECKED(ui->cbWindSpeed))
            columns |= LV_WindSpeed;
        if (COL_CHECKED(ui->cbWindDirection))
            columns |= LV_WindDirection;
    }

    if (ui->gbRain->isEnabled()) {
        if (COL_CHECKED(ui->cbRainRate))
            columns |= LV_RainRate;
        if (COL_CHECKED(ui->cbStormRain))
            columns |= LV_StormRain;
    }

    if (ui->gbSolar->isEnabled()) {
        if (COL_CHECKED(ui->cbUVIndex))
            columns |= LV_UVIndex;
        if (COL_CHECKED(ui->cbSolarRadiation))
            columns |= LV_SolarRadiation;
    }

    return columns;
}
