#include "columnpickerwidget.h"
#include "ui_columnpickerwidget.h"

#include <QtDebug>

ColumnPickerWidget::ColumnPickerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ColumnPickerWidget)
{
    ui->setupUi(this);

    // This one is off by default as not very useful for display
    ui->cbForecastRule->setVisible(false);
    ui->cbForecastRule->setEnabled(false);


    // Off by default as its exclusive to live data
    ui->cbConsoleBatteryVoltage->setEnabled(false);
    ui->cbConsoleBatteryVoltage->setVisible(false);
}

ColumnPickerWidget::~ColumnPickerWidget()
{
    delete ui;
}


void ColumnPickerWidget::configureUi(
        bool solarAvailable, hardware_type_t hw_type, bool isWireless,
        ExtraColumns extraColumns,
        QMap<ExtraColumn, QString> extraColumnNames) {

    if (hw_type != HW_DAVIS) {
        hideDavisOnlyColumns();
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabLeafAndSoil));
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabExtra));
    } else {
        // TODO: Get all this from the data sources column config
        if (!solarAvailable) {
            hideSolarColumns();
        }

        if (!isWireless) {
            hideWirelessReceptionColumn();
        }

        // Extra column config
        configureExtraColumns(extraColumns, extraColumnNames);
    }


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

    emit columnSelectionChanged();

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

void ColumnPickerWidget::hideSolarColumns() {
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

void ColumnPickerWidget::hideWirelessReceptionColumn() {
    ui->cbWirelessReception->setVisible(false);
    ui->cbWirelessReception->setEnabled(false);
}

void ColumnPickerWidget::hideDavisOnlyColumns() {
    ui->gbTemperatureHighLow->setVisible(false);
    ui->cbHighTemperature->setVisible(false);
    ui->cbLowTemperature->setVisible(false);
    ui->cbWirelessReception->setVisible(false);
    ui->cbRainRate->setVisible(false);
    ui->cbGustDirection->setVisible(false);
    ui->cbConsoleBatteryVoltage->setVisible(false);
    ui->cbForecastRule->setVisible(false);

    ui->gbTemperatureHighLow->setEnabled(false);
    ui->cbHighTemperature->setEnabled(false);
    ui->cbLowTemperature->setEnabled(false);
    ui->cbWirelessReception->setEnabled(false);
    ui->cbRainRate->setEnabled(false);
    ui->cbGustDirection->setEnabled(false);
    ui->cbConsoleBatteryVoltage->setEnabled(false);
    ui->cbForecastRule->setEnabled(false);

    // These are also davis-exclusive in zxweather for now.
    hideWirelessReceptionColumn();
    hideSolarColumns();
}

void ColumnPickerWidget::configureExtraColumns(ExtraColumns extraColumns, QMap<ExtraColumn, QString> extraColumnNames) {
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
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabLeafAndSoil),
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

    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabExtra),
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
}

void ColumnPickerWidget::focusFirstAvailableTab() {
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
