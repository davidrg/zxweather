#include "chartoptionsdialog.h"
#include "ui_chartoptionsdialog.h"

ChartOptionsDialog::ChartOptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartOptionsDialog)
{
    ui->setupUi(this);
}

ChartOptionsDialog::~ChartOptionsDialog()
{
    delete ui;
}
