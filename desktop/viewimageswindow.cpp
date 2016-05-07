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

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(this, this));
    else
        dataSource.reset(new WebDataSource(this, this));

    connect(dataSource.data(), SIGNAL(imageReady(int,QImage)),
            this, SLOT(imageReady(int,QImage)));

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
}

ViewImagesWindow::~ViewImagesWindow()
{
    delete ui;
}

void ViewImagesWindow::listItemDoubleClicked(QModelIndex index) {
    // Double click to open folders
    if (!model->isImage(index)){
        ui->lvImageList->setRootIndex(index);
    }
}

void ViewImagesWindow::loadImageForIndex(QModelIndex index) {
    if (model->isImage(index)) {
        // Fetch the image from the data source
        requestedImageId = model->imageId(index);

        Q_ASSERT_X(requestedImageId >= 0, "loadImageForIndex", "Invalid image ID");

        qDebug() << "Requesting image" << requestedImageId << "from the data source";

        dataSource->fetchImage(requestedImageId);
    } else {
        // Just show the icon.
        QVariant icon = index.data(Qt::DecorationRole);
        if (icon.isValid()) {
            ui->lImage->setPixmap(icon.value<QIcon>().pixmap(ui->lImage->size()));
        }
    }
}

void ViewImagesWindow::imageReady(int imageId, QImage image) {
    qDebug() << "Received image" << imageId << "expecting id" << requestedImageId;
    if (imageId != requestedImageId) {
        // Its an image someone else asked for. Not interested in it.
        return;
    }

    ui->lImage->setPixmap(QPixmap::fromImage(image));
    requestedImageId = -1;
}

void ViewImagesWindow::treeItemSelectionChanged(QItemSelection selected,
                                                QItemSelection deselected) {
    if (selected.indexes().isEmpty()) {
        return;
    }

    QModelIndex index = selected.indexes().first();

    if (!model.data()->isImage(index)) {
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
