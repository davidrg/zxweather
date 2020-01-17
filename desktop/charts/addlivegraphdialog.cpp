#include "addlivegraphdialog.h"
#include "ui_addlivegraphdialog.h"

AddLiveGraphDialog::AddLiveGraphDialog(LiveValues availableColumns,
                                       bool solarAvailable,
                                       hardware_type_t hw_type,
                                       ExtraColumns extraColumns,
                                       QMap<ExtraColumn, QString> extraColumnNames,
                                       QString message,
                                       QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddLiveGraphDialog)
{
    ui->setupUi(this);

    if (!message.isNull()) {
        ui->lblMessage->setText(message);
    }

    ui->columnPicker->configure(solarAvailable, hw_type, extraColumns, extraColumnNames);

    ui->columnPicker->checkAndLockColumns(~availableColumns);
}

AddLiveGraphDialog::~AddLiveGraphDialog()
{
    delete ui;
}

LiveValues AddLiveGraphDialog::selectedColumns() {
    return ui->columnPicker->getNewColumns();
}
