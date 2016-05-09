#include "viewimageswindow.h"
#include "ui_viewimageswindow.h"

#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"

#include <QtDebug>

ViewImagesWindow::ViewImagesWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewImagesWindow)
{
    ui->setupUi(this);

    connect(ui->lvImageList, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(listItemDoubleClicked(QModelIndex)));

    // Setup data source
    Settings& settings = Settings::getInstance();
    restoreState(settings.getImagesWindowLayout());

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    model.reset(new ImageModel(dataSource.data(), this));

    ui->lvImageList->setIconSize(QSize(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT));

    ui->tvImageSet->setModel(model.data());
    ui->lvImageList->setModel(model.data());

    connect(ui->lvImageList->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(listItemSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tvImageSet->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(treeItemSelectionChanged(QItemSelection,QItemSelection)));

    connect(ui->splitter_2, SIGNAL(splitterMoved(int,int)),
            this, SLOT(hSplitterMoved(int,int)));
    connect(ui->splitter, SIGNAL(splitterMoved(int,int)),
            this, SLOT(vSplitterMoved(int,int)));

    ui->splitter->restoreState(settings.getImagesWindowVSplitterLayout());
    ui->splitter_2->restoreState(settings.getImagesWindowHSplitterLayout());
}

ViewImagesWindow::~ViewImagesWindow()
{
    delete ui;
}

void ViewImagesWindow::listItemDoubleClicked(QModelIndex index) {
    // Double click to open folders

    if (!model->isImage(index)){
        ui->tvImageSet->expand(index);
        ui->lvImageList->setRootIndex(index);
    }
}

void ViewImagesWindow::loadImageForIndex(QModelIndex index) {
    if (model->isImage(index)) {

        ui->lImage->setImage(model->image(index),
                             model->imageTemporaryFileName(index));
    } else {
        // Just show the icon.
        QVariant icon = index.data(Qt::DecorationRole);
        if (icon.isValid()) {
            ui->lImage->setPixmap(icon.value<QIcon>().pixmap(ui->lImage->size()));
        }
    }
}

void ViewImagesWindow::treeItemSelectionChanged(QItemSelection selected,
                                                QItemSelection deselected) {
    if (selected.indexes().isEmpty()) {
        return;
    }

    QModelIndex index = selected.indexes().first();

    if (model.data()->isImage(index)) {
        ui->lvImageList->setRootIndex(selected.indexes().first().parent());
    } else {
        ui->lvImageList->setRootIndex(selected.indexes().first());
    }

    listItemSelectionChanged(selected, deselected);
}


void ViewImagesWindow::listItemSelectionChanged(QItemSelection selected,
                                                QItemSelection deselected) {
    if (selected.indexes().isEmpty()) {
        return;
    }

    loadImageForIndex(selected.indexes().first());
}

void ViewImagesWindow::hSplitterMoved(int, int) {
    Settings::getInstance().setImagesWindowHSplitterLayout(ui->splitter_2->saveState());
}

void ViewImagesWindow::vSplitterMoved(int, int) {
    Settings::getInstance().setImagesWindowVSplitterLayout(ui->splitter->saveState());
}

void ViewImagesWindow::closeEvent(QCloseEvent *event)
{
    Settings::getInstance().setImagesWindowLayout(saveState());
    QMainWindow::closeEvent(event);
}
