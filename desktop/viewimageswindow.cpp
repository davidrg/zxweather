#include "viewimageswindow.h"
#include "ui_viewimageswindow.h"

#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/dialogprogresslistener.h"

#include <QtDebug>
#include <QMenu>
#include <QMessageBox>

ViewImagesWindow::ViewImagesWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewImagesWindow)
{
    ui->setupUi(this);
    ui->lImage->setScaled(false);
    ui->lvImageList->setDragEnabled(true);

    connect(ui->lvImageList, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(listItemDoubleClicked(QModelIndex)));

    // Setup data source
    Settings& settings = Settings::getInstance();
    restoreState(settings.getImagesWindowLayout());

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        dataSource.reset(new WebDataSource(new DialogProgressListener(this), this));

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
    connect(ui->tvImageSet, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(treeItemDoubleClicked(QModelIndex)));

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

void ViewImagesWindow::treeItemDoubleClicked(QModelIndex index) {
    if (!model->isImage(index)){
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
        ImageInfo info = model->imageInfo(index);

        if (!image.isNull()) {
            ui->lImage->setImage(image, info, filename);
            return;
        }


        qDebug() << info.mimeType;
        if (info.mimeType.startsWith("video/") || info.mimeType.startsWith("audio/")) {
            // Video file. Send it to the Image Widget - it should be able to
            // handle it provided the right codec is available
            if (filename == "") {
                // Not available yet (still downloading?)
                if (info.mimeType.startsWith("video/"))
                    ui->lImage->setIcon(QIcon(":/icons/film-32"));
                else
                    ui->lImage->setIcon(QIcon(":/icons/audio-32"));
            } else {
                ui->lImage->setImage(image, info, filename);
            }
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
    QPoint pt = ui->lvImageList->mapToGlobal(point);
    contextMenu(pt, ui->lvImageList->indexAt(point), true);
}

void ViewImagesWindow::treeItemContextMenu(const QPoint& point) {
    QPoint pt = ui->tvImageSet->mapToGlobal(point);
    contextMenu(pt, ui->tvImageSet->indexAt(point), false);
}

void ViewImagesWindow::contextMenu(QPoint point, QModelIndex idx, bool isList) {
    if (!idx.isValid()) {
        return;
    }

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

        menu->addSeparator();

        act = menu->addAction("&Properties",
                        this, SLOT(properties()));
        act->setData(isList);

        menu->popup(point);
    } else {
        // Its a folder node
        // options should be:
        // <b>Open</b>
        // Download all (only if using web data source)
        // ---
        // Save As...
        // ---
        // Properties

        QMenu* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *act = menu->addAction("&Open",
                                           this, SLOT(openItem()));
        QFont f = act->font();
        f.setBold(true);
        act->setFont(f);
        act->setData(isList);

#ifdef QT_DEBUG
        // In Qt 4.8 at least this crashes sometime (looks like Qt bug). Its
        // also very slow via the web data source.
        if (!isList){
            // Tree only!
            menu->addSeparator();

            act = menu->addAction("&Expand all",
                                  this, SLOT(expandRecursively()));
            act->setData(isList);
            act = menu->addAction("&Collapse all",
                                  this, SLOT(collapseRecursively()));
            act->setData(isList);
        }
#endif

        //menu->addSeparator();

        menu->popup(point);
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

            ImageWidget::saveAs(this, info, image, filename);
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

            ImageWidget::weatherDataAtTime(info.id);
        }
    }
}

void ViewImagesWindow::properties() {
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

            ImageWidget::showProperties(info, image, filename);
        }
    }
}

void ViewImagesWindow::openItem() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;
        if (isList) {
            index = ui->lvImageList->currentIndex();
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (index.isValid() && !model->isImage(index)) {
            ui->tvImageSet->expand(index);
            ui->lvImageList->setRootIndex(index);
        }
    }
}

void ViewImagesWindow::expandRecursively() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;

        // Tree only!
        if (isList) {
            return;
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (!index.isValid()) {
            return;
        }

        if (Settings::getInstance().sampleDataSourceType() != Settings::DS_TYPE_DATABASE) {
            int ret = QMessageBox::question(
                        this, tr("Expand"),
                        tr("This will cause all images in this folder to be "
                           "downloaded which may take a while. Do you want "
                           "to continue and expand all folders?"),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
            if (ret == QMessageBox::No) {
                return;
            }
        }

        QModelIndexList children;
        children << index;
        for ( int i = 0; i < children.size(); ++i ) {
            for ( int j = 0; j < model.data()->rowCount( children[i] ); ++j ) {
                children << children[i].child( j, 0 );
            }
        }
        qDebug() << "Found" << children.size() << "children";

        // The model normally lazy-loads. Here we need to be less lazy.
        foreach (QModelIndex idx, children) {
            model.data()->loadItem(idx);
        }

        // Now that load requests are queued for everything, expand it all.
        foreach (QModelIndex idx, children) {
            ui->tvImageSet->expand(idx);
        }
    }
}

void ViewImagesWindow::collapseRecursively() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        bool isList = menuAction->data().toBool();
        QModelIndex index;

        // Tree only!
        if (isList) {
            return;
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (!index.isValid()) {
            return;
        }

        QModelIndexList children;
        children << index;
        for ( int i = 0; i < children.size(); ++i ) {
            for ( int j = 0; j < model.data()->rowCount( children[i] ); ++j ) {
                children << children[i].child( j, 0 );
            }
        }
        qDebug() << "Found" << children.size() << "children";

        foreach (QModelIndex idx, children) {
            ui->tvImageSet->collapse(idx);
        }
    }
}
