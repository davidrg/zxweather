#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QClipboard>

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

    copy = new QShortcut(QKeySequence::Copy, this);
    connect(copy, SIGNAL(activated()), this, SLOT(copySelection()));

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

void ViewDataSetWindow::copySelection() {
    // Copies selected datain the table to tab-delimited data on the clipboard
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

    DataSetModel *model = new DataSetModel(dataSet, samples, this);
    QSortFilterProxyModel *sortableModel = new QSortFilterProxyModel(this);
    sortableModel->setSourceModel(model);

    /* Turns out there is a bug in QTableView that causes it to do a bunch of
     * resizing when the widget is hidden. This results in the entire
     * application locking up for anywhere between a few seconds and a few
     * minutes when the window is closed (depending on how much data is in it).
     *
     * This bug was fixed in Qt 5.0.0. As such, resize-to-contents is disabled
     * when buliding against older Qt versions.
     *
     * Details: https://bugreports.qt.io/browse/QTBUG-14234
     */
#if QT_VERSION >= 0x050000
    for (int c = 0; c < ui->tableView->horizontalHeader()->count(); ++c)
    {
        ui->tableView->horizontalHeader()->setSectionResizeMode(
            c, QHeaderView::ResizeToContents);
    }
#endif

    ui->tableView->setModel(sortableModel);

}

void ViewDataSetWindow::samplesFailed(QString message)
{
    QMessageBox::critical(this, "Error", message);
}
