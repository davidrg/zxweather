#include "datetimeformathelpdialog.h"
#include "ui_datetimeformathelpdialog.h"

DateTimeFormatHelpDialog::DateTimeFormatHelpDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DateTimeFormatHelpDialog)
{
    ui->setupUi(this);
}

DateTimeFormatHelpDialog::~DateTimeFormatHelpDialog()
{
    delete ui;
}
