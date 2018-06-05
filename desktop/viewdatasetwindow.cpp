#include <QMessageBox>
#include <QClipboard>
#include <QDebug>
#include <QFontMetrics>
#include <QVariant>

#include "viewdatasetwindow.h"
#include "ui_viewdatasetwindow.h"
#include "datasetmodel.h"
#include "settings.h"
#include "sortproxymodel.h"

#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/dialogprogresslistener.h"

ViewDataSetWindow::ViewDataSetWindow(DataSet dataSet, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewDataSetWindow)
{
    ui->setupUi(this);

#if QT_VERSION >= 0x050000
    ui->tableView->horizontalHeader()->setSectionsMovable(true);
#else
    ui->tableView->horizontalHeader()->setMovable(true);
#endif

    this->dataSet = dataSet;

    copy = new QShortcut(QKeySequence::Copy, this);
    connect(copy, SIGNAL(activated()), this, SLOT(copySelection()));

    // Setup data source
    Settings& settings = Settings::getInstance();

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        dataSource.reset(new WebDataSource(new DialogProgressListener(this), this));

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

void ViewDataSetWindow::copySelection() {
    // Copies selected data in the table to tab-delimited data on the clipboard
    QAbstractItemModel *tableModel = ui->tableView->model();
    QItemSelectionModel * selectionModel = ui->tableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    qSort(selectedIndexes);

    if(selectedIndexes.size() < 1) {
        return;
    }

    QString clipboardData;

    QModelIndex previous = selectedIndexes.first();

    selectedIndexes.removeFirst();

    for(int i = 0; i < selectedIndexes.size(); i++) {
        QVariant data = tableModel->data(previous);
        QString text = data.toString();

        QModelIndex index = selectedIndexes.at(i);
        clipboardData.append(text);

        if(index.row() != previous.row()) {
            clipboardData.append('\n');
        }
        else {
            clipboardData.append('\t');
        }
        previous = index;
    }

    clipboardData.append(tableModel->data(selectedIndexes.last()).toString());
    clipboardData.append('\n');

    QApplication::clipboard()->setText(clipboardData);
}

void ViewDataSetWindow::samplesReady(SampleSet samples)
{
    qDebug() << "Samples ready!";

    DataSetModel *model = new DataSetModel(dataSet, samples, this);
    SortProxyModel *sortableModel = new SortProxyModel(this);
    sortableModel->setSortRole(DSM_SORT_ROLE);
    sortableModel->setSourceModel(model);

    qDebug() << "Model created. Assigning to view...";

    ui->tableView->setModel(sortableModel);

    qDebug() << "View assigned Adjusting column & row sizes...";
    ui->tableView->resizeColumnsToContents();

    qDebug() << "Loading complete.";
}

void ViewDataSetWindow::samplesFailed(QString message)
{
    QMessageBox::critical(this, "Error", message);
}
