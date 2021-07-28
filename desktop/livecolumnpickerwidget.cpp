#include "livecolumnpickerwidget.h"
#include "ui_columnpickerwidget.h"
#include "datasource/abstractlivedatasource.h"

LiveColumnPickerWidget::LiveColumnPickerWidget(QWidget *parent):
    ColumnPickerWidget(parent)
{

}


void LiveColumnPickerWidget::configure(bool solarAvailable,
                                       bool indoorDataAvailable,
                                       hardware_type_t hw_type,
                                       ExtraColumns extraColumns,
                                       QMap<ExtraColumn, QString> extraColumnNames) {

    // Show live-exclusive things for davis stations
    ui->cbConsoleBatteryVoltage->setEnabled(true);
    ui->cbConsoleBatteryVoltage->setVisible(true);

    // Hide sample-only stuff: Evapotranspiration, Wireless Reception, Forecast Rule ID
    hideWirelessReceptionColumn();
    ui->cbEvapotranspiration->setVisible(false);
    ui->cbEvapotranspiration->setEnabled(false);


    // Rename a few things
    if (hw_type == HW_DAVIS) {
        // Davis has storm rain.
        ui->cbRainfall->setText(tr("Storm Rain"));
    } else {
        // Generic stations have no live rain data.
        ui->gbRain->setVisible(false);
    }
    ui->cbRainRate->setText(tr("Rain Rate"));
    ui->cbWindSpeed->setText(tr("Wind Speed"));
    ui->cbWindDirection->setText(tr("Wind Direction"));

    // Nothing on this tab is ever available in a live data feed so just get rid of it.
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabHighsAndLows));

    configureUi(solarAvailable,
                indoorDataAvailable,
                hw_type,
                false, // No wireless reception readings for live data
                extraColumns,
                extraColumnNames);
}

#define CHECK_COLUMN(cb, col) if (cb->isChecked()) { columns |= col; }

LiveValues LiveColumnPickerWidget::getColumns() const {
    LiveValues columns = LV_NoColumns;

    CHECK_COLUMN(ui->cbTemperature, LV_Temperature);
    CHECK_COLUMN(ui->cbApparentTemperature, LV_ApparentTemperature);
    CHECK_COLUMN(ui->cbIndoorTemperature, LV_IndoorTemperature);
    CHECK_COLUMN(ui->cbWindChill, LV_WindChill);
    CHECK_COLUMN(ui->cbDewPoint, LV_DewPoint);
    CHECK_COLUMN(ui->cbHumidity, LV_Humidity);
    CHECK_COLUMN(ui->cbIndoorHumidity, LV_IndoorHumidity);
    CHECK_COLUMN(ui->cbRainfall, LV_StormRain);
    CHECK_COLUMN(ui->cbPressure, LV_Pressure);
    CHECK_COLUMN(ui->cbRainRate, LV_RainRate);
    CHECK_COLUMN(ui->cbWindSpeed, LV_WindSpeed);
    CHECK_COLUMN(ui->cbWindDirection, LV_WindDirection);
    CHECK_COLUMN(ui->cbUVIndex, LV_UVIndex);
    CHECK_COLUMN(ui->cbSolarRadiation, LV_SolarRadiation);
    CHECK_COLUMN(ui->cbConsoleBatteryVoltage, LV_BatteryVoltage);
    CHECK_COLUMN(ui->cbSoilMoisture1, LV_SoilMoisture1);
    CHECK_COLUMN(ui->cbSoilMoisture2, LV_SoilMoisture2);
    CHECK_COLUMN(ui->cbSoilMoisture3, LV_SoilMoisture3);
    CHECK_COLUMN(ui->cbSoilMoisture4, LV_SoilMoisture4);
    CHECK_COLUMN(ui->cbSoilTemperature1, LV_SoilTemperature1);
    CHECK_COLUMN(ui->cbSoilTemperature2, LV_SoilTemperature2);
    CHECK_COLUMN(ui->cbSoilTemperature3, LV_SoilTemperature3);
    CHECK_COLUMN(ui->cbSoilTemperature4, LV_SoilTemperature4);
    CHECK_COLUMN(ui->cbLeafWetness1, LV_LeafWetness1);
    CHECK_COLUMN(ui->cbLeafWetness2, LV_LeafWetness2);
    CHECK_COLUMN(ui->cbLeafTemperature1, LV_LeafTemperature1);
    CHECK_COLUMN(ui->cbLeafTemperature2, LV_LeafTemperature2);
    CHECK_COLUMN(ui->cbExtraHumidity1, LV_ExtraHumidity1);
    CHECK_COLUMN(ui->cbExtraHumidity2, LV_ExtraHumidity2);
    CHECK_COLUMN(ui->cbExtraTemperature1, LV_ExtraTemperature1);
    CHECK_COLUMN(ui->cbExtraTemperature2, LV_ExtraTemperature2);
    CHECK_COLUMN(ui->cbExtraTemperature3, LV_ExtraTemperature3);

    return columns;
}

