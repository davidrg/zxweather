#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

#include <QMessageBox>
#include <QtDebug>
#include <QCheckBox>

ChartOptionsDialog::ChartOptionsDialog(bool solarAvailable,
                                       hardware_type_t hw_type, bool isWireless,
                                       ExtraColumns extraColumns,
                                       QMap<ExtraColumn, QString> extraColumnNames,
                                       QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);

    ui->columnPicker->configure(solarAvailable, hw_type, isWireless, extraColumns,
                                extraColumnNames);

    connect(ui->columnPicker, SIGNAL(columnSelectionChanged()),
            this, SLOT(columnSelectionChanged()));

    // Buttons
    connect(this->ui->buttonBox, SIGNAL(accepted()), this, SLOT(checkAndAccept()));

    // Make sure the rain/evapo aggregate options match the default column selection.
    columnSelectionChanged();
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}


void ChartOptionsDialog::checkAndAccept() {
    SampleColumns columns = ui->columnPicker->getColumns();

    if (columns.standard == SC_NoColumns && columns.extra == EC_NoColumns) {
        QMessageBox::information(this, tr("Data Sets"),
                                 tr("At least one data set must be selected"));
        return;
    }

    accept();
}

QDateTime ChartOptionsDialog::getStartTime() const {
    return ui->timespan->getStartTime();
}

QDateTime ChartOptionsDialog::getEndTime() const {
    return ui->timespan->getEndTime();
}

AggregateFunction ChartOptionsDialog::getAggregateFunction() const {
    if (!ui->gbAggregate->isChecked())
        return AF_None;

    return ui->aggregateWidget->getAggregateFunction();
}

AggregateGroupType ChartOptionsDialog::getAggregateGroupType() const {
    if (!ui->gbAggregate->isChecked())
        return AGT_None;

    return ui->aggregateWidget->getAggregateGroupType();
}

uint32_t ChartOptionsDialog::getCustomMinutes() const {
    if (!ui->gbAggregate->isChecked())
        return 0;
    return ui->aggregateWidget->getCustomMinutes();
}

SampleColumns ChartOptionsDialog::getColumns() const {
    return ui->columnPicker->getColumns();
}

void ChartOptionsDialog::columnSelectionChanged() {
    SampleColumns columns = getColumns();

    if (columns.standard.testFlag(SC_Evapotranspiration)
            || columns.standard.testFlag(SC_Rainfall)) {
        ui->aggregateWidget->setRainEvapoOptionsEnabled(true);
    } else {
        ui->aggregateWidget->setRainEvapoOptionsEnabled(false);
    }
}
