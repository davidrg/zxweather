#include "viewimageswindow.h"
#include "ui_viewimageswindow.h"

#include "settings.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/dialogprogresslistener.h"
#include "constants.h"

#include <QtDebug>
#include <QMenu>
#include <QMessageBox>

// Stack widget indicies
#define SW_ICONS 0
#define SW_DETAIL 1

/* TODO:
 *  -> Sorting in the Detail View?
 *      This could be challenging as using a proxy model won't expose the
 *      necessary functions (unless it provides an API to map its indicies
 *      onto the base model?)
 *  -> Context menu option to open a folder in a new window?
 *          This really needs a more persistent cache of images. Probably the DS needs to return
 *          twice - once from cache and a second time if and when it has more up-to-date information
 *          from the server.
 *  -> Properties window for a folder
 *          Needs more detailed cache information before we can show anything useful in this
 *  -> Refresh/reload button? This is probably most useful if a model
 *      is being shared by multiple windows.
 *  -> Test: Live image loading on new image source
 */

ViewImagesWindow::ViewImagesWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ViewImagesWindow)
{
    ui->setupUi(this);
    ui->lImage->setScaled(false);

    // This doesn't seem to fix the overpaint
    //ui->scrollArea->setFrameStyle(QFrame::NoFrame);
    //ui->scrollArea->setFrameShadow(QFrame::Plain);

    ui->lvImageList->setDragEnabled(true);
    ui->tvDetail->setDragEnabled(true);

    location = new QLabel(this);
    ui->statusBar->addWidget(location);
    location->setText("");

    itemCount = new QLabel(this);
    ui->statusBar->addWidget(itemCount);
    itemCount->setText("0 items");

    Settings& settings = Settings::getInstance();

    // Toolbar
    ui->actionDetail->setData((int)VIWM_Detail);
    ui->actionIcons->setData((int)VIWM_Icons);
    ui->actionList->setData((int)VIWM_SmallIcons);
    ui->actionSmall_Thumbnails->setData((int)VIWM_SmallThumbnails);
    ui->actionThumbnails->setData((int)VIWM_Thumbnails);

    connect(ui->actionNavigate_Up, SIGNAL(triggered(bool)), this, SLOT(navigateUp()));

    connect(ui->actionTree, SIGNAL(triggered(bool)), this, SLOT(showHideTreePane(bool)));
    connect(ui->actionPreview, SIGNAL(triggered(bool)), this, SLOT(showHidePreviewPane(bool)));

    connect(ui->actionThumbnails, SIGNAL(triggered(bool)), this, SLOT(setViewModeMenuHandler()));
    connect(ui->actionSmall_Thumbnails, SIGNAL(triggered()), this, SLOT(setViewModeMenuHandler()));
    connect(ui->actionIcons, SIGNAL(triggered(bool)), this, SLOT(setViewModeMenuHandler()));
    connect(ui->actionList, SIGNAL(triggered(bool)), this, SLOT(setViewModeMenuHandler()));
    connect(ui->actionDetail, SIGNAL(triggered(bool)), this, SLOT(setViewModeMenuHandler()));

    connect(ui->actionPop_Out, SIGNAL(triggered(bool)), this, SLOT(openImageInWindow()));
    connect(ui->actionShow_Weather, SIGNAL(triggered(bool)), this, SLOT(viewWeather()));
    connect(ui->actionSave_As, SIGNAL(triggered(bool)), this, SLOT(saveImageAs()));
    connect(ui->actionProperties, SIGNAL(triggered(bool)), this, SLOT(properties()));

    ui->actionPop_Out->setData(CMS_Toolbar);
    ui->actionShow_Weather->setData(CMS_Toolbar);
    ui->actionSave_As->setData(CMS_Toolbar);
    ui->actionProperties->setData(CMS_Toolbar);

    // Item double-click
    connect(ui->lvImageList, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(listItemDoubleClicked(QModelIndex)));
    connect(ui->tvDetail, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(detailItemDoubleClicked(QModelIndex)));

    // Setup data source

    if (settings.sampleDataSourceType() == Settings::DS_TYPE_DATABASE)
        dataSource.reset(new DatabaseDataSource(new DialogProgressListener(this), this));
    else
        dataSource.reset(new WebDataSource(new DialogProgressListener(this), this));

    model.reset(new ImageModel(dataSource.data(), this));

    ui->tvDetail->setIconSize(QSize(16,16));

    ui->tvImageSet->setModel(model.data());
    ui->tvDetail->setModel(model.data());
    ui->lvImageList->setModel(model.data());

    ui->tvImageSet->hideColumn(ImageModel::COL_TIME);
    ui->tvImageSet->hideColumn(ImageModel::COL_TYPE);
    ui->tvImageSet->hideColumn(ImageModel::COL_SIZE);
    ui->tvImageSet->hideColumn(ImageModel::COL_DESCRIPTION);
    ui->tvImageSet->hideColumn(ImageModel::COL_MIME_TYPE);
    ui->tvImageSet->hideColumn(ImageModel::COL_IMAGE_SOURCE);
    ui->tvImageSet->hideColumn(ImageModel::COL_NAME_THUMB);

    ui->tvDetail->hideColumn(ImageModel::COL_NAME_THUMB);

    setViewMode((ViewImagesWindowViewMode)settings.imagesWindowViewMode());
    showHidePreviewPane(settings.imagesWindowPreviewPaneVisible());
    showHideTreePane(settings.imagesWindowNavigationPaneVisible());

    connect(model.data(), SIGNAL(modelReset()), this, SLOT(expandNow()));

    // Update layout of image list as items come in
    connect(model.data(), SIGNAL(layoutChanged()),
            ui->lvImageList, SLOT(doItemsLayout()));

    // Setup selection changed events
    connect(ui->lvImageList->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(listItemSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tvDetail->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(detailItemSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tvImageSet->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(treeItemSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tvImageSet, SIGNAL(clicked(QModelIndex)),
            this, SLOT(setViewIndex(QModelIndex)));

    // Setup context menus
    ui->lvImageList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tvDetail->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tvImageSet->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->lvImageList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(listItemContextMenu(QPoint)));
    connect(ui->tvDetail, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(detailItemContextMenu(QPoint)));
    connect(ui->tvImageSet, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(treeItemContextMenu(QPoint)));

    connect(ui->tvImageSet, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(treeItemDoubleClicked(QModelIndex)));

    // Configure splitters
    connect(ui->splitter_2, SIGNAL(splitterMoved(int,int)),
            this, SLOT(hSplitterMoved(int,int)));
    connect(ui->splitter, SIGNAL(splitterMoved(int,int)),
            this, SLOT(vSplitterMoved(int,int)));

    connect(model.data(), SIGNAL(lazyLoadingComplete(QModelIndex)),
            this, SLOT(lazyLoadingComplete(QModelIndex)));
    connect(model.data(), SIGNAL(imageReady(QModelIndex)),
            this, SLOT(imageReady(QModelIndex)));

    ui->splitter->restoreState(settings.getImagesWindowVSplitterLayout());
    ui->splitter_2->restoreState(settings.getImagesWindowHSplitterLayout());

    updateToolbarStatus(QModelIndex());

    restoreState(settings.getImagesWindowLayout());
    restoreGeometry(settings.imagesWindowGeometry());
}

ViewImagesWindow::~ViewImagesWindow()
{
    delete ui;
}

void ViewImagesWindow::listItemDoubleClicked(QModelIndex index) {
    // Double click to open folders
qDebug() << "Q";
    // Navigation is always on COL_NAME but the list is sometimes
    // displaying COL_NAME_THUMB.
    if (index.isValid()) {
        index = normaliseIndexColumn(index);
        updateToolbarStatus(index);

        if (!model->isImage(index)){
            ui->tvImageSet->expand(index);
            setViewIndex(index);
        } else {
            loadImageForIndex(index);
            ui->lImage->popOut();
        }
    } else {
        qDebug() << "X";
        setViewIndex(index);
    }
}

void ViewImagesWindow::detailItemDoubleClicked(QModelIndex index) {
    // Double click to open folders

    updateToolbarStatus(index);

    if (!model->isImage(index)){
        ui->tvImageSet->expand(index);
        setViewIndex(index);
    } else {
        loadImageForIndex(index);
        ui->lImage->popOut();
    }
}

void ViewImagesWindow::treeItemDoubleClicked(QModelIndex index) {
    updateToolbarStatus(index);

    if (!model->isImage(index)){
        setViewIndex(index);
    } else {
        loadImageForIndex(index);
        ui->lImage->popOut();
    }
}

void ViewImagesWindow::updateToolbarStatus(QModelIndex index) {
    bool isImage = index.isValid() && model.data()->isImage(index);

    ui->actionPop_Out->setEnabled(isImage);
    ui->actionShow_Weather->setEnabled(isImage);
    ui->actionSave_As->setEnabled(isImage);
    ui->actionProperties->setEnabled(isImage);
}

void ViewImagesWindow::loadImageForIndex(QModelIndex index) {
    ui->lImage->setScaled(true);
    if (model->isImage(index)) {

        QImage image = model->image(index);
        QString filename = model->imageTemporaryFileName(index);
        ImageInfo info = model->imageInfo(index);

        currentImageIndex = normaliseIndexColumn(index);
        imageLoaded = !image.isNull() || filename != "";

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
        setViewIndex(index.parent());
    } else {
        setViewIndex(index);
    }

    listItemSelectionChanged(selected, deselected);
}


void ViewImagesWindow::listItemSelectionChanged(QItemSelection selected,
                                                QItemSelection deselected) {
    Q_UNUSED(deselected)

    if (selected.indexes().isEmpty()) {
        return;
    }

    updateToolbarStatus(selected.indexes().first());
    loadImageForIndex(selected.indexes().first());
}

void ViewImagesWindow::detailItemSelectionChanged(QItemSelection selected,
                                                  QItemSelection deselected) {
    listItemSelectionChanged(selected, deselected);
}

void ViewImagesWindow::hSplitterMoved(int, int) {
    Settings::getInstance().setImagesWindowHSplitterLayout(ui->splitter_2->saveState());
}

void ViewImagesWindow::vSplitterMoved(int, int) {
    Settings::getInstance().setImagesWindowVSplitterLayout(ui->splitter->saveState());
}

void ViewImagesWindow::newImage(NewImageInfo info) {
    model->newImage(info);
}

void ViewImagesWindow::closeEvent(QCloseEvent *event)
{
    Settings::getInstance().setImagesWindowLayout(saveState());
    Settings::getInstance().saveImagesWindowGeometry(saveGeometry());
    QMainWindow::closeEvent(event);
}

void ViewImagesWindow::listItemContextMenu(const QPoint& point) {
    QPoint pt = ui->lvImageList->mapToGlobal(point);
    contextMenu(pt, ui->lvImageList->indexAt(point), CMS_List);
}

void ViewImagesWindow::detailItemContextMenu(const QPoint& point) {
    QPoint pt = ui->tvDetail->viewport()->mapToGlobal(point);
    contextMenu(pt, ui->tvDetail->indexAt(point), CMS_Detail);
}

void ViewImagesWindow::treeItemContextMenu(const QPoint& point) {
    QPoint pt = ui->tvImageSet->viewport()->mapToGlobal(point);
    contextMenu(pt, ui->tvImageSet->indexAt(point), CMS_Tree);
}

void ViewImagesWindow::contextMenu(QPoint point, QModelIndex idx, ContextMenuSource source) {
    if (!idx.isValid()) {
        // Show the general view thing

        if (source == CMS_List || source == CMS_Detail) {
            QMenu* menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);

            QMenu* viewMenu = menu->addMenu("&View");
            QAction *act = viewMenu->addAction("&Thumbnails", this, SLOT(setViewModeMenuHandler()));
            act->setData((int)VIWM_Thumbnails);
            act->setCheckable(true);
            act->setChecked(currentViewMode == VIWM_Thumbnails);

            act = viewMenu->addAction("&Small thumbnails", this, SLOT(setViewModeMenuHandler()));
                        act->setData((int)VIWM_SmallThumbnails);
                        act->setCheckable(true);
                        act->setChecked(currentViewMode == VIWM_SmallThumbnails);

            act = viewMenu->addAction("&Icons", this, SLOT(setViewModeMenuHandler()));
            act->setData((int)VIWM_Icons);
            act->setCheckable(true);
            act->setChecked(currentViewMode == VIWM_Icons);

            act = viewMenu->addAction("&List", this, SLOT(setViewModeMenuHandler()));
            act->setData((int)VIWM_SmallIcons);
            act->setCheckable(true);
            act->setChecked(currentViewMode == VIWM_SmallIcons);

            act = viewMenu->addAction("&Detail", this, SLOT(setViewModeMenuHandler()));
            act->setData((int)VIWM_Detail);
            act->setCheckable(true);
            act->setChecked(currentViewMode == VIWM_Detail);


//            menu->addSeparator();
//            QAction *act = menu->addAction("&Properties",
//                            this, SLOT(properties()));
//            act->setData(source);

            menu->popup(point);
        }

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
        act->setData(source);

        act = menu->addAction("&View weather at time",
                              this, SLOT(viewWeather()));
        act->setData(source);

        menu->addSeparator();

        act = menu->addAction("&Save As...",
                        this, SLOT(saveImageAs()));
        act->setData(source);

        menu->addSeparator();

        act = menu->addAction("&Properties",
                        this, SLOT(properties()));
        act->setData(source);

#ifdef QT_DEBUG
        act = menu->addAction("Test find index",
                              this, SLOT(testFindIndex()));
        act->setData(source);
#endif

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
        act->setData(source);

#ifdef QT_DEBUG
        act = menu->addAction("Test find index",
                              this, SLOT(testFindIndex()));
        act->setData(source);
#endif

#ifdef QT_DEBUG
        // In Qt 4.8 at least this crashes sometime (looks like Qt bug). Its
        // also very slow via the web data source.
        if (source == CMS_Tree){
            // Tree only!
            menu->addSeparator();

            act = menu->addAction("&Expand all",
                                  this, SLOT(expandRecursively()));
            act->setData(source);
            act = menu->addAction("&Collapse all",
                                  this, SLOT(collapseRecursively()));
            act->setData(source);
        }
#endif

        //menu->addSeparator();

        menu->popup(point);
    }
}

void ViewImagesWindow::openImageInWindow() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        ContextMenuSource menuSource = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (menuSource == CMS_List) {
            index = ui->lvImageList->currentIndex();
        } else if (menuSource == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
        } else if (menuSource == CMS_Toolbar) {
            index = currentImageIndex;
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
        ContextMenuSource menuSource = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (menuSource == CMS_List) {
            index = ui->lvImageList->currentIndex();
        } else if (menuSource == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
        } else if (menuSource == CMS_Toolbar) {
            index = currentImageIndex;
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
        ContextMenuSource menuSource = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (menuSource == CMS_List) {
            index = ui->lvImageList->currentIndex();
        } else if (menuSource == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
        } else if (menuSource == CMS_Toolbar) {
            index = currentImageIndex;
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
        ContextMenuSource menuSource = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (menuSource == CMS_List) {
            index = ui->lvImageList->currentIndex();
            if (!index.isValid()) {
                index = ui->lvImageList->rootIndex();
            }
        } else if (menuSource == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
            if (!index.isValid()) {
                index = ui->tvDetail->rootIndex();
            }
        } else if (menuSource == CMS_Tree){
            index = ui->tvImageSet->currentIndex();
        } else if (menuSource == CMS_Toolbar) {
            index = currentImageIndex;
        }

        if (index.isValid()) {
            if (model->isImage(index)) {
                QImage image = model->image(index);
                QString filename = model->imageTemporaryFileName(index);
                ImageInfo info = model->imageInfo(index);

                ImageWidget::showProperties(info, image, filename);
            } else {
                // TODO: show properties for a folder.
            }
        }
    }
}

void ViewImagesWindow::openItem() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        ContextMenuSource source = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (source == CMS_List) {
            index = ui->lvImageList->currentIndex();
        } else if (source == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
        } else if (source == CMS_Toolbar) {
            index = currentImageIndex;
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        if (index.isValid() && !model->isImage(index)) {
            ui->tvImageSet->expand(index);
            setViewIndex(index);
        }
    }
}

void ViewImagesWindow::expandNow() {
    if (Settings::getInstance().showCurrentDayInImageWindow()) {
        expandCurrentDay(Settings::getInstance().selectCurrentDayInImageWindow());
    }
}

void ViewImagesWindow::expandCurrentDay(bool expandDay) {
    qDebug() << "Expanding current date";

    QDate now = QDate::currentDate();

    for (int i = 0; i < model->rowCount(); i++) {
        // Years
        QModelIndex yearIdx = model->index(i, 0);
        int year = model->itemDate(yearIdx).year();
        qDebug() << "Found year" << year;
        if (now.year() == year) {
            ui->tvImageSet->expand(yearIdx);
            for (int j = 0; j < model->rowCount(yearIdx); j++) {
                // Months
                QModelIndex monthIdx = yearIdx.child(j, 0);
                int month = model->itemDate(monthIdx).month();
                qDebug() << "Found month" << month;
                if (month == now.month()) {
                    ui->tvImageSet->expand(monthIdx);
                    if (expandDay) {
                        for (int k = 0; k < model->rowCount(monthIdx); k++) {
                            QModelIndex dayIdx = monthIdx.child(k, 0);
                            int day = model->itemDate(dayIdx).day();
                            qDebug() << "Found day" << day;
                            if (day == now.day()) {
                                //ui->tvImageSet->expand(dayIdx);
                                ui->tvImageSet->selectionModel()->setCurrentIndex(dayIdx, QItemSelectionModel::Select);
                                setViewIndex(dayIdx);
                                return;
                            }
                        }
                    } else {
                        ui->tvImageSet->expand(monthIdx);
                        setViewIndex(monthIdx);
                        return;
                    }
                }
            }
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

void ViewImagesWindow::setViewModeMenuHandler() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        ViewImagesWindowViewMode mode = (ViewImagesWindowViewMode)menuAction->data().toInt();
        setViewMode(mode);
    }
}

void ViewImagesWindow::setViewMode(ViewImagesWindowViewMode mode) {
    ui->actionSmall_Thumbnails->setChecked(false);
    ui->actionThumbnails->setChecked(false);
    ui->actionIcons->setChecked(false);
    ui->actionList->setChecked(false);
    ui->actionDetail->setChecked(false);

    ui->lvImageList->setSpacing(5);

    switch (mode) {
    case VIWM_Default:
    case VIWM_Thumbnails:
    case VIWM_SmallThumbnails:
        ui->lvImageList->setViewMode(QListView::IconMode);
        if (mode == VIWM_SmallThumbnails) {
            ui->lvImageList->setIconSize(QSize(Constants::MINI_THUMBNAIL_WIDTH, Constants::MINI_THUMBNAIL_HEIGHT));
            ui->actionSmall_Thumbnails->setChecked(true);
        } else {
            ui->lvImageList->setIconSize(QSize(Constants::THUMBNAIL_WIDTH, Constants::THUMBNAIL_HEIGHT));
            ui->actionThumbnails->setChecked(true);
        }
        ui->lvImageList->setModelColumn(ImageModel::COL_NAME_THUMB);
        ui->stackedWidget->setCurrentIndex(SW_ICONS);
        break;
    case VIWM_Icons:
        ui->lvImageList->setViewMode(QListView::IconMode);
        ui->lvImageList->setIconSize(QSize(32,32));
        ui->lvImageList->setModelColumn(ImageModel::COL_NAME);
        ui->stackedWidget->setCurrentIndex(SW_ICONS);
        ui->actionIcons->setChecked(true);
        break;
    case VIWM_SmallIcons:
        ui->lvImageList->setViewMode(QListView::ListMode);
        ui->lvImageList->setModelColumn(ImageModel::COL_NAME);
        ui->lvImageList->setIconSize(QSize(16,16));
        ui->lvImageList->setSpacing(0);
        ui->stackedWidget->setCurrentIndex(SW_ICONS);
        ui->lvImageList->setWrapping(true);
        ui->actionList->setChecked(true);
        break;
    case VIWM_Detail:
        ui->stackedWidget->setCurrentIndex(SW_DETAIL);
        ui->actionDetail->setChecked(true);
        break;
    }

    currentViewMode = mode;
    Settings::getInstance().setImagesWindowViewMode(mode);
}

void ViewImagesWindow::setViewIndex(QModelIndex index) {
    if (index.isValid() && model.data()->isImage(index)) {
        setViewIndex(index.parent());
        loadImageForIndex(index);
        return;
    }

    ui->tvDetail->setRootIndex(index);

    index = normaliseIndexColumn(index);

    ui->lvImageList->setRootIndex(index);
    updateLocation(index);
    updateItemCount();
}

void ViewImagesWindow::updateLocation(QModelIndex index) {
    QString l = getCurrentLocation(index);
    location->setText(QString("Location: %0").arg(l));
    setWindowTitle(l + " - Images");
}

QString ViewImagesWindow::getCurrentLocation(QModelIndex index) {
    if (index.isValid()) {
        QString indexLabel = model.data()->data(index, Qt::DisplayRole).toString();
        QString loc = getCurrentLocation(index.parent());
        if (indexLabel.isEmpty()) {
            return loc;
        }
        return loc + indexLabel + "\\";
    } else {
        return "\\";
    }
}

void ViewImagesWindow::updateItemCount() {
    int number = model.data()->rowCount(ui->lvImageList->rootIndex());
    itemCount->setText(QString("%0 items").arg(number));
}

void ViewImagesWindow::showHidePreviewPane(bool show) {
    ui->scrollArea->setVisible(show);
    ui->actionPreview->setChecked(show);
    Settings::getInstance().setImagesWindowPreviewPaneVisible(show);
}
void ViewImagesWindow::showHideTreePane(bool show) {
    ui->tvImageSet->setVisible(show);
    ui->actionTree->setChecked(show);
    Settings::getInstance().setImagesWindowNavigationPaneVisible(show);
}

void ViewImagesWindow::navigateUp() {
    qDebug() << "UP!";
    QModelIndex index;
    if (currentViewMode == VIWM_Detail) {
        index = ui->tvDetail->rootIndex();
    } else {
        index = ui->lvImageList->rootIndex();
    }

    if (!index.isValid()) {
        return; // No parent
    }

    index = index.parent();

    setViewIndex(index);
    ui->tvImageSet->expand(index);
}

void ViewImagesWindow::lazyLoadingComplete(QModelIndex index) {
    if (index == ui->lvImageList->rootIndex()) {
        updateItemCount();
    }
}

void ViewImagesWindow::imageReady(QModelIndex index) {
    qDebug() << "Image Ready" << (currentImageIndex == index) << imageLoaded << index;
    if (currentImageIndex == index && !imageLoaded) {
        loadImageForIndex(index);
    }
}

QModelIndex ViewImagesWindow::normaliseIndexColumn(QModelIndex index) {
    if (!index.isValid()) {
        return index;
    }

    int row = index.row();

    if (index.parent().isValid()) {
        return index.parent().child(row, ImageModel::COL_NAME);
    } else {
        return model.data()->index(row, ImageModel::COL_NAME);
    }
}

#ifdef QT_DEBUG
void ViewImagesWindow::testFindIndex() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        ContextMenuSource source = (ContextMenuSource)menuAction->data().toInt();
        QModelIndex index;
        if (source == CMS_List) {
            index = ui->lvImageList->currentIndex();
        } else if (source == CMS_Detail) {
            index = ui->tvDetail->currentIndex();
        } else {
            index = ui->tvImageSet->currentIndex();
        }

        bool result = model.data()->testFindIndex(index);

        if (result) {
            QMessageBox::information(NULL, "result", "success");
        } else {
            QMessageBox::information(NULL, "result", "fail");
        }
    }
}
#endif
