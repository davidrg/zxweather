#include "viewdataoptionsdialog.h"
#include "ui_viewdataoptionsdialog.h"

ViewDataOptionsDialog::ViewDataOptionsDialog(bool solarAvailable, hardware_type_t hw_type, bool isWireless,
                                             ExtraColumns extraColumns, QMap<ExtraColumn, QString> extraColumnNames,
                                             QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ViewDataOptionsDialog)
{
    ui->setupUi(this);
    connect(ui->pbSelectAll, SIGNAL(clicked(bool)), ui->columnPicker, SLOT(checkAll()));
    connect(ui->pbSelectNone, SIGNAL(clicked(bool)), ui->columnPicker, SLOT(uncheckAll()));
    connect(ui->columnPicker, SIGNAL(columnSelectionChanged()), this, SLOT(columnSelectionChanged()));

    ui->columnPicker->configure(solarAvailable, hw_type, isWireless, extraColumns, extraColumnNames, true);

    ui->columnPicker->checkAll();
}

ViewDataOptionsDialog::~ViewDataOptionsDialog()
{
    delete ui;
}


QDateTime ViewDataOptionsDialog::getStartTime() const {
    return ui->timespan->getStartTime();
}

QDateTime ViewDataOptionsDialog::getEndTime() const {
    return ui->timespan->getEndTime();
}

AggregateFunction ViewDataOptionsDialog::getAggregateFunction() const {
    if (!ui->gbAggregate->isChecked())
        return AF_None;

    return ui->aggregateWidget->getAggregateFunction();
}

AggregateGroupType ViewDataOptionsDialog::getAggregateGroupType() const {
    if (!ui->gbAggregate->isChecked())
        return AGT_None;

    return ui->aggregateWidget->getAggregateGroupType();
}

uint32_t ViewDataOptionsDialog::getCustomMinutes() const {
    if (!ui->gbAggregate->isChecked())
        return 0;
    return ui->aggregateWidget->getCustomMinutes();
}

SampleColumns ViewDataOptionsDialog::getColumns() const {
    SampleColumns result = ui->columnPicker->getColumns();
    result.standard |= SC_Timestamp;
    return result;
}

void ViewDataOptionsDialog::columnSelectionChanged() {
    SampleColumns columns = getColumns();

    if (columns.standard.testFlag(SC_Evapotranspiration)
            || columns.standard.testFlag(SC_Rainfall)) {
        ui->aggregateWidget->setRainEvapoOptionsEnabled(true);
    } else {
        ui->aggregateWidget->setRainEvapoOptionsEnabled(false);
    }
}
