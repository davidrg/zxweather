#include <QMessageBox>
#include <QSortFilterProxyModel>

#include "viewdatasetwindow.h"
#include "ui_viewdatasetwindow.h"
#include "datasetmodel.h"
#include "settings.h"

#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"

ViewDataSetWindow::ViewDataSetWindow(DataSet dataSet, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewDataSetWindow)
{
    ui->setupUi(this);

    this->dataSet = dataSet;

    // Setup data source
    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    connect(dataSource.data(), SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(dataSource.data(), SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(samplesFailed(QString)));
}

ViewDataSetWindow::~ViewDataSetWindow()
{
    delete ui;
}

void ViewDataSetWindow::show() {
    QMainWindow::show();

    dataSource->fetchSamples(dataSet);
}

void ViewDataSetWindow::samplesReady(SampleSet samples)
{

    DataSetModel *model = new DataSetModel(dataSet, samples, this);
    QSortFilterProxyModel *sortableModel = new QSortFilterProxyModel(ui->tableView);
    sortableModel->setSourceModel(model);

    ui->tableView->setModel(sortableModel);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
}

void ViewDataSetWindow::samplesFailed(QString message)
{
    QMessageBox::critical(this, "Error", message);
}
