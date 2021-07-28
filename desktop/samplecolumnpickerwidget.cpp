#include "samplecolumnpickerwidget.h"
#include "ui_columnpickerwidget.h"

SampleColumnPickerWidget::SampleColumnPickerWidget(QWidget *parent):
    ColumnPickerWidget(parent)
{

}

void SampleColumnPickerWidget::configure(bool solarAvailable, hardware_type_t hw_type,
                                   bool isWireless, ExtraColumns extraColumns,
                                   QMap<ExtraColumn, QString> extraColumnNames,
                                   bool forecastRule) {

    configureUi(solarAvailable, true, hw_type, isWireless, extraColumns, extraColumnNames);

    if (hw_type == HW_DAVIS) {
        ui->cbForecastRule->setVisible(forecastRule);
        ui->cbForecastRule->setEnabled(forecastRule);
    }
}

#define CHECK_COLUMN(cb, col) if (cb->isChecked()) { columns.standard |= col; }
#define CHECK_EX_COLUMN(cb, col) if (cb->isChecked()) { columns.extra |= col; }

SampleColumns SampleColumnPickerWidget::getColumns() const {
    SampleColumns columns;

    columns.standard &= SC_NoColumns;
    columns.extra &= EC_NoColumns;

    CHECK_COLUMN(ui->cbTemperature, SC_Temperature);
    CHECK_COLUMN(ui->cbIndoorTemperature, SC_IndoorTemperature);
    CHECK_COLUMN(ui->cbApparentTemperature, SC_ApparentTemperature);
    CHECK_COLUMN(ui->cbDewPoint, SC_DewPoint);
    CHECK_COLUMN(ui->cbWindChill, SC_WindChill);
    CHECK_COLUMN(ui->cbHumidity, SC_Humidity);
    CHECK_COLUMN(ui->cbIndoorHumidity, SC_IndoorHumidity);
    CHECK_COLUMN(ui->cbPressure, SC_Pressure);
    CHECK_COLUMN(ui->cbRainfall, SC_Rainfall);
    CHECK_COLUMN(ui->cbWindSpeed, SC_AverageWindSpeed);
    CHECK_COLUMN(ui->cbGustWindSpeed, SC_GustWindSpeed);
    CHECK_COLUMN(ui->cbWindDirection, SC_WindDirection);
    CHECK_COLUMN(ui->cbUVIndex, SC_UV_Index);
    CHECK_COLUMN(ui->cbSolarRadiation, SC_SolarRadiation);
    CHECK_COLUMN(ui->cbHighTemperature, SC_HighTemperature);
    CHECK_COLUMN(ui->cbLowTemperature, SC_LowTemperature);
    CHECK_COLUMN(ui->cbHighSolarRadiation, SC_HighSolarRadiation);
    CHECK_COLUMN(ui->cbHighUVIndex, SC_HighUVIndex);
    CHECK_COLUMN(ui->cbWirelessReception, SC_Reception);
    CHECK_COLUMN(ui->cbRainRate, SC_HighRainRate);
    CHECK_COLUMN(ui->cbEvapotranspiration, SC_Evapotranspiration);
    CHECK_COLUMN(ui->cbGustDirection, SC_GustWindDirection);
    CHECK_COLUMN(ui->cbForecastRule, SC_ForecastRuleId);

    CHECK_EX_COLUMN(ui->cbLeafWetness1, EC_LeafWetness1);
    CHECK_EX_COLUMN(ui->cbLeafWetness2, EC_LeafWetness2);
    CHECK_EX_COLUMN(ui->cbLeafTemperature1, EC_LeafTemperature1);
    CHECK_EX_COLUMN(ui->cbLeafTemperature2, EC_LeafTemperature2);
    CHECK_EX_COLUMN(ui->cbSoilMoisture1, EC_SoilMoisture1);
    CHECK_EX_COLUMN(ui->cbSoilMoisture2, EC_SoilMoisture2);
    CHECK_EX_COLUMN(ui->cbSoilMoisture3, EC_SoilMoisture3);
    CHECK_EX_COLUMN(ui->cbSoilMoisture4, EC_SoilMoisture4);
    CHECK_EX_COLUMN(ui->cbSoilTemperature1, EC_SoilTemperature1);
    CHECK_EX_COLUMN(ui->cbSoilTemperature2, EC_SoilTemperature2);
    CHECK_EX_COLUMN(ui->cbSoilTemperature3, EC_SoilTemperature3);
    CHECK_EX_COLUMN(ui->cbSoilTemperature4, EC_SoilTemperature4);
    CHECK_EX_COLUMN(ui->cbExtraHumidity1, EC_ExtraHumidity1);
    CHECK_EX_COLUMN(ui->cbExtraHumidity2, EC_ExtraHumidity2);
    CHECK_EX_COLUMN(ui->cbExtraTemperature1, EC_ExtraTemperature1);
    CHECK_EX_COLUMN(ui->cbExtraTemperature2, EC_ExtraTemperature2);
    CHECK_EX_COLUMN(ui->cbExtraTemperature3, EC_ExtraTemperature3);

    return columns;
}

