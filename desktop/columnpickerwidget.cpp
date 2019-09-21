#include "columnpickerwidget.h"
#include "ui_columnpickerwidget.h"

#include <QtDebug>

ColumnPickerWidget::ColumnPickerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ColumnPickerWidget)
{
    ui->setupUi(this);
}

ColumnPickerWidget::~ColumnPickerWidget()
{
    delete ui;
}


void ColumnPickerWidget::configure(bool solarAvailable, hardware_type_t hw_type,
                                   bool isWireless, ExtraColumns extraColumns,
                                   QMap<ExtraColumn, QString> extraColumnNames) {
    // TODO: Get all this from the data sources column config
    if (!solarAvailable) {
        ui->gbSolar->setVisible(false);
        ui->gbSolarHighs->setVisible(false);
        ui->cbSolarRadiation->setVisible(false);
        ui->cbUVIndex->setVisible(false);
        ui->cbEvapotranspiration->setVisible(false);
        ui->cbHighSolarRadiation->setVisible(false);
        ui->cbHighUVIndex->setVisible(false);

        ui->gbSolar->setEnabled(false);
        ui->gbSolarHighs->setEnabled(false);
        ui->cbSolarRadiation->setEnabled(false);
        ui->cbUVIndex->setEnabled(false);
        ui->cbEvapotranspiration->setEnabled(false);
        ui->cbHighSolarRadiation->setEnabled(false);
        ui->cbHighUVIndex->setEnabled(false);
    }

    if (!isWireless) {
        ui->cbWirelessReception->setVisible(false);
        ui->cbWirelessReception->setEnabled(false);
    }

    if (hw_type != HW_DAVIS) {
        ui->gbTemperatureHighLow->setVisible(false);
        ui->cbHighTemperature->setVisible(false);
        ui->cbLowTemperature->setVisible(false);
        ui->cbWirelessReception->setVisible(false);
        ui->cbHighRainRate->setVisible(false);
        ui->cbGustDirection->setVisible(false);

        ui->gbTemperatureHighLow->setEnabled(false);
        ui->cbHighTemperature->setEnabled(false);
        ui->cbLowTemperature->setEnabled(false);
        ui->cbWirelessReception->setEnabled(false);
        ui->cbHighRainRate->setEnabled(false);
        ui->cbGustDirection->setEnabled(false);
    }

    // Extra column config
    ui->cbSoilMoisture1->setVisible(extraColumns.testFlag(EC_SoilMoisture1));
    ui->cbSoilMoisture2->setVisible(extraColumns.testFlag(EC_SoilMoisture2));
    ui->cbSoilMoisture3->setVisible(extraColumns.testFlag(EC_SoilMoisture3));
    ui->cbSoilMoisture4->setVisible(extraColumns.testFlag(EC_SoilMoisture4));

    ui->cbSoilMoisture1->setEnabled(extraColumns.testFlag(EC_SoilMoisture1));
    ui->cbSoilMoisture2->setEnabled(extraColumns.testFlag(EC_SoilMoisture2));
    ui->cbSoilMoisture3->setEnabled(extraColumns.testFlag(EC_SoilMoisture3));
    ui->cbSoilMoisture4->setEnabled(extraColumns.testFlag(EC_SoilMoisture4));

    ui->gbSoilMoisture->setVisible(
                extraColumns.testFlag(EC_SoilMoisture1) ||
                extraColumns.testFlag(EC_SoilMoisture2) ||
                extraColumns.testFlag(EC_SoilMoisture3) ||
                extraColumns.testFlag(EC_SoilMoisture4)
                );

    ui->cbSoilTemperature1->setVisible(extraColumns.testFlag(EC_SoilTemperature1));
    ui->cbSoilTemperature2->setVisible(extraColumns.testFlag(EC_SoilTemperature2));
    ui->cbSoilTemperature3->setVisible(extraColumns.testFlag(EC_SoilTemperature3));
    ui->cbSoilTemperature4->setVisible(extraColumns.testFlag(EC_SoilTemperature4));

    ui->cbSoilTemperature1->setEnabled(extraColumns.testFlag(EC_SoilTemperature1));
    ui->cbSoilTemperature2->setEnabled(extraColumns.testFlag(EC_SoilTemperature2));
    ui->cbSoilTemperature3->setEnabled(extraColumns.testFlag(EC_SoilTemperature3));
    ui->cbSoilTemperature4->setEnabled(extraColumns.testFlag(EC_SoilTemperature4));

    ui->gbSoilTemperature->setVisible(
                extraColumns.testFlag(EC_SoilTemperature1) ||
                extraColumns.testFlag(EC_SoilTemperature2) ||
                extraColumns.testFlag(EC_SoilTemperature3) ||
                extraColumns.testFlag(EC_SoilTemperature4)
                );

    ui->cbLeafWetness1->setVisible(extraColumns.testFlag(EC_LeafWetness1));
    ui->cbLeafWetness2->setVisible(extraColumns.testFlag(EC_LeafWetness2));

    ui->cbLeafWetness1->setEnabled(extraColumns.testFlag(EC_LeafWetness1));
    ui->cbLeafWetness2->setEnabled(extraColumns.testFlag(EC_LeafWetness2));

    ui->gbLeafWetness->setVisible(
                extraColumns.testFlag(EC_LeafWetness1) ||
                extraColumns.testFlag(EC_LeafWetness2)
                );

    ui->cbLeafTemperature1->setVisible(extraColumns.testFlag(EC_LeafTemperature1));
    ui->cbLeafTemperature2->setVisible(extraColumns.testFlag(EC_LeafTemperature2));

    ui->cbLeafTemperature1->setEnabled(extraColumns.testFlag(EC_LeafTemperature1));
    ui->cbLeafTemperature2->setEnabled(extraColumns.testFlag(EC_LeafTemperature2));

    ui->gbLeafTemperature->setVisible(
                extraColumns.testFlag(EC_LeafTemperature1) ||
                extraColumns.testFlag(EC_LeafTemperature2)
                );

    ui->tabLeafAndSoil->layout()->update();
    ui->tabWidget->setTabEnabled(2,
                extraColumns.testFlag(EC_SoilMoisture1) ||
                extraColumns.testFlag(EC_SoilMoisture2) ||
                extraColumns.testFlag(EC_SoilMoisture3) ||
                extraColumns.testFlag(EC_SoilMoisture4) ||
                extraColumns.testFlag(EC_SoilTemperature1) ||
                extraColumns.testFlag(EC_SoilTemperature2) ||
                extraColumns.testFlag(EC_SoilTemperature3) ||
                extraColumns.testFlag(EC_SoilTemperature4) ||
                extraColumns.testFlag(EC_LeafWetness1) ||
                extraColumns.testFlag(EC_LeafWetness2) ||
                extraColumns.testFlag(EC_LeafTemperature1) ||
                extraColumns.testFlag(EC_LeafTemperature2)
                );

    ui->cbExtraHumidity1->setVisible(extraColumns.testFlag(EC_ExtraHumidity1));
    ui->cbExtraHumidity2->setVisible(extraColumns.testFlag(EC_ExtraHumidity2));

    ui->cbExtraHumidity1->setEnabled(extraColumns.testFlag(EC_ExtraHumidity1));
    ui->cbExtraHumidity2->setEnabled(extraColumns.testFlag(EC_ExtraHumidity2));

    ui->gbExtraHumidity->setVisible(
                extraColumns.testFlag(EC_ExtraHumidity1) ||
                extraColumns.testFlag(EC_ExtraHumidity2)
                );

    ui->cbExtraTemperature1->setVisible(extraColumns.testFlag(EC_ExtraTemperature1));
    ui->cbExtraTemperature2->setVisible(extraColumns.testFlag(EC_ExtraTemperature2));
    ui->cbExtraTemperature3->setVisible(extraColumns.testFlag(EC_ExtraTemperature3));

    ui->cbExtraTemperature1->setEnabled(extraColumns.testFlag(EC_ExtraTemperature1));
    ui->cbExtraTemperature2->setEnabled(extraColumns.testFlag(EC_ExtraTemperature2));
    ui->cbExtraTemperature3->setEnabled(extraColumns.testFlag(EC_ExtraTemperature3));

    ui->gbExtraTemperature->setVisible(
                extraColumns.testFlag(EC_ExtraTemperature1) ||
                extraColumns.testFlag(EC_ExtraTemperature2) ||
                extraColumns.testFlag(EC_ExtraTemperature3)
                );

    ui->tabWidget->setTabEnabled(3,
                extraColumns.testFlag(EC_ExtraHumidity1) ||
                extraColumns.testFlag(EC_ExtraHumidity2) ||
                extraColumns.testFlag(EC_ExtraTemperature1) ||
                extraColumns.testFlag(EC_ExtraTemperature2) ||
                extraColumns.testFlag(EC_ExtraTemperature3)
                );

    if (extraColumns.testFlag(EC_SoilMoisture1))
        ui->cbSoilMoisture1->setText(extraColumnNames[EC_SoilMoisture1]);
    if (extraColumns.testFlag(EC_SoilMoisture2))
        ui->cbSoilMoisture2->setText(extraColumnNames[EC_SoilMoisture2]);
    if (extraColumns.testFlag(EC_SoilMoisture3))
        ui->cbSoilMoisture3->setText(extraColumnNames[EC_SoilMoisture3]);
    if (extraColumns.testFlag(EC_SoilMoisture4))
        ui->cbSoilMoisture4->setText(extraColumnNames[EC_SoilMoisture4]);
    if (extraColumns.testFlag(EC_SoilTemperature1))
        ui->cbSoilTemperature1->setText(extraColumnNames[EC_SoilTemperature1]);
    if (extraColumns.testFlag(EC_SoilTemperature2))
        ui->cbSoilTemperature2->setText(extraColumnNames[EC_SoilTemperature2]);
    if (extraColumns.testFlag(EC_SoilTemperature3))
        ui->cbSoilTemperature3->setText(extraColumnNames[EC_SoilTemperature3]);
    if (extraColumns.testFlag(EC_SoilTemperature4))
        ui->cbSoilTemperature4->setText(extraColumnNames[EC_SoilTemperature4]);
    if (extraColumns.testFlag(EC_LeafWetness1))
        ui->cbLeafWetness1->setText(extraColumnNames[EC_LeafWetness1]);
    if (extraColumns.testFlag(EC_LeafWetness2))
        ui->cbLeafWetness2->setText(extraColumnNames[EC_LeafWetness2]);
    if (extraColumns.testFlag(EC_LeafTemperature1))
        ui->cbLeafTemperature1->setText(extraColumnNames[EC_LeafTemperature1]);
    if (extraColumns.testFlag(EC_LeafTemperature2))
        ui->cbLeafTemperature2->setText(extraColumnNames[EC_LeafTemperature2]);
    if (extraColumns.testFlag(EC_ExtraHumidity1))
        ui->cbExtraHumidity1->setText(extraColumnNames[EC_ExtraHumidity1]);
    if (extraColumns.testFlag(EC_ExtraHumidity2))
        ui->cbExtraHumidity2->setText(extraColumnNames[EC_ExtraHumidity2]);
    if (extraColumns.testFlag(EC_ExtraTemperature1))
        ui->cbExtraTemperature1->setText(extraColumnNames[EC_ExtraTemperature1]);
    if (extraColumns.testFlag(EC_ExtraTemperature2))
        ui->cbExtraTemperature2->setText(extraColumnNames[EC_ExtraTemperature2]);
    if (extraColumns.testFlag(EC_ExtraTemperature3))
        ui->cbExtraTemperature3->setText(extraColumnNames[EC_ExtraTemperature3]);

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        tabLabels.insert(i, ui->tabWidget->tabText(i));
    }

    // Checkboxes
    foreach (QCheckBox* cb, this->findChildren<QCheckBox*>()) {
        connect(cb, SIGNAL(toggled(bool)), this, SLOT(checkboxToggled(bool)));
    }
}