#define CHECK_AND_DISABLE_CB(cb, col) cb->setEnabled(!columns.testFlag(col)); cb->setChecked(columns.testFlag(col));

void LiveColumnPickerWidget::checkAndLockColumns(LiveValues columns) {
    lockedColumns = columns;

    // Using a macro for this is a bit dirty but it significantly reduces the amount
    // of copy&paste here. For each checkbox, LiveValue pairing it will disable and
    // check the checkbox if the columns variable has the specified live value set.
    CHECK_AND_DISABLE_CB(ui->cbTemperature, LV_Temperature);
    CHECK_AND_DISABLE_CB(ui->cbApparentTemperature, LV_ApparentTemperature);
    CHECK_AND_DISABLE_CB(ui->cbIndoorTemperature, LV_IndoorTemperature);
    CHECK_AND_DISABLE_CB(ui->cbWindChill, LV_WindChill);
    CHECK_AND_DISABLE_CB(ui->cbDewPoint, LV_DewPoint);
    CHECK_AND_DISABLE_CB(ui->cbHumidity, LV_Humidity);
    CHECK_AND_DISABLE_CB(ui->cbIndoorHumidity, LV_IndoorHumidity);
    CHECK_AND_DISABLE_CB(ui->cbRainfall, LV_StormRain);
    CHECK_AND_DISABLE_CB(ui->cbPressure, LV_Pressure);
    CHECK_AND_DISABLE_CB(ui->cbRainRate, LV_RainRate);
    CHECK_AND_DISABLE_CB(ui->cbWindSpeed, LV_WindSpeed);
    CHECK_AND_DISABLE_CB(ui->cbWindDirection, LV_WindDirection);
    CHECK_AND_DISABLE_CB(ui->cbUVIndex, LV_UVIndex);
    CHECK_AND_DISABLE_CB(ui->cbSolarRadiation, LV_SolarRadiation);
    CHECK_AND_DISABLE_CB(ui->cbConsoleBatteryVoltage, LV_BatteryVoltage);
    CHECK_AND_DISABLE_CB(ui->cbSoilMoisture1, LV_SoilMoisture1);
    CHECK_AND_DISABLE_CB(ui->cbSoilMoisture2, LV_SoilMoisture2);
    CHECK_AND_DISABLE_CB(ui->cbSoilMoisture3, LV_SoilMoisture3);
    CHECK_AND_DISABLE_CB(ui->cbSoilMoisture4, LV_SoilMoisture4);
    CHECK_AND_DISABLE_CB(ui->cbSoilTemperature1, LV_SoilTemperature1);
    CHECK_AND_DISABLE_CB(ui->cbSoilTemperature2, LV_SoilTemperature2);
    CHECK_AND_DISABLE_CB(ui->cbSoilTemperature3, LV_SoilTemperature3);
    CHECK_AND_DISABLE_CB(ui->cbSoilTemperature4, LV_SoilTemperature4);
    CHECK_AND_DISABLE_CB(ui->cbLeafWetness1, LV_LeafWetness1);
    CHECK_AND_DISABLE_CB(ui->cbLeafWetness2, LV_LeafWetness2);
    CHECK_AND_DISABLE_CB(ui->cbLeafTemperature1, LV_LeafTemperature1);
    CHECK_AND_DISABLE_CB(ui->cbLeafTemperature2, LV_LeafTemperature2);
    CHECK_AND_DISABLE_CB(ui->cbExtraHumidity1, LV_ExtraHumidity1);
    CHECK_AND_DISABLE_CB(ui->cbExtraHumidity2, LV_ExtraHumidity2);
    CHECK_AND_DISABLE_CB(ui->cbExtraTemperature1, LV_ExtraTemperature1);
    CHECK_AND_DISABLE_CB(ui->cbExtraTemperature2, LV_ExtraTemperature2);
    CHECK_AND_DISABLE_CB(ui->cbExtraTemperature3, LV_ExtraTemperature3);

    // Now give focus to the first tab with available columns!
    focusFirstAvailableTab();
}

LiveValues LiveColumnPickerWidget::getNewColumns() const {
    LiveValues columns = getColumns();

    // Clear the columns we started out with so we're only returning the
    // freshly checked options
    columns &= ~lockedColumns;

    return columns;
}
