#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>

ChartOptionsDialog::ChartOptionsDialog(bool solarAvailable,
                                       hardware_type_t hw_type, bool isWireless, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    if (!solarAvailable) {
        ui->lblSolar->setVisible(false);
        ui->cbSolarRadiation->setVisible(false);
        ui->cbUVIndex->setVisible(false);
        ui->cbEvapotranspiration->setVisible(false);
        ui->cbHighSolarRadiation->setVisible(false);
        ui->cbHighUVIndex->setVisible(false);
    }

    if (!isWireless) {
        ui->cbWirelessReception->setVisible(false);
    }

    if (hw_type != HW_DAVIS) {
        ui->cbHighTemperature->setVisible(false);
        ui->cbLowTemperature->setVisible(false);
        ui->cbWirelessReception->setVisible(false);
        ui->cbHighRainRate->setVisible(false);
        ui->cbGustDirection->setVisible(false);
    }

    // Buttons
    connect(this->ui->buttonBox, SIGNAL(accepted()), this, SLOT(checkAndAccept()));
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}

void ChartOptionsDialog::checkAndAccept() {
    columns.standard &= SC_NoColumns;

    if (ui->cbTemperature->isChecked())
        columns.standard |= SC_Temperature;
    if (ui->cbIndoorTemperature->isChecked())
        columns.standard |= SC_IndoorTemperature;
    if (ui->cbApparentTemperature->isChecked())
        columns.standard |= SC_ApparentTemperature;
    if (ui->cbDewPoint->isChecked())
        columns.standard |= SC_DewPoint;
    if (ui->cbWindChill->isChecked())
        columns.standard |= SC_WindChill;
    if (ui->cbHumidity->isChecked())
        columns.standard |= SC_Humidity;
    if (ui->cbIndoorHumidity->isChecked())
        columns.standard |= SC_IndoorHumidity;
    if (ui->cbPressure->isChecked())
        columns.standard |= SC_Pressure;
    if (ui->cbRainfall->isChecked())
        columns.standard |= SC_Rainfall;
    if (ui->cbAverageWindSpeed->isChecked())
        columns.standard |= SC_AverageWindSpeed;
    if (ui->cbGustWindSpeed->isChecked())
        columns.standard |= SC_GustWindSpeed;
    if (ui->cbWindDirection->isChecked())
        columns.standard |= SC_WindDirection;
    if (ui->cbUVIndex->isChecked())
        columns.standard |= SC_UV_Index;
    if (ui->cbSolarRadiation->isChecked())
        columns.standard |= SC_SolarRadiation;
    if (ui->cbHighTemperature->isChecked())
        columns.standard |= SC_HighTemperature;
    if (ui->cbLowTemperature->isChecked())
        columns.standard |= SC_LowTemperature;
    if (ui->cbHighSolarRadiation->isChecked())
        columns.standard |= SC_HighSolarRadiation;
    if (ui->cbHighUVIndex->isChecked())
        columns.standard |= SC_HighUVIndex;
    if (ui->cbWirelessReception->isChecked())
        columns.standard |= SC_Reception;
    if (ui->cbHighRainRate->isChecked())
        columns.standard |= SC_HighRainRate;
    if (ui->cbEvapotranspiration->isChecked())
        columns.standard |= SC_Evapotranspiration;
    if (ui->cbGustDirection->isChecked())
        columns.standard |= SC_GustWindDirection;

    if (columns.standard == SC_NoColumns && columns.extra == EC_NoColumns) {
        QMessageBox::information(this, "Data Sets",
                                 "At least one data set must be selected");
        return;
    }

    accept();
}

QDateTime ChartOptionsDialog::getStartTime() {
    return ui->timespan->getStartTime();
}

QDateTime ChartOptionsDialog::getEndTime() {
    return ui->timespan->getEndTime();
}

AggregateFunction ChartOptionsDialog::getAggregateFunction() {
    if (!ui->gbAggregate->isChecked())
        return AF_None;

    return ui->aggregateWidget->getAggregateFunction();
}

AggregateGroupType ChartOptionsDialog::getAggregateGroupType() {
    if (!ui->gbAggregate->isChecked())
        return AGT_None;

    return ui->aggregateWidget->getAggregateGroupType();
}

uint32_t ChartOptionsDialog::getCustomMinutes() {
    if (!ui->gbAggregate->isChecked())
        return 0;
    return ui->aggregateWidget->getCustomMinutes();
}

SampleColumns ChartOptionsDialog::getColumns() {
    return columns;
}

