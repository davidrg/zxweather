#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>
#include <QCheckbox>

ChartOptionsDialog::ChartOptionsDialog(bool solarAvailable,
                                       hardware_type_t hw_type, bool isWireless, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    if (!solarAvailable) {
        ui->gbSolar->setVisible(false);
        ui->gbSolarHighs->setVisible(false);
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
        ui->gbTemperatureHighLow->setVisible(false);
        ui->cbHighTemperature->setVisible(false);
        ui->cbLowTemperature->setVisible(false);
        ui->cbWirelessReception->setVisible(false);
        ui->cbHighRainRate->setVisible(false);
        ui->cbGustDirection->setVisible(false);
    }

    // Buttons
    connect(this->ui->buttonBox, SIGNAL(accepted()), this, SLOT(checkAndAccept()));

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        tabLabels.insert(i, ui->tabWidget->tabText(i));
    }

    // Checkboxes
    foreach (QCheckBox* cb, this->findChildren<QCheckBox*>()) {
        connect(cb, SIGNAL(toggled(bool)), this, SLOT(checkboxToggled(bool)));
    }
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}

// Show how many checkboxes are checked in the title for each tab
void ChartOptionsDialog::checkboxToggled(bool checked) {
    Q_UNUSED(checked);

    QCheckBox *cb = qobject_cast<QCheckBox*>(sender());

    // Try and find the tab the checkbox is on
    QWidget *parent = qobject_cast<QWidget*>(cb->parentWidget());
    if (parent == NULL || ui->tabWidget->indexOf(parent) == -1) {
        parent = qobject_cast<QWidget*>(cb->parentWidget()->parentWidget());
        if (parent == NULL || ui->tabWidget->indexOf(parent) == -1) {
            parent = qobject_cast<QWidget*>(cb->parentWidget()->parentWidget()->parentWidget());
            if (parent == NULL || ui->tabWidget->indexOf(parent) == -1) {
                // We tried looking three levels up and could find anything. Its probably not
                // on a tab. Give up.
                qDebug() << "Parent tab not found!";
                return;
            }
        }
    }

    int checkedCount = 0;
    foreach (QCheckBox* checkbox, parent->findChildren<QCheckBox*>()) {
        if (checkbox->isChecked()) {
            checkedCount++;
        }
    }

    int index = ui->tabWidget->indexOf(parent);

    if (index >= 0) {
        QString label = tabLabels[index];
        if (checkedCount > 0) {
            label = (label + " (%1)").arg(checkedCount);
        }
        ui->tabWidget->setTabText(index, label);
    }

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
    if (ui->cbLeafWetness1->isChecked()) columns.extra |= EC_LeafWetness1;
    if (ui->cbLeafWetness2->isChecked()) columns.extra |= EC_LeafWetness2;
    if (ui->cbLeafTemperature1->isChecked()) columns.extra |= EC_LeafTemperature1;
    if (ui->cbLeafTemperature2->isChecked()) columns.extra |= EC_LeafTemperature2;
    if (ui->cbSoilMoisture1->isChecked()) columns.extra |= EC_SoilMoisture1;
    if (ui->cbSoilMoisture2->isChecked()) columns.extra |= EC_SoilMoisture2;
    if (ui->cbSoilMoisture3->isChecked()) columns.extra |= EC_SoilMoisture3;
    if (ui->cbSoilMoisture4->isChecked()) columns.extra |= EC_SoilMoisture4;
    if (ui->cbSoilTemperature1->isChecked()) columns.extra |= EC_SoilTemperature1;
    if (ui->cbSoilTemperature2->isChecked()) columns.extra |= EC_SoilTemperature2;
    if (ui->cbSoilTemperature3->isChecked()) columns.extra |= EC_SoilTemperature3;
    if (ui->cbSoilTemperature4->isChecked()) columns.extra |= EC_SoilTemperature4;
    if (ui->cbExtraHumidity1->isChecked()) columns.extra |= EC_ExtraHumidity1;
    if (ui->cbExtraHumidity2->isChecked()) columns.extra |= EC_ExtraHumidity2;
    if (ui->cbExtraTemperature1->isChecked()) columns.extra |= EC_ExtraTemperature1;
    if (ui->cbExtraTemperature2->isChecked()) columns.extra |= EC_ExtraTemperature2;
    if (ui->cbExtraTemperature3->isChecked()) columns.extra |= EC_ExtraTemperature3;

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

