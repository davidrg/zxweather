#include "addgraphdialog.h"
#include "ui_addgraphdialog.h"

AddGraphDialog::AddGraphDialog(SampleColumns availableColumns, bool solarAvailable, bool isWireless, hardware_type_t hw_type,
                               QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddGraphDialog)
{
    ui->setupUi(this);

    if (!solarAvailable) {
        ui->gbSolar->setVisible(false);
    }

    if (!isWireless) {
        ui->cbReception->setVisible(false);
    }

    if (hw_type != HW_DAVIS) {
        ui->cbHighTemperature->setVisible(false);
        ui->cbLowTemperature->setVisible(false);
        ui->cbHighRainRate->setVisible(false);
        ui->cbReception->setVisible(false);
        ui->cbGustDirection->setVisible(false);
    }

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    if ((availableColumns.standard & TEMPERATURE_COLUMNS) == 0
            && (availableColumns.extra & EXTRA_TEMPERATURE_COLUMNS) == 0) {
        // No temperature columns are available. Turn it off entirely.
        ui->gbTemperature->setEnabled(false);
    } else {
        // At least one is available.
        ui->cbTemperature->setEnabled(
                    availableColumns.standard.testFlag(SC_Temperature));
        ui->cbApparentTemperature->setEnabled(
                    availableColumns.standard.testFlag(SC_ApparentTemperature));
        ui->cbInsideTemperature->setEnabled(
                    availableColumns.standard.testFlag(SC_IndoorTemperature));
        ui->cbWindChill->setEnabled(availableColumns.standard.testFlag(SC_WindChill));
        ui->cbDewPoint->setEnabled(availableColumns.standard.testFlag(SC_DewPoint));
        ui->cbHighTemperature->setEnabled(availableColumns.standard.testFlag(SC_HighTemperature));
        ui->cbLowTemperature->setEnabled(availableColumns.standard.testFlag(SC_LowTemperature));
        ui->cbExtraTemperature1->setEnabled(availableColumns.extra.testFlag(EC_ExtraTemperature1));
        ui->cbExtraTemperature2->setEnabled(availableColumns.extra.testFlag(EC_ExtraTemperature2));;
        ui->cbExtraTemperature3->setEnabled(availableColumns.extra.testFlag(EC_ExtraTemperature3));
    }

    if ((availableColumns.standard & HUMIDITY_COLUMNS) == 0 && (availableColumns.extra & EXTRA_HUMIDITY_COLUMNS) == 0) {
        // No humidity columns available.
        ui->gbHumidity->setEnabled(false);
    } else {
        ui->cbHumidity->setEnabled(availableColumns.standard.testFlag(SC_Humidity));
        ui->cbInsideHumidity->setEnabled(
                    availableColumns.standard.testFlag(SC_IndoorHumidity));
        ui->cbExtraHumidity1->setEnabled(availableColumns.extra.testFlag(EC_ExtraHumidity1));
        ui->cbExtraHumidity2->setEnabled(availableColumns.extra.testFlag(EC_ExtraHumidity2));
    }

    if ((availableColumns.standard & OTHER_COLUMNS) == 0) {
        ui->gbOther->setEnabled(false);
    } else {
        ui->cbRainfall->setEnabled(availableColumns.standard.testFlag(SC_Rainfall));
        ui->cbBarometricPressure->setEnabled(
                    availableColumns.standard.testFlag(SC_Pressure));
        ui->cbHighRainRate->setEnabled(availableColumns.standard.testFlag(SC_HighRainRate));
        ui->cbReception->setEnabled(availableColumns.standard.testFlag(SC_Reception));
    }

    if ((availableColumns.standard & WIND_COLUMNS) == 0) {
        ui->gbWind->setEnabled(false);
    } else {
        ui->cbAverageWindSpeed->setEnabled(
                    availableColumns.standard.testFlag(SC_AverageWindSpeed));
        ui->cbGustWindSpeed->setEnabled(
                    availableColumns.standard.testFlag(SC_GustWindSpeed));
        ui->cbWindDirection->setEnabled(
                    availableColumns.standard.testFlag(SC_WindDirection));
        ui->cbGustDirection->setEnabled(availableColumns.standard.testFlag(SC_GustWindDirection));
    }

    if ((availableColumns.standard & SOLAR_COLUMNS) == 0) {
        ui->gbSolar->setEnabled(false);
    } else {
        ui->cbUVIndex->setEnabled(
                    availableColumns.standard.testFlag(SC_UV_Index));
        ui->cbSolarRadiation->setEnabled(
                    availableColumns.standard.testFlag(SC_SolarRadiation));
        ui->cbEvapotranspiration->setEnabled(
                    availableColumns.standard.testFlag(SC_Evapotranspiration));
        ui->cbHighSolarRadiation->setEnabled(
                    availableColumns.standard.testFlag(SC_HighSolarRadiation));
        ui->cbHighUVIndex->setEnabled(
                    availableColumns.standard.testFlag(SC_HighUVIndex));
    }

    if ((availableColumns.extra & SOIL_COLUMNS) == 0) {
        ui->gbSoil->setEnabled(false);
    } else {
        ui->cbSoilMoisture1->setEnabled(availableColumns.extra.testFlag(EC_SoilMoisture1));
        ui->cbSoilMoisture2->setEnabled(availableColumns.extra.testFlag(EC_SoilMoisture2));
        ui->cbSoilMoisture3->setEnabled(availableColumns.extra.testFlag(EC_SoilMoisture3));
        ui->cbSoilMoisture4->setEnabled(availableColumns.extra.testFlag(EC_SoilMoisture4));
        ui->cbSoilTemperature1->setEnabled(availableColumns.extra.testFlag(EC_SoilTemperature1));
        ui->cbSoilTemperature2->setEnabled(availableColumns.extra.testFlag(EC_SoilTemperature2));
        ui->cbSoilTemperature3->setEnabled(availableColumns.extra.testFlag(EC_SoilTemperature3));
        ui->cbSoilTemperature4->setEnabled(availableColumns.extra.testFlag(EC_SoilTemperature4));
    }

    if ((availableColumns.extra & LEAF_COLUMNS) == 0) {
        ui->gbLeaf->setEnabled(false);
    } else {
        ui->cbLeafWetness1->setEnabled(availableColumns.extra.testFlag(EC_LeafWetness1));
        ui->cbLeafWetness2->setEnabled(availableColumns.extra.testFlag(EC_LeafWetness2));
        ui->cbLeafTemperature1->setEnabled(availableColumns.extra.testFlag(EC_LeafTemperature1));
        ui->cbLeafTemperature2->setEnabled(availableColumns.extra.testFlag(EC_LeafTemperature2));
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
    ui->cbHighTemperature->setChecked(
                !ui->cbHighTemperature->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbLowTemperature->setChecked(
                !ui->cbLowTemperature->isEnabled() ||
                !ui->gbTemperature->isEnabled());
    ui->cbExtraTemperature1->setChecked(
                !ui->cbExtraTemperature1->isEnabled() ||
                !ui->cbExtraTemperature1->isEnabled());
    ui->cbExtraTemperature2->setChecked(
                !ui->cbExtraTemperature2->isEnabled() ||
                !ui->cbExtraTemperature2->isEnabled());
    ui->cbExtraTemperature3->setChecked(
                !ui->cbExtraTemperature3->isEnabled() ||
                !ui->cbExtraTemperature3->isEnabled());

    ui->cbHumidity->setChecked(
                !ui->cbHumidity->isEnabled() ||
                !ui->gbHumidity->isEnabled());
    ui->cbInsideHumidity->setChecked(
                !ui->cbInsideHumidity->isEnabled() ||
                !ui->gbHumidity->isEnabled());
    ui->cbExtraHumidity1->setChecked(
                !ui->cbExtraHumidity1->isEnabled() ||
                !ui->cbExtraHumidity1->isEnabled());
    ui->cbExtraHumidity2->setChecked(
                !ui->cbExtraHumidity2->isEnabled() ||
                !ui->cbExtraHumidity2->isEnabled());

    ui->cbRainfall->setChecked(
                !ui->cbRainfall->isEnabled() ||
                !ui->gbOther->isEnabled());
    ui->cbBarometricPressure->setChecked(
                !ui->cbBarometricPressure->isEnabled() ||
                !ui->gbOther->isEnabled());
    ui->cbHighRainRate->setChecked(
                !ui->cbHighRainRate->isEnabled() ||
                !ui->gbOther->isEnabled());
    ui->cbReception->setChecked(
                !ui->cbReception->isEnabled() ||
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
    ui->cbGustDirection->setChecked(
                !ui->cbGustDirection->isEnabled() ||
                !ui->gbWind->isEnabled());

    ui->cbUVIndex->setChecked(
                !ui->cbUVIndex->isEnabled() ||
                !ui->gbSolar->isEnabled());
    ui->cbSolarRadiation->setChecked(
                !ui->cbSolarRadiation->isEnabled() ||
                !ui->gbSolar->isEnabled());
    ui->cbEvapotranspiration->setChecked(
                !ui->cbEvapotranspiration->isEnabled() ||
                !ui->gbSolar->isEnabled());
    ui->cbHighSolarRadiation->setChecked(
                !ui->cbHighSolarRadiation->isEnabled() ||
                !ui->gbSolar->isEnabled());
    ui->cbHighUVIndex->setChecked(
                !ui->cbHighUVIndex->isEnabled() ||
                !ui->gbSolar->isEnabled());

    ui->cbLeafWetness1->setChecked(
                !ui->cbLeafWetness1->isEnabled() ||
                !ui->cbLeafWetness1->isEnabled());
    ui->cbLeafWetness2->setChecked(
                !ui->cbLeafWetness2->isEnabled() ||
                !ui->cbLeafWetness2->isEnabled());

    ui->cbLeafTemperature1->setChecked(
                !ui->cbLeafTemperature1->isEnabled() ||
                !ui->cbLeafTemperature1->isEnabled());
    ui->cbLeafTemperature2->setChecked(
                !ui->cbLeafTemperature2->isEnabled() ||
                !ui->cbLeafTemperature2->isEnabled());

    ui->cbSoilMoisture1->setChecked(
                !ui->cbSoilMoisture1->isEnabled() ||
                !ui->cbSoilMoisture1->isEnabled());
    ui->cbSoilMoisture2->setChecked(
                !ui->cbSoilMoisture2->isEnabled() ||
                !ui->cbSoilMoisture2->isEnabled());
    ui->cbSoilMoisture3->setChecked(
                !ui->cbSoilMoisture3->isEnabled() ||
                !ui->cbSoilMoisture3->isEnabled());
    ui->cbSoilMoisture4->setChecked(
                !ui->cbSoilMoisture4->isEnabled() ||
                !ui->cbSoilMoisture4->isEnabled());

    ui->cbSoilTemperature1->setChecked(
                !ui->cbSoilTemperature1->isEnabled() ||
                !ui->cbSoilTemperature1->isEnabled());
    ui->cbSoilTemperature2->setChecked(
                !ui->cbSoilTemperature2->isEnabled() ||
                !ui->cbSoilTemperature2->isEnabled());
    ui->cbSoilTemperature3->setChecked(
                !ui->cbSoilTemperature3->isEnabled() ||
                !ui->cbSoilTemperature3->isEnabled());
    ui->cbSoilTemperature4->setChecked(
                !ui->cbSoilTemperature4->isEnabled() ||
                !ui->cbSoilTemperature4->isEnabled());

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
            columns.standard |= SC_Temperature;
        if (COL_CHECKED(ui->cbInsideTemperature))
            columns.standard |= SC_IndoorTemperature;
        if (COL_CHECKED(ui->cbApparentTemperature))
            columns.standard |= SC_ApparentTemperature;
        if (COL_CHECKED(ui->cbWindChill))
            columns.standard |= SC_WindChill;
        if (COL_CHECKED(ui->cbDewPoint))
            columns.standard |= SC_DewPoint;
        if (COL_CHECKED(ui->cbHighTemperature))
            columns.standard |= SC_HighTemperature;
        if (COL_CHECKED(ui->cbLowTemperature))
            columns.standard |= SC_LowTemperature;
        if (COL_CHECKED(ui->cbExtraTemperature1))
            columns.extra |= EC_ExtraTemperature1;
        if (COL_CHECKED(ui->cbExtraTemperature2))
            columns.extra |= EC_ExtraTemperature2;
        if (COL_CHECKED(ui->cbExtraTemperature3))
            columns.extra |= EC_ExtraTemperature3;
    }

    if (ui->gbHumidity->isEnabled()) {
        if (COL_CHECKED(ui->cbHumidity))
            columns.standard |= SC_Humidity;
        if (COL_CHECKED(ui->cbInsideHumidity))
            columns.standard |= SC_IndoorHumidity;
        if (COL_CHECKED(ui->cbExtraHumidity1))
            columns.extra |= EC_ExtraHumidity1;
        if (COL_CHECKED(ui->cbExtraHumidity2))
            columns.extra |= EC_ExtraHumidity2;
    }

    if (ui->gbWind->isEnabled()) {
        if (COL_CHECKED(ui->cbAverageWindSpeed))
            columns.standard |= SC_AverageWindSpeed;
        if (COL_CHECKED(ui->cbGustWindSpeed))
            columns.standard |= SC_GustWindSpeed;
        if (COL_CHECKED(ui->cbWindDirection))
            columns.standard |= SC_WindDirection;
        if (COL_CHECKED(ui->cbGustDirection))
            columns.standard |= SC_GustWindDirection;
    }

    if (ui->gbOther->isEnabled()) {
        if (COL_CHECKED(ui->cbBarometricPressure))
            columns.standard |= SC_Pressure;
        if (COL_CHECKED(ui->cbRainfall))
            columns.standard |= SC_Rainfall;
        if (COL_CHECKED(ui->cbHighRainRate))
            columns.standard |= SC_HighRainRate;
        if (COL_CHECKED(ui->cbReception))
            columns.standard |= SC_Reception;
    }
    if (ui->gbSolar->isEnabled()) {
        if (COL_CHECKED(ui->cbUVIndex))
            columns.standard |= SC_UV_Index;
        if (COL_CHECKED(ui->cbSolarRadiation))
            columns.standard |= SC_SolarRadiation;
        if (COL_CHECKED(ui->cbEvapotranspiration))
            columns.standard |= SC_Evapotranspiration;
        if (COL_CHECKED(ui->cbHighSolarRadiation))
            columns.standard |= SC_HighSolarRadiation;
        if (COL_CHECKED(ui->cbHighUVIndex))
            columns.standard |= SC_HighUVIndex;
    }
    if (ui->gbLeaf->isEnabled()) {
        if (COL_CHECKED(ui->cbLeafWetness1))
            columns.extra |= EC_LeafWetness1;
        if (COL_CHECKED(ui->cbLeafWetness2))
            columns.extra |= EC_LeafWetness2;
        if (COL_CHECKED(ui->cbLeafTemperature1))
            columns.extra |= EC_LeafTemperature1;
        if (COL_CHECKED(ui->cbLeafTemperature2))
            columns.extra |= EC_LeafTemperature2;
    }
    if (ui->gbSoil->isEnabled()) {
        if(COL_CHECKED(ui->cbSoilMoisture1))
            columns.extra |= EC_SoilMoisture1;
        if(COL_CHECKED(ui->cbSoilMoisture2))
            columns.extra |= EC_SoilMoisture2;
        if(COL_CHECKED(ui->cbSoilMoisture3))
            columns.extra |= EC_SoilMoisture3;
        if(COL_CHECKED(ui->cbSoilMoisture4))
            columns.extra |= EC_SoilMoisture4;
        if(COL_CHECKED(ui->cbSoilTemperature1))
            columns.extra |= EC_SoilTemperature1;
        if(COL_CHECKED(ui->cbSoilTemperature2))
            columns.extra |= EC_SoilTemperature2;
        if(COL_CHECKED(ui->cbSoilTemperature3))
            columns.extra |= EC_SoilTemperature3;
        if(COL_CHECKED(ui->cbSoilTemperature4))
            columns.extra |= EC_SoilTemperature4;
    }

    return columns;
}

SampleColumns AddGraphDialog::supportedColumns(hardware_type_t hw_type, bool isWireless,
                                               bool hasSolar) {
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

    // TODO: Only the columns the station has configured
    columns.extra = ALL_EXTRA_COLUMNS;

    return columns;
}