// Show how many checkboxes are checked in the title for each tab
void ColumnPickerWidget::checkboxToggled(bool checked) {
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
        if (checkbox->isChecked() && checkbox->isEnabled()) {
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

SampleColumns ColumnPickerWidget::getColumns() const {
    SampleColumns columns;

    columns.standard &= SC_NoColumns;
    columns.extra &= EC_NoColumns;

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

    return columns;
}

void ColumnPickerWidget::checkAndLockColumns(SampleColumns columns) {
    lockedColumns = columns;

    // Disable all the specified columns.
    ui->cbTemperature->setEnabled(!columns.standard.testFlag(SC_Temperature));
    ui->cbApparentTemperature->setEnabled(!columns.standard.testFlag(SC_ApparentTemperature));
    ui->cbIndoorTemperature->setEnabled(!columns.standard.testFlag(SC_IndoorTemperature));
    ui->cbWindChill->setEnabled(!columns.standard.testFlag(SC_WindChill));
    ui->cbDewPoint->setEnabled(!columns.standard.testFlag(SC_DewPoint));
    ui->cbHighTemperature->setEnabled(!columns.standard.testFlag(SC_HighTemperature));
    ui->cbLowTemperature->setEnabled(!columns.standard.testFlag(SC_LowTemperature));

    ui->cbHumidity->setEnabled(!columns.standard.testFlag(SC_Humidity));
    ui->cbIndoorHumidity->setEnabled(!columns.standard.testFlag(SC_IndoorHumidity));

    ui->cbRainfall->setEnabled(!columns.standard.testFlag(SC_Rainfall));
    ui->cbPressure->setEnabled(!columns.standard.testFlag(SC_Pressure));
    ui->cbHighRainRate->setEnabled(!columns.standard.testFlag(SC_HighRainRate));

    ui->cbWirelessReception->setEnabled(!columns.standard.testFlag(SC_Reception));

    ui->cbAverageWindSpeed->setEnabled(!columns.standard.testFlag(SC_AverageWindSpeed));
    ui->cbGustWindSpeed->setEnabled(!columns.standard.testFlag(SC_GustWindSpeed));
    ui->cbWindDirection->setEnabled(!columns.standard.testFlag(SC_WindDirection));
    ui->cbGustDirection->setEnabled(!columns.standard.testFlag(SC_GustWindDirection));

    ui->cbUVIndex->setEnabled(!columns.standard.testFlag(SC_UV_Index));
    ui->cbSolarRadiation->setEnabled(!columns.standard.testFlag(SC_SolarRadiation));
    ui->cbEvapotranspiration->setEnabled(!columns.standard.testFlag(SC_Evapotranspiration));
    ui->cbHighSolarRadiation->setEnabled(!columns.standard.testFlag(SC_HighSolarRadiation));
    ui->cbHighUVIndex->setEnabled(!columns.standard.testFlag(SC_HighUVIndex));

    ui->cbSoilMoisture1->setEnabled(!columns.extra.testFlag(EC_SoilMoisture1));
    ui->cbSoilMoisture2->setEnabled(!columns.extra.testFlag(EC_SoilMoisture2));
    ui->cbSoilMoisture3->setEnabled(!columns.extra.testFlag(EC_SoilMoisture3));
    ui->cbSoilMoisture4->setEnabled(!columns.extra.testFlag(EC_SoilMoisture4));
    ui->cbSoilTemperature1->setEnabled(!columns.extra.testFlag(EC_SoilTemperature1));
    ui->cbSoilTemperature2->setEnabled(!columns.extra.testFlag(EC_SoilTemperature2));
    ui->cbSoilTemperature3->setEnabled(!columns.extra.testFlag(EC_SoilTemperature3));
    ui->cbSoilTemperature4->setEnabled(!columns.extra.testFlag(EC_SoilTemperature4));

    ui->cbLeafWetness1->setEnabled(!columns.extra.testFlag(EC_LeafWetness1));
    ui->cbLeafWetness2->setEnabled(!columns.extra.testFlag(EC_LeafWetness2));
    ui->cbLeafTemperature1->setEnabled(!columns.extra.testFlag(EC_LeafTemperature1));
    ui->cbLeafTemperature2->setEnabled(!columns.extra.testFlag(EC_LeafTemperature2));

    ui->cbExtraHumidity1->setEnabled(!columns.extra.testFlag(EC_ExtraHumidity1));
    ui->cbExtraHumidity2->setEnabled(!columns.extra.testFlag(EC_ExtraHumidity2));

    ui->cbExtraTemperature1->setEnabled(!columns.extra.testFlag(EC_ExtraTemperature1));
    ui->cbExtraTemperature2->setEnabled(!columns.extra.testFlag(EC_ExtraTemperature2));
    ui->cbExtraTemperature3->setEnabled(!columns.extra.testFlag(EC_ExtraTemperature3));


    // Check all of the now disabled checkboxes. Ideally we would do this with a loop but we don't want to
    // end up checking disabled checkboxes for hardware that isn't present.
    ui->cbTemperature->setChecked(columns.standard.testFlag(SC_Temperature));
    ui->cbApparentTemperature->setChecked(columns.standard.testFlag(SC_ApparentTemperature));
    ui->cbIndoorTemperature->setChecked(columns.standard.testFlag(SC_IndoorTemperature));
    ui->cbWindChill->setChecked(columns.standard.testFlag(SC_WindChill));
    ui->cbDewPoint->setChecked(columns.standard.testFlag(SC_DewPoint));
    ui->cbHighTemperature->setChecked(columns.standard.testFlag(SC_HighTemperature));
    ui->cbLowTemperature->setChecked(columns.standard.testFlag(SC_LowTemperature));

    ui->cbHumidity->setChecked(columns.standard.testFlag(SC_Humidity));
    ui->cbIndoorHumidity->setChecked(columns.standard.testFlag(SC_IndoorHumidity));

    ui->cbRainfall->setChecked(columns.standard.testFlag(SC_Rainfall));
    ui->cbPressure->setChecked(columns.standard.testFlag(SC_Pressure));
    ui->cbHighRainRate->setChecked(columns.standard.testFlag(SC_HighRainRate));

    ui->cbWirelessReception->setChecked(columns.standard.testFlag(SC_Reception));

    ui->cbAverageWindSpeed->setChecked(columns.standard.testFlag(SC_AverageWindSpeed));
    ui->cbGustWindSpeed->setChecked(columns.standard.testFlag(SC_GustWindSpeed));
    ui->cbWindDirection->setChecked(columns.standard.testFlag(SC_WindDirection));
    ui->cbGustDirection->setChecked(columns.standard.testFlag(SC_GustWindDirection));

    ui->cbUVIndex->setChecked(columns.standard.testFlag(SC_UV_Index));
    ui->cbSolarRadiation->setChecked(columns.standard.testFlag(SC_SolarRadiation));
    ui->cbEvapotranspiration->setChecked(columns.standard.testFlag(SC_Evapotranspiration));
    ui->cbHighSolarRadiation->setChecked(columns.standard.testFlag(SC_HighSolarRadiation));
    ui->cbHighUVIndex->setChecked(columns.standard.testFlag(SC_HighUVIndex));

    ui->cbSoilMoisture1->setChecked(columns.extra.testFlag(EC_SoilMoisture1));
    ui->cbSoilMoisture2->setChecked(columns.extra.testFlag(EC_SoilMoisture2));
    ui->cbSoilMoisture3->setChecked(columns.extra.testFlag(EC_SoilMoisture3));
    ui->cbSoilMoisture4->setChecked(columns.extra.testFlag(EC_SoilMoisture4));
    ui->cbSoilTemperature1->setChecked(columns.extra.testFlag(EC_SoilTemperature1));
    ui->cbSoilTemperature2->setChecked(columns.extra.testFlag(EC_SoilTemperature2));
    ui->cbSoilTemperature3->setChecked(columns.extra.testFlag(EC_SoilTemperature3));
    ui->cbSoilTemperature4->setChecked(columns.extra.testFlag(EC_SoilTemperature4));

    ui->cbLeafWetness1->setChecked(columns.extra.testFlag(EC_LeafWetness1));
    ui->cbLeafWetness2->setChecked(columns.extra.testFlag(EC_LeafWetness2));
    ui->cbLeafTemperature1->setChecked(columns.extra.testFlag(EC_LeafTemperature1));
    ui->cbLeafTemperature2->setChecked(columns.extra.testFlag(EC_LeafTemperature2));

    ui->cbExtraHumidity1->setChecked(columns.extra.testFlag(EC_ExtraHumidity1));
    ui->cbExtraHumidity2->setChecked(columns.extra.testFlag(EC_ExtraHumidity2));

    ui->cbExtraTemperature1->setChecked(columns.extra.testFlag(EC_ExtraTemperature1));
    ui->cbExtraTemperature2->setChecked(columns.extra.testFlag(EC_ExtraTemperature2));
    ui->cbExtraTemperature3->setChecked(columns.extra.testFlag(EC_ExtraTemperature3));

    // Now give focus to the first tab with available columns!
    bool found = false;
    for(int i = 0; i < ui->tabWidget->count() && !found; i++) {
        QWidget* w = ui->tabWidget->widget(i);

        foreach (QCheckBox* cb, w->findChildren<QCheckBox*>()) {
            if (cb->isEnabled()) {
                ui->tabWidget->setCurrentIndex(i);
                found = true;
                break;
            }
        }
        if (!found) {
            ui->tabWidget->setTabEnabled(i, false);
        }
    }
}

SampleColumns ColumnPickerWidget::getNewColumns() const {
    SampleColumns columns = getColumns();

    // Clear the columns we started out with so we're only returning the
    // freshly checked options
    columns.standard &= ~lockedColumns.standard;
    columns.extra &= ~lockedColumns.extra;

    return columns;
}

void ColumnPickerWidget::checkAll() {
    foreach (QCheckBox* cb, this->findChildren<QCheckBox*>()) {
        if (cb->isEnabled()) {
            cb->setChecked(true);
        }
    }
}

void ColumnPickerWidget::uncheckAll() {
    foreach (QCheckBox* cb, this->findChildren<QCheckBox*>()) {
        if (cb->isEnabled()) {
            cb->setChecked(false);
        }
    }
}