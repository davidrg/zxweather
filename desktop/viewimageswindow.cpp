#include "viewimageswindow.h"
#include "ui_viewimageswindow.h"

#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "weatherimagewindow.h"

#include <QtDebug>
#include <QMenu>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>

ViewImagesWindow::ViewImagesWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewImagesWindow)
{
    ui->setupUi(this);
    ui->lImage->setScaled(false);

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

    // Update layout of image list as items come in
    connect(model.data(), SIGNAL(layoutChanged()),
            ui->lvImageList, SLOT(doItemsLayout()));

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

    ui->lvImageList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tvImageSet->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->lvImageList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(listItemContextMenu(QPoint)));
    connect(ui->tvImageSet, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(treeItemContextMenu(QPoint)));

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
    } else {
        loadImageForIndex(index);
        ui->lImage->popOut();
    }
}

void ViewImagesWindow::loadImageForIndex(QModelIndex index) {
    ui->lImage->setScaled(true);
    if (model->isImage(index)) {

        QImage image = model->image(index);
        QString filename = model->imageTemporaryFileName(index);

        if (!image.isNull()) {
            ui->lImage->setImage(image, filename);
            return;
        }

        ImageInfo info = model->imageInfo(index);
        qDebug() << info.mimeType;
        if (info.mimeType.startsWith("video/")) {
            // Video file. Send it to the Image Widget - it should be able to
            // handle it provided the right codec is available
            ui->lImage->setImage(image, info, filename);
            return;
        }
    }


    // No image? Not an image? Just show the icon.
    QVariant icon = index.data(Qt::DecorationRole);
    if (icon.isValid()) {
        ui->lImage->setScaled(false); // don't scale the icon
        //ui->lImage->setPixmap(icon.value<QIcon>().pixmap(ui->lImage->size()));
        ui->lImage->setIcon(icon.value<QIcon>());
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

void ViewImagesWindow::listItemContextMenu(const QPoint& point) {
    qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! List ctx";
    QPoint pt = ui->lvImageList->mapToGlobal(point);
    contextMenu(pt, ui->lvImageList->indexAt(point), true);
}

void ViewImagesWindow::treeItemContextMenu(const QPoint& point) {
    QPoint pt = ui->tvImageSet->mapToGlobal(point);
    contextMenu(pt, ui->tvImageSet->indexAt(point), false);
}

void ViewImagesWindow::contextMenu(QPoint point, QModelIndex idx, bool isList) {
    if(model.data()->isImage(idx)) {
        // Image node
        QMenu* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *act = menu->addAction("&Open in new window",
                                           this, SLOT(openImageInWindow()));
        QFont f = act->font();
        f.setBold(true);
        act->setFont(f);
        act->setData(isList);

        act = menu->addAction("&View weather at time",
                              this, SLOT(viewWeather()));
        act->setData(isList);

        menu->addSeparator();

        act = menu->addAction("&Save As...",
                        this, SLOT(saveImageAs()));
        act->setData(isList);

        menu->popup(point);
    } else {
        // Its a folder node
        qDebug() << "TODO: folder context menu";
    }
}

void ViewImagesWindow::openImageInWindow() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;
        if (isList) {
            index = ui->lvImageList->currentIndex();
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (model->isImage(index)) {

            QImage image = model->image(index);
            QString filename = model->imageTemporaryFileName(index);
            ImageInfo info = model->imageInfo(index);

            ImageWidget::popOut(info, image, filename);
        }
    }
}

void ViewImagesWindow::saveImageAs() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;
        if (isList) {
            index = ui->lvImageList->currentIndex();
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (index.isValid() && model->isImage(index)) {

            QImage image = model->image(index);
            QString filename = model->imageTemporaryFileName(index);
            ImageInfo info = model->imageInfo(index);
            QFileInfo fileInfo(filename);


            QString filter;
            if (info.mimeType.startsWith("video/")) {
                filter = "Video files (*." + fileInfo.completeSuffix() + ")";
            } else {
                filter = "Image files (*." + fileInfo.completeSuffix() + ")";
            }

            QString fn = QFileDialog::getSaveFileName(this, tr("Save As..."),
                QString(), filter);

            if (info.mimeType.startsWith("image/")) {
                image.save(fn);
            } else {
                QFile::copy(filename, fn);
            }
        }
    }
}

void ViewImagesWindow::viewWeather() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;
        if (isList) {
            index = ui->lvImageList->currentIndex();
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (index.isValid() && model->isImage(index)) {
            ImageInfo info = model->imageInfo(index);

            WeatherImageWindow *wnd = new WeatherImageWindow();
            wnd->setAttribute(Qt::WA_DeleteOnClose);
            wnd->show();
            wnd->setImage(info.id);
        }
    }
}
