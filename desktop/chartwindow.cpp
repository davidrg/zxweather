#include "chartwindow.h"
#include "ui_chartwindow.h"

ChartWindow::ChartWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartWindow)
{
    ui->setupUi(this);
}

ChartWindow::~ChartWindow()
{
    delete ui;
}
