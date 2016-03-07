#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>

ChartOptionsDialog::ChartOptionsDialog(bool solarAvailable, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    if (!solarAvailable) {
        ui->lblSolar->setVisible(false);
        ui->cbSolarRadiation->setVisible(false);
        ui->cbUVIndex->setVisible(false);
    }

    // Buttons
    connect(this->ui->buttonBox, SIGNAL(accepted()), this, SLOT(checkAndAccept()));
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}

void ChartOptionsDialog::checkAndAccept() {
    columns &= SC_NoColumns;

    if (ui->cbTemperature->isChecked())
        columns |= SC_Temperature;
    if (ui->cbIndoorTemperature->isChecked())
        columns |= SC_IndoorTemperature;
    if (ui->cbApparentTemperature->isChecked())
        columns |= SC_ApparentTemperature;
    if (ui->cbDewPoint->isChecked())
        columns |= SC_DewPoint;
    if (ui->cbWindChill->isChecked())
        columns |= SC_WindChill;
    if (ui->cbHumidity->isChecked())
        columns |= SC_Humidity;
    if (ui->cbIndoorHumidity->isChecked())
        columns |= SC_IndoorHumidity;
    if (ui->cbPressure->isChecked())
        columns |= SC_Pressure;
    if (ui->cbRainfall->isChecked())
        columns |= SC_Rainfall;
    if (ui->cbAverageWindSpeed->isChecked())
        columns |= SC_AverageWindSpeed;
    if (ui->cbGustWindSpeed->isChecked())
        columns |= SC_GustWindSpeed;
    if (ui->cbWindDirection->isChecked())
        columns |= SC_WindDirection;
    if (ui->cbUVIndex->isChecked())
        columns |= SC_UV_Index;
    if (ui->cbSolarRadiation->isChecked())
        columns |= SC_SolarRadiation;

    if (columns == SC_NoColumns) {
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

