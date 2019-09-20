#include "addgraphdialog.h"
#include "ui_addgraphdialog.h"

AddGraphDialog::AddGraphDialog(SampleColumns availableColumns,
                               bool solarAvailable,
                               bool isWireless,
                               hardware_type_t hw_type,
                               ExtraColumns extraColumns,
                               QMap<ExtraColumn, QString> extraColumnNames,
                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddGraphDialog)
{
    ui->setupUi(this);

    ui->columnPicker->configure(solarAvailable, hw_type, isWireless, extraColumns, extraColumnNames);

    SampleColumns unavailableColumns;
    unavailableColumns.standard = ~availableColumns.standard;
    unavailableColumns.extra = ~availableColumns.extra;

    ui->columnPicker->checkAndLockColumns(unavailableColumns);
}

AddGraphDialog::~AddGraphDialog()
{
    delete ui;
}

SampleColumns AddGraphDialog::selectedColumns()
{
    ui->columnPicker->getNewColumns();
}

SampleColumns AddGraphDialog::supportedColumns(hardware_type_t hw_type, bool isWireless,
                                               bool hasSolar, ExtraColumns extraColumns) {
    SampleColumns columns;

    // Standard columns supported by all weather stations
    columns.standard = SC_Temperature | SC_IndoorTemperature | SC_ApparentTemperature
            | SC_WindChill | SC_DewPoint | SC_Humidity | SC_IndoorHumidity
            | SC_AverageWindSpeed | SC_GustWindSpeed | SC_WindDirection
            | SC_Pressure | SC_Rainfall
            ;

    if (hw_type == HW_DAVIS) {
        // Columns supported by all Davis Vantage Pro2 and Vue stations
        columns.standard |= SC_HighTemperature | SC_LowTemperature | SC_HighRainRate
                | SC_GustWindDirection;

        if (isWireless) {
            // Columns supported by all Wireless davis stations
            columns.standard |= SC_Reception;
        }

        if (hasSolar) {
            // Columns supported by the Vantage Pro2 Plus
            columns.standard |= SC_UV_Index | SC_SolarRadiation | SC_Evapotranspiration
                    | SC_HighSolarRadiation | SC_HighUVIndex;
        }
    }

    columns.extra = extraColumns;

    return columns;
}