#define CHECK_AND_DISABLE_CB(cb, col) cb->setEnabled(!columns.standard.testFlag(col)); cb->setChecked(columns.standard.testFlag(col));
#define CHECK_AND_DISABLE_EX_CB(cb, col) cb->setEnabled(!columns.extra.testFlag(col)); cb->setChecked(columns.extra.testFlag(col));

void SampleColumnPickerWidget::checkAndLockColumns(SampleColumns columns) {
    lockedColumns = columns;

    CHECK_AND_DISABLE_CB(ui->cbTemperature, SC_Temperature);
    CHECK_AND_DISABLE_CB(ui->cbIndoorTemperature, SC_IndoorTemperature);
    CHECK_AND_DISABLE_CB(ui->cbApparentTemperature, SC_ApparentTemperature);
    CHECK_AND_DISABLE_CB(ui->cbDewPoint, SC_DewPoint);
    CHECK_AND_DISABLE_CB(ui->cbWindChill, SC_WindChill);
    CHECK_AND_DISABLE_CB(ui->cbHumidity, SC_Humidity);
    CHECK_AND_DISABLE_CB(ui->cbIndoorHumidity, SC_IndoorHumidity);
    CHECK_AND_DISABLE_CB(ui->cbPressure, SC_Pressure);
    CHECK_AND_DISABLE_CB(ui->cbRainfall, SC_Rainfall);
    CHECK_AND_DISABLE_CB(ui->cbWindSpeed, SC_AverageWindSpeed);
    CHECK_AND_DISABLE_CB(ui->cbGustWindSpeed, SC_GustWindSpeed);
    CHECK_AND_DISABLE_CB(ui->cbWindDirection, SC_WindDirection);
    CHECK_AND_DISABLE_CB(ui->cbUVIndex, SC_UV_Index);
    CHECK_AND_DISABLE_CB(ui->cbSolarRadiation, SC_SolarRadiation);
    CHECK_AND_DISABLE_CB(ui->cbHighTemperature, SC_HighTemperature);
    CHECK_AND_DISABLE_CB(ui->cbLowTemperature, SC_LowTemperature);
    CHECK_AND_DISABLE_CB(ui->cbHighSolarRadiation, SC_HighSolarRadiation);
    CHECK_AND_DISABLE_CB(ui->cbHighUVIndex, SC_HighUVIndex);
    CHECK_AND_DISABLE_CB(ui->cbWirelessReception, SC_Reception);
    CHECK_AND_DISABLE_CB(ui->cbRainRate, SC_HighRainRate);
    CHECK_AND_DISABLE_CB(ui->cbEvapotranspiration, SC_Evapotranspiration);
    CHECK_AND_DISABLE_CB(ui->cbGustDirection, SC_GustWindDirection);
    CHECK_AND_DISABLE_CB(ui->cbForecastRule, SC_ForecastRuleId);

    CHECK_AND_DISABLE_EX_CB(ui->cbLeafWetness1, EC_LeafWetness1);
    CHECK_AND_DISABLE_EX_CB(ui->cbLeafWetness2, EC_LeafWetness2);
    CHECK_AND_DISABLE_EX_CB(ui->cbLeafTemperature1, EC_LeafTemperature1);
    CHECK_AND_DISABLE_EX_CB(ui->cbLeafTemperature2, EC_LeafTemperature2);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilMoisture1, EC_SoilMoisture1);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilMoisture2, EC_SoilMoisture2);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilMoisture3, EC_SoilMoisture3);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilMoisture4, EC_SoilMoisture4);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilTemperature1, EC_SoilTemperature1);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilTemperature2, EC_SoilTemperature2);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilTemperature3, EC_SoilTemperature3);
    CHECK_AND_DISABLE_EX_CB(ui->cbSoilTemperature4, EC_SoilTemperature4);
    CHECK_AND_DISABLE_EX_CB(ui->cbExtraHumidity1, EC_ExtraHumidity1);
    CHECK_AND_DISABLE_EX_CB(ui->cbExtraHumidity2, EC_ExtraHumidity2);
    CHECK_AND_DISABLE_EX_CB(ui->cbExtraTemperature1, EC_ExtraTemperature1);
    CHECK_AND_DISABLE_EX_CB(ui->cbExtraTemperature2, EC_ExtraTemperature2);
    CHECK_AND_DISABLE_EX_CB(ui->cbExtraTemperature3, EC_ExtraTemperature3);

    // Now give focus to the first tab with available columns!
    focusFirstAvailableTab();
}

SampleColumns SampleColumnPickerWidget::getNewColumns() const {
    SampleColumns columns = getColumns();

    // Clear the columns we started out with so we're only returning the
    // freshly checked options
    columns.standard &= ~lockedColumns.standard;
    columns.extra &= ~lockedColumns.extra;

    return columns;
}
