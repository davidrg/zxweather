#include "viewdataoptionsdialog.h"
#include "ui_viewdataoptionsdialog.h"

ViewDataOptionsDialog::ViewDataOptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ViewDataOptionsDialog)
{
    ui->setupUi(this);
}

ViewDataOptionsDialog::~ViewDataOptionsDialog()
{
    delete ui;
}


QDateTime ViewDataOptionsDialog::getStartTime() {
    return ui->timespan->getStartTime();
}

QDateTime ViewDataOptionsDialog::getEndTime() {
    return ui->timespan->getEndTime();
}

AggregateFunction ViewDataOptionsDialog::getAggregateFunction() {
    if (!ui->gbAggregate->isChecked())
        return AF_None;

    return ui->aggregateWidget->getAggregateFunction();
}

AggregateGroupType ViewDataOptionsDialog::getAggregateGroupType() {
    if (!ui->gbAggregate->isChecked())
        return AGT_None;

    return ui->aggregateWidget->getAggregateGroupType();
}

uint32_t ViewDataOptionsDialog::getCustomMinutes() {
    if (!ui->gbAggregate->isChecked())
        return 0;
    return ui->aggregateWidget->getCustomMinutes();
}
