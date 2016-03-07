#include "datasettimespandialog.h"
#include "ui_datasettimespandialog.h"

DataSetTimespanDialog::DataSetTimespanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataSetTimespanDialog)
{
    ui->setupUi(this);
}

DataSetTimespanDialog::~DataSetTimespanDialog()
{
    delete ui;
}

QDateTime DataSetTimespanDialog::getStartTime() {
    return ui->timespan->getStartTime();
}

QDateTime DataSetTimespanDialog::getEndTime() {
    return ui->timespan->getEndTime();
}

void DataSetTimespanDialog::setTime(QDateTime start, QDateTime end) {
    ui->timespan->setTimeSpan(start, end);
}
