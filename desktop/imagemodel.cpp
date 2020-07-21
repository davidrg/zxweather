#include "imagemodel.h"

#include <QList>
#include <QStringList>
#include <QIcon>
#include <QTimer>
#include <QtDebug>
#include <QMimeData>
#include <QTemporaryFile>
#include <QUrl>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDir>
#include <QTime>
#include <QPainter>
#include <QApplication>

#include "settings.h"
#include "constants.h"

class TreeItem{
public:
    TreeItem(ItemType type, QDate date, QString sourceCode, QString text = "",
             TreeItem *parent = 0, int imageId = -1);
    ~TreeItem();
    ItemType itemType() const;
    QString text() const;
    void appendChild(TreeItem* child);
    void deleteChild(TreeItem *child);
    TreeItem* parent() const;
    TreeItem* child(int row);
    int childCount() const;
    int row() const;
    bool childrenLoaded() const;
    QIcon icon() const;
    QIcon thumbnail() const;
    QImage image() const;
    QFile* imageFile() const;
    void setLoadRequested();
    bool loadRequested() const;
    QDate date() const;
    QString sourceCode() const;
    void deleteChildren();
    void setThumbnail(QImage thumbnailImage);
    int id() const;
    void setImage(ImageInfo info, QImage image, QString cacheFile);
    ImageInfo imageInfo() const;

private:
    TreeItem *parentNode;
    QList<TreeItem*> childItems;
    ItemType type;
    QString textValue;
    bool mChildrenLoaded;
    bool mLoadRequested;
    QDate mDate;
    QString mSourceCode;
    QImage mThumbnail;
    int imageId;
    QFile* temporaryImageFile;
    ImageInfo info;
};

TreeItem::TreeItem(ItemType type, QDate date, QString sourceCode, QString text,
                   TreeItem *parent, int imageId) {
    this->type = type;
    this->textValue = text;
    this->parentNode = parent;    
    mChildrenLoaded = false;
    mDate = date;
    mSourceCode = sourceCode;
    mLoadRequested = false;
    this->imageId = imageId;
    temporaryImageFile = NULL;

    if ((type == IT_DAY && sourceCode != "") || type == IT_IMAGE_SOURCE) {
        // Add a place-holder "Loading..." item. This will be removed
        // automatically when the first child is added.
        this->childItems.append(new TreeItem(IT_LOADING,
                                             date,
                                             sourceCode,
                                             QApplication::translate("ImageModel","Loading..."),
                                             this,
                                             -1));
    }
}

TreeItem::~TreeItem() {
    qDeleteAll(childItems);
    childItems.clear();

    if (temporaryImageFile) {
        delete temporaryImageFile;
    }
}

void TreeItem::deleteChildren() {
    qDeleteAll(childItems);
    childItems.clear();
}

ItemType TreeItem::itemType() const {
    return type;
}

QString TreeItem::text() const {
    return textValue;
}

int TreeItem::id() const {
    if (itemType() == IT_IMAGE)
        return imageId;
    return -1;
}

void TreeItem::appendChild(TreeItem* child) {

    if (!mChildrenLoaded && !childItems.isEmpty() &&
            (type == IT_DAY || type == IT_IMAGE_SOURCE)) {
        // Delete the place-holder loading item
        qDeleteAll(childItems);
        childItems.clear();
    }

    childItems.append(child);
    mChildrenLoaded = true;
}

void TreeItem::deleteChild(TreeItem *child) {
    childItems.removeOne(child);
    delete child;
}

TreeItem* TreeItem::parent() const {
    return parentNode;
}

TreeItem* TreeItem::child(int row) {
    return childItems.value(row);
}

int TreeItem::childCount() const {
    return childItems.count();
}

int TreeItem::row() const {
    if (parentNode) {
        return parentNode->childItems.indexOf(const_cast<TreeItem*>(this));
    }
    return 0;
}

bool TreeItem::childrenLoaded() const {
    return mChildrenLoaded;
}

void TreeItem::setThumbnail(QImage thumbnailImage) {
    mThumbnail = thumbnailImage;
}

QIcon TreeItem::icon() const {
    QIcon icon;

    switch(type) {
    case IT_YEAR:
    case IT_MONTH:
    case IT_DAY:
    case IT_IMAGE_SOURCE:
        icon.addFile(":/icons/folder-horizontal", QSize(16,16), QIcon::Normal);
        icon.addFile(":/icons/folder-horizontal-32", QSize(32,32), QIcon::Normal);
        //icon.addFile(":/icons/folder-horizontal-open", QSize(16, 16), QIcon::Selected);
        break;
    case IT_IMAGE:
        if (info.mimeType.startsWith("video/")) {
            icon.addFile(":/icons/film", QSize(16, 16));
            icon.addFile(":/icons/film-32", QSize(32, 32));
        } else if (info.mimeType.startsWith("audio/")) {
            // TODO: get an audio file icon
            icon.addFile(":/icons/audio", QSize(16, 16));
            icon.addFile(":/icons/audio-32", QSize(32, 32));
        } else {
            icon.addFile(":/icons/image", QSize(16,16));
            icon.addFile(":/icons/image-32", QSize(32, 32));
        }

//        if (!mThumbnail.isNull()) {

//            int width = Constants::THUMBNAIL_WIDTH;
//            int height = Constants::THUMBNAIL_HEIGHT;

//            if (width > height) {
//                height = width;
//            } else if (height > width) {
//                width = height;
//            }

//            QPixmap source = QPixmap::fromImage(mThumbnail);
//            QPixmap dest(width, height);
//            dest.fill(Qt::transparent);
//            QPixmap resized = source.scaled(dest.size(), Qt::KeepAspectRatio);
//            QPainter p(&dest);
//            if (resized.width() < dest.width())
//                p.drawPixmap( (dest.width() - resized.width())/2, 0, resized);
//            else
//                p.drawPixmap( 0, (dest.height() - resized.height())/2, resized);
//            p.end();

//            icon.addPixmap(dest);

//            //icon.addPixmap(QPixmap::fromImage(thumbnail), QIcon::Normal);
//        }


        break;
    case IT_LOADING:
    case IT_ROOT:
    default:
        ; // No icon.
    }

    return icon;
}

QIcon TreeItem::thumbnail() const {
    QIcon icon;

    switch(type) {
    case IT_YEAR:
    case IT_MONTH:
    case IT_DAY:
    case IT_IMAGE_SOURCE:
        icon.addFile(":/icons/folder-horizontal", QSize(16,16), QIcon::Normal);
        icon.addFile(":/icons/folder-horizontal-32", QSize(32,32), QIcon::Normal);
        //icon.addFile(":/icons/folder-horizontal-open", QSize(16, 16), QIcon::Selected);
        break;
    case IT_IMAGE:
        if (info.mimeType.startsWith("video/")) {
            icon.addFile(":/icons/film", QSize(16, 16));
            icon.addFile(":/icons/film-32", QSize(32, 32));
        } else if (info.mimeType.startsWith("audio/")) {
            // TODO: get an audio file icon
            icon.addFile(":/icons/audio", QSize(16, 16));
            icon.addFile(":/icons/audio-32", QSize(32, 32));
        } else {
            icon.addFile(":/icons/image", QSize(16,16));
            icon.addFile(":/icons/image-32", QSize(32, 32));
        }

        if (!mThumbnail.isNull()) {
            icon.addPixmap(QPixmap::fromImage(mThumbnail).scaled(Constants::MINI_THUMBNAIL_WIDTH, Constants::MINI_THUMBNAIL_HEIGHT), QIcon::Normal);
            icon.addPixmap(QPixmap::fromImage(mThumbnail).scaled(Constants::THUMBNAIL_WIDTH, Constants::THUMBNAIL_HEIGHT), QIcon::Normal);
        }

        break;
    case IT_LOADING:
    case IT_ROOT:
    default:
        ; // No icon.
    }

    return icon;
}

void TreeItem::setImage(ImageInfo info, QImage image, QString cacheFile) {
    this->info = info;

    bool fileOk = true;
    if (cacheFile.isNull() || cacheFile.isEmpty()) {
        fileOk = false; // No filename specified
    }

    QFileInfo fi(cacheFile);
    if (!fi.exists() || !fi.isFile()) {
        fileOk = false; // File missing or not a file
    }

    if (fileOk) {
        // The supplied cache file exists on disk! We'll just re-use that
        temporaryImageFile = new QFile(cacheFile);
    } else {
        // Cache file doesn't exist (or isn't a file or wasn't specified).
        // We'll dump the supplied image to a temporary file somewhere rather
        // than holding it in RAM. We'll do this as a temporary file so it will
        // be cleaned up later.

        if (image.isNull()) {
            qWarning() << "No cache file or image data for: " << cacheFile;
        }

#if QT_VERSION >= 0x050000
        QString filename = QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation) + QDir::separator() + "temp" + QDir::separator();
#else
        QString filename = QDesktopServices::storageLocation(
                    QDesktopServices::TempLocation) + QDir::separator() + "zxweather" + QDir::separator();
#endif

        if (!QDir(filename).exists())
            QDir().mkpath(filename);

        temporaryImageFile = new QTemporaryFile(filename +
                    QDir::separator() + date().toString(Qt::ISODate) +
                    " "+ text().replace(":","_") + " XXXXXX.jpeg");
        image.save(temporaryImageFile);
        temporaryImageFile->flush();
        temporaryImageFile->close();
    }
}

ImageInfo TreeItem::imageInfo() const {
    return this->info;
}

QFile* TreeItem::imageFile() const {
    return temporaryImageFile;
}

QImage TreeItem::image() const {
    if (temporaryImageFile == NULL) {
        return QImage();
    }

    temporaryImageFile->open(QIODevice::ReadOnly);
    QImage image = QImage::fromData(temporaryImageFile->readAll());
    temporaryImageFile->close();

    return image;
}

void TreeItem::setLoadRequested() {
    mLoadRequested = true;
}

bool TreeItem::loadRequested() const {
    return mLoadRequested;
}

QDate TreeItem::date() const {
    return mDate;
}

QString TreeItem::sourceCode() const {
    return mSourceCode;
}




ImageModel::ImageModel(AbstractDataSource *dataSource, QObject *parent)
    : QAbstractItemModel(parent)
{
    // Create the root item
    rootNode = 0;
    treeBuilt = false;
    resetTree();
    loadingImages = false;

    if (dataSource != 0) {
        setDataSource(dataSource);
    }
}

void ImageModel::setDataSource(AbstractDataSource *dataSource) {
    this->dataSource = dataSource;

    connect(dataSource,
            SIGNAL(imageDatesReady(QList<ImageDate>,QList<ImageSource>)),
            this, SLOT(imageDatesReady(QList<ImageDate>,QList<ImageSource>)));
    connect(dataSource,
            SIGNAL(imageListReady(QList<ImageInfo>)),
            this,
            SLOT(imageListReady(QList<ImageInfo>)));
    connect(dataSource,
            SIGNAL(thumbnailReady(int,QImage)),
            this,
            SLOT(thumbnailReady(int,QImage)));
    connect(dataSource,
            SIGNAL(imageReady(ImageInfo,QImage,QString)),
            this,
            SLOT(imageReady(ImageInfo,QImage,QString)));

    // Launch a query to populate the tree
    dataSource->fetchImageDateList();
}


QMap<QString, ImageSource> imageSourcesListToMap(QList<ImageSource> sources) {
    QMap<QString, ImageSource> imageSources;
    foreach (ImageSource src, sources) {
        imageSources[src.code] = src;
    }
    return imageSources;
}

void ImageModel::resetTree() {
    pendingThumbnails.clear();
    imageLoadRequestQueue.clear();
    if (!rootNode) {
        delete rootNode;
    }
    rootNode = new TreeItem(IT_ROOT, QDate(), "", "", NULL, -1);
}

typedef QMap<int, QMap<int, QMap<int, QStringList> > > ImageSourceMapTree;

ImageSourceMapTree buildImageSourceMapTree(QList<ImageDate> dates) {
    ImageSourceMapTree mapTree;

    // Build a map tree!
    foreach (ImageDate imageDate, dates) {
        int year = imageDate.date.year();
        int month = imageDate.date.month();
        int day = imageDate.date.day();

        if (!mapTree.contains(year)) {
            mapTree[year] = QMap<int, QMap<int, QStringList> >();
        }

        if (!mapTree[year].contains(month)) {
            mapTree[year][month] = QMap<int, QStringList>();
        }

        mapTree[year][month][day] = imageDate.sourceCodes;
    }

    return mapTree;
}


void ImageModel::imageDatesReady(QList<ImageDate> dates,
                                 QList<ImageSource> sources) {

    qDebug() << "Image dates ready!";
    if (treeBuilt) {
        qDebug() << "Tree already built - updating...";
        updateTree(dates, sources);
    } else {
        qDebug() << "Building tree...";
        buildTree(dates, sources);
    }
}

void ImageModel::buildTree(QList<ImageDate> dates, QList<ImageSource> sources) {
    qDebug() << "Rebuild tree...";
    beginResetModel();
    resetTree();

    /* Here we build the initial tree structure. This will be:
     *   Year > Month > Day > Image source
     * Image source nodes are only created where more than one iamge source
     * exists for that particular day.
     *
     * The process for doing this is a little convoluted as the TreeItem class
     * doesn't currently provide a way of getting a particular item efficiently
     * by its value - if we wanted the TreeItem representing the year 2016 we'd
     * have to loop through all year level TreeItems.
     */

    // This is a tree of image sources:
    // sourceTree[year][month][day] = list of image sources with one or more
    // images for that date
    ImageSourceMapTree sourceTree = buildImageSourceMapTree(dates);

    QMap<QString, ImageSource> imageSources = imageSourcesListToMap(sources);

    // Build the actual tree
    foreach (int year, sourceTree.keys()) {
        TreeItem* yearNode = new TreeItem(IT_YEAR,
                                          QDate(year, 1, 1),
                                          "",
                                          QString::number(year),
                                          rootNode,
                                          -1);
        rootNode->appendChild(yearNode);
        foreach (int month, sourceTree[year].keys()) {
            QString monthName = QDate::longMonthName(month);
            TreeItem *monthNode = new TreeItem(IT_MONTH,
                                               QDate(year, month, 1),
                                               "",
                                               monthName,
                                               yearNode,
                                               -1);
            yearNode->appendChild(monthNode);
            foreach (int day, sourceTree[year][month].keys()) {

                QStringList imageSourceCodes = sourceTree[year][month][day];

                QString srcCode = "";
                if (imageSourceCodes.count() == 1) {
                    srcCode = imageSourceCodes.first();
                }

                QDate date(year, month, day);

                TreeItem *dayNode = new TreeItem(IT_DAY,
                                                 date,
                                                 srcCode,
                                                 QString::number(day),
                                                 monthNode,
                                                 -1);

                monthNode->appendChild(dayNode);

                // Only bother adding image source nodes if there is more than
                // one for this particular date.
                if (imageSourceCodes.count() > 1) {
                    foreach (QString imageSourceCode, imageSourceCodes) {

                        TreeItem *src = new TreeItem(
                                    IT_IMAGE_SOURCE,
                                    date,
                                    imageSourceCode,
                                    imageSources[imageSourceCode].name,
                                    dayNode,
                                    -1);
                        dayNode->appendChild(src);
                    }
                }
            }
        }
    }
    qDebug() << "Tree rebuild complete.";
    treeBuilt = true;
    endResetModel();
    emit modelReady();
}

TreeItem* GetYear(TreeItem *root, int year) {
    for (int yearIdx = 0; yearIdx < root->childCount(); yearIdx++) {
        TreeItem *yearNode = root->child(yearIdx);
        if (yearNode->date() == QDate(year, 1, 1)) {
            return yearNode;
        }
    }

    return NULL;
}

TreeItem* GetMonth(TreeItem *root, int year, int month, TreeItem* yearNode=NULL) {
    if (yearNode == NULL) {
        yearNode = GetYear(root, year);
    }

    for (int monthIdx = 0; monthIdx < yearNode->childCount(); monthIdx++) {
        TreeItem *monthNode = yearNode->child(monthIdx);
        if (monthNode->date() == QDate(year, month, 1)) {
            return monthNode;
        }
    }

    return NULL;
}

TreeItem* GetDay(TreeItem *root, QDate date, TreeItem* monthNode=NULL) {
    if (monthNode == NULL) {
        monthNode = GetMonth(root, date.year(), date.month());
    }

    for (int dayIdx = 0; dayIdx < monthNode->childCount(); dayIdx++) {
        TreeItem *dayNode = monthNode->child(dayIdx);
        if (dayNode->date() == date) {
            return dayNode;
        }
    }

    return NULL;
}

TreeItem* GetImageSourceNode(TreeItem *root, QDate date, QString imageSourceCode, TreeItem* dayNode=NULL) {
    if (dayNode == NULL) {
        dayNode = GetDay(root, date);
    }

    if (dayNode->sourceCode() == imageSourceCode) {
        // there is only one image source for this date so the day node *is* the image source
        // node.
        return dayNode;
    }

    for (int srcIdx = 0; srcIdx < dayNode->childCount(); srcIdx++) {
        TreeItem *sourceNode = dayNode->child(srcIdx);
        if (sourceNode->date() == date && sourceNode->sourceCode() == imageSourceCode) {
            return sourceNode;
        }
    }

    return NULL;
}

void ImageModel::updateTree(QList<ImageDate> dates, QList<ImageSource> sources) {
    // Here we want to do something similar to build tree but we'll only create nodes
    // were they don't already exist. Any source-level nodes (an IT_IMAGE_SOURCE or an
    // IT_DATE with no child IT_IMAGE_SOURCEs) will be queued for refresh as part of this.
    // This is because the main reason for a tree update is a new image being received so we
    // want that image immediately available.

    ImageSourceMapTree sourceTree = buildImageSourceMapTree(dates);

    QMap<QString, ImageSource> imageSources = imageSourcesListToMap(sources);

    qDebug() << "======================== Updating Image Tree ========================";

    // Update the tree.
    foreach (int year, sourceTree.keys()) {
        TreeItem* yearNode = GetYear(rootNode, year);
        bool yearCreated = false;
        if (yearNode == NULL) {

            // Couldn't find the year. Create it.
            yearNode = new TreeItem(IT_YEAR,
                                    QDate(year, 1, 1),
                                    "",
                                    QString::number(year),
                                    rootNode,
                                    -1);
            yearCreated = true;
            QModelIndex root = index(0,0);
            int rows = rowCount(root);
            beginInsertRows(root, rows, rows+1);

            rootNode->appendChild(yearNode);
            qDebug() << "Created Year:" << year;
        }

        foreach (int month, sourceTree[year].keys()) {
            TreeItem *monthNode = GetMonth(rootNode, year, month, yearNode);

            bool monthCreated = false;
            if (monthNode == NULL) {
                // Couldn't find the year. Create it.
                QString monthName = QDate::longMonthName(month);
                monthNode = new TreeItem(IT_MONTH,
                                         QDate(year, month, 1),
                                         "",
                                         monthName,
                                         yearNode,
                                         -1);
                monthCreated = true;

                if (!yearCreated) {
                    QModelIndex yearIndex = findIndex(IT_YEAR, year);
                    int rows = rowCount(yearIndex);
                    beginInsertRows(yearIndex, rows, rows+1);
                }

                yearNode->appendChild(monthNode);
                qDebug() << "Created Month: " << year << month;
            }

            foreach (int day, sourceTree[year][month].keys()) {

                QStringList imageSourceCodes = sourceTree[year][month][day];

                QString srcCode = "";
                if (imageSourceCodes.count() == 1) {
                    srcCode = imageSourceCodes.first();
                }

                QDate date(year, month, day);

                TreeItem *dayNode = GetDay(rootNode, date, monthNode);
                bool dayCreated = false;

                if (dayNode != NULL && dayNode->sourceCode() != "" && imageSourceCodes.count() > 1) {
                    // We have multiple image souces for this date but the date node we found
                    // is for a single image source. We'll just delete the day node and recreate
                    // it with multiple image sources.

                    qDebug() << "Creating day for multiple image sources: " << date;

                    QModelIndex dayIndex = findIndex(IT_DAY, year, month, day);
                    QModelIndex monthIndex = dayIndex.parent();

                    beginRemoveRows(monthIndex, dayIndex.row(), dayIndex.row());
                    TreeItem *parent = dayNode->parent();
                    parent->deleteChild(dayNode);
                    dayNode = NULL;
                    endRemoveRows();
                }

                if (dayNode == NULL) {

                    // Couldn't find the day. Create it.
                    dayNode = new TreeItem(IT_DAY,
                                           date,
                                           srcCode,
                                           QString::number(day),
                                           monthNode,
                                           -1);
                    dayCreated = true;

                    if (!yearCreated && !monthCreated) {
                        QModelIndex monthIndex = findIndex(IT_MONTH, year, month);
                        int rows = rowCount(monthIndex);
                        beginInsertRows(monthIndex, rows, rows+1);
                    }


                    monthNode->appendChild(dayNode);
                    qDebug() << "Created Date:" << date;
                }

                QModelIndex dayIndex = findIndex(IT_DAY, year, month, day);

                // Only bother adding image source nodes if there is more than
                // one for this particular date.
                if (imageSourceCodes.count() > 1) {
                    foreach (QString imageSourceCode, imageSourceCodes) {
                        if (GetImageSourceNode(rootNode, date, imageSourceCode, dayNode) == NULL) {
                            // Image source doesn't exist. Create it.
                            TreeItem *src = new TreeItem(
                                        IT_IMAGE_SOURCE,
                                        date,
                                        imageSourceCode,
                                        imageSources[imageSourceCode].name,
                                        dayNode,
                                        -1);

                            if (!yearCreated && !monthCreated && !dayCreated) {
                                int rows = rowCount(dayIndex);
                                beginInsertRows(dayIndex, rows, rows+1);
                            }

                            dayNode->appendChild(src);
                            qDebug() << "Created Image Source:" << date << imageSourceCode;

                            if (!yearCreated && !monthCreated && !dayCreated) {
                                endInsertRows();
                            }

                            if (src->childCount() > 0) {
                                // Queue for loading
                                QModelIndex sourceIndex = findIndex(IT_IMAGE_SOURCE, year, month, day, imageSourceCode);
                                loadItem(index(0, 0, sourceIndex));
                            }
                        }
                    }
                } else if (dayCreated) {
                    // queue for loading
                    loadItem(index(0, 0, dayIndex));
                }

                if (!yearCreated && !monthCreated && dayCreated) {
                    endInsertRows();
                }
            }

            if (!yearCreated && monthCreated) {
                endInsertRows();
            }
        }

        if (yearCreated) {
            endInsertRows();
        }
    }
    qDebug() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^";
}

ImageModel::~ImageModel() {
    if (rootNode) {
        delete rootNode;
    }
}

QModelIndex ImageModel::findIndex(ItemType type, int year, int month, int day,
                      QString source, QTime time) {
    return findIndex(type, QDate(year, month, day), source, time);
}

QModelIndex ImageModel::findIndex(ItemType type, QDate date, QString source, QTime time) {
    int rootRows = rowCount();
    for (int row = 0; row < rootRows; row++) {
        QModelIndex idx = findIndex(index(row,0), type, date, source, time);
        if (idx.isValid()) {
            return idx;
        }
    }

    return QModelIndex();
}

QModelIndex ImageModel::findIndex(QModelIndex index, ItemType type, QDate date,
                                  QString source, QTime time) {

    //qDebug() << "findIndex" << type << date << source << time;

    if (type == IT_ROOT) {
        //qDebug() << "Found root.";
        return this->index(0,0);
    }

    TreeItem *treeNode = static_cast<TreeItem*>(index.internalPointer());

    if (treeNode == NULL) {
        qWarning() << "Index has no tree node!";
        return QModelIndex(); // This shouldn't happen.
    }

    if (treeNode->itemType() == type && treeNode->date() == date) {
        if (type == IT_YEAR || type == IT_MONTH || type == IT_DAY) {
            // For year, month or day nodes all we need to check is the current index is
            // of the correct type and the date match. Time and source don't matter.
            //qDebug() << "Found year, month or day";
            return index;
        }
        else if (treeNode->sourceCode() == source) {
            // For source nodes, the source code needs to match
            if (type == IT_IMAGE_SOURCE) {
                //qDebug() << "Found image source.";
                return index;
            } else if (type == IT_IMAGE && treeNode->imageInfo().timeStamp == QDateTime(date, time)) {
                // And for an image, the time must also match.
                //qDebug() << "Found image.";
                return index;
            }
        }
    }

    // Current model index isn't the one we're after. If it has children we could search
    // them...
    if (!this->hasChildren(index)) {
        qDebug() << "Node has no children - at end of branch";
        return QModelIndex(); // No matches on this branch of the tree.
    }

    //qDebug() << "Searching children...";

    // Descend into the children.
    for(int i = 0; i < rowCount(index); i++) {
        //qDebug() << "Child" << i;
        QModelIndex child = this->index(i, 0, index);
        TreeItem *treeNode = static_cast<TreeItem*>(child.internalPointer());
        if (treeNode == NULL) {
            qWarning() << "Index has no associated tree node!";
            return QModelIndex(); // this shouldn't happen.
        }

        QModelIndex idx;
        ItemType currentItemType = treeNode->itemType();

        //qDebug() << "Item" << currentItemType << "Date" << treeNode->date() << "Searching for" << type << date;
        if (currentItemType == IT_ROOT || currentItemType == type) {
            idx = findIndex(child, type, date, source, time);
        } else if (currentItemType == IT_YEAR && treeNode->date() == QDate(date.year(), 1, 1)) {
            idx = findIndex(child, type, date, source, time);
        } else if (currentItemType == IT_MONTH && treeNode->date() == QDate(date.year(), date.month(), 1)) {
                idx = findIndex(child, type, date, source, time);
        } else if (currentItemType == IT_DAY && treeNode->date() == date) {
            idx = findIndex(child, type, date, source, time);
        } else if (currentItemType == IT_IMAGE_SOURCE && treeNode->date() == date && treeNode->sourceCode() == source) {
            idx = findIndex(child, type, date, source, time);
        }
        if (idx.isValid()) {
            //qDebug() << "found index" << type << date << source << time;
            return idx; // Found it
        }
        // Else, check the next child.
    }

    //qDebug() << "Could not find index.";

    return QModelIndex();
}


void ImageModel::loadItem(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this) {
        return;
    }

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (item->itemType() == IT_LOADING && !item->loadRequested()) {
        QString source = item->sourceCode();
        QDate date = item->date();

        qDebug() << "Loading images for" << source << "on" << date << "...";

        ImageLoadRequest request;
        request.date = date;
        request.imageSourceCode = source;
        request.treeItem = item->parent();
        request.index = index.parent();
        const_cast<QList<ImageLoadRequest> *>(&imageLoadRequestQueue)->append(request);

        QTimer::singleShot(1,
                           const_cast<ImageModel*>(this),
                           SLOT(processImageLoadRequestQueue()));

        item->setLoadRequested();
    }
}

void ImageModel::newImage(NewImageInfo info) {
    qDebug() << "============================================";
    qDebug() << "New image! " << info.imageSourceCode << info.timestamp;

    QModelIndex dateIndex = findIndex(IT_DAY, info.timestamp.date());
    QModelIndex sourceIndex = findIndex(dateIndex, IT_IMAGE_SOURCE, info.timestamp.date(),
                                        info.imageSourceCode, QTime());

    // TODO: What if only one image source was present when the date list was loaded
    // and this image is not from that source? we need to rebuild the tree from the date
    // node down. Same goes for if the date (or month or year) node doesn't exist at all.

    QModelIndex index;

    if (sourceIndex.isValid()) {
        qDebug() << "Found image source node";
        index = sourceIndex;
    } else if (dateIndex.isValid()) {
        index = dateIndex;
        qDebug() << "Found date node";
    }

    TreeItem *item = NULL;
    if (index.isValid()) {
        item = static_cast<TreeItem*>(index.internalPointer());

        if (item->sourceCode().toLower() != info.imageSourceCode.toLower()) {
            // Oops! the item we found isn't the one we're after. Whats probably happened
            // is when the date list was loaded only one image source had produced images. Now
            // we've received an image from a different source on that date. This date should
            // now have two or more image source nodes on it containing the images rather than
            // the images being directly under the date node.
            index = QModelIndex();
            item = NULL;

            qDebug() << "Found node was not of the correct image source" << item->sourceCode() << info.imageSourceCode;
        }
    }

    if (!index.isValid()) {
        qDebug() << "Could not find suitable index to add image - refreshing dates from server...";

        // We couldn't locate an appropriate node to slot this image under. We need to rebuild
        // part of the tree...

        // Fetching the image date list with an already loaded tree will result in the tree
        // being updated with date and/or image source nodes pre-emptively loaded so we  don't
        // need to proc
        dataSource->fetchImageDateList();

        return;
    }

    qDebug() << "Requesting image load...";
    ImageLoadRequest request;
    request.date = info.timestamp.date();
    request.imageSourceCode = info.imageSourceCode;
    request.treeItem = item;
    request.index = index;
    const_cast<QList<ImageLoadRequest> *>(&imageLoadRequestQueue)->append(request);
    QTimer::singleShot(1,
                       const_cast<ImageModel*>(this),
                       SLOT(processImageLoadRequestQueue()));
}

QVariant ImageModel::data(const QModelIndex &index, int role) const {

    if (!index.isValid() || index.model() != this) {
        return QVariant();
    }

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    loadItem(index);

    // If we supported multiple values we'd do something like this instead:
    // return item->data(index.column());

    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole: {
        if (item->itemType() == IT_IMAGE) {
            switch (index.column()) {
            case COL_NAME:
            case COL_NAME_THUMB:
                return item->text();
            case COL_TIME: return item->imageInfo().timeStamp;
            case COL_TYPE: return item->imageInfo().imageTypeName;
            case COL_SIZE: {
                if (item->imageFile() == NULL) {
                    return "";
                }

                QFileInfo info(item->imageFile()->fileName());
                qint64 size = info.size();

                float humanSize = size;
                QString humanSuffix = QString::null;
                if (humanSize > 1024) {
                    humanSize /= 1024;
                    humanSuffix = tr("KiB");
                }
                if (humanSize > 1024) {
                    humanSize /= 1024;
                    humanSuffix = tr("MiB");
                }
                if (humanSuffix.isNull()) {
                    return QString(tr("%1 bytes")).arg(size);
                }
                else {
                    return QString(tr("%1 %2")).arg(QString::number(humanSize,'f', 2)).arg(humanSuffix);
                }
            }
            case COL_DESCRIPTION: return item->imageInfo().description;
            case COL_MIME_TYPE: return item->imageInfo().mimeType;
            case COL_IMAGE_SOURCE: return item->imageInfo().imageSource.name;
            default:
                return "";
            }
        } else if (item->itemType() == IT_LOADING) {
            switch(index.column()) {
            case COL_NAME:
            case COL_NAME_THUMB:
                return item->text();
            case COL_TIME: return item->date();
            case COL_TYPE: return tr("Loading");
            case COL_SIZE: return "";
            case COL_DESCRIPTION: {
                return tr("Images for this date are being loaded...");
            }
            case COL_MIME_TYPE: return "";
            case COL_IMAGE_SOURCE: return ""; // No source name available except on image nodes.
            }
        } else {
            switch(index.column()) {
            case COL_NAME:
            case COL_NAME_THUMB:
                return item->text();
            case COL_TIME: return item->date();
            case COL_TYPE: return tr("Folder");
            case COL_SIZE: return ""; // No size available for folders (too expensive to compute)
            case COL_DESCRIPTION: {
                switch (item->itemType()) {
                case IT_DAY:
                    return tr("Day");
                case IT_MONTH:
                    return tr("Month");
                case IT_YEAR:
                    return tr("Year");
                case IT_IMAGE_SOURCE:
                    return tr("Image source");
                default:
                    return "";
                }
            }
            case COL_MIME_TYPE: return "";
            case COL_IMAGE_SOURCE: return ""; // No source name available except on image nodes.
            }
        }
        return item->text();
    }
    case Qt::DecorationRole:
        if (index.column() == COL_NAME)
            return item->icon();
        if (index.column() == COL_NAME_THUMB)
            return item->thumbnail();
        return QVariant();
    case Qt::ToolTipRole:
        return item->date().toString();
    case Qt::WhatsThisRole:
        switch (item->itemType()) {
        case IT_DAY:
            return tr("Day");
        case IT_IMAGE:
            return tr("Image");
        case IT_IMAGE_SOURCE:
            return tr("Image source");
        case IT_LOADING:
            return tr("Loading...");
        case IT_MONTH:
            return tr("Month");
        case IT_ROOT:
            return tr("Images");
        case IT_YEAR:
            return tr("Year");
        default:
            return QVariant();
        }
    default:
        return QVariant();
    }
}

QVariant ImageModel::headerData(int section, Qt::Orientation orientation,
                                int role) const {

    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch(section) {
        case 0: return tr("Title");
        case 1: return tr("Time");
        case 2: return tr("Type");
        case 3: return tr("Size");
        case 4: return tr("Description");
        case 5: return tr("MIME Type");
        case 6: return tr("Source");
        default:
            return "";
        }
    }

    return QVariant(section);

}


QModelIndex ImageModel::index(int row, int column,
                              const QModelIndex &parent) const {

    // TODO: verify this is ok.

    QModelIndex index;

    if (hasIndex(row, column, parent)) {
        TreeItem *parentItem;

        if (!parent.isValid()) {
            parentItem = rootNode;
        } else {
            parentItem = static_cast<TreeItem*>(parent.internalPointer());
        }

        TreeItem *childItem = parentItem->child(row);

        if (childItem) {
            index = createIndex(row, column, childItem);
        }
    }

    return index;
}

QModelIndex ImageModel::parent(const QModelIndex &index) const {
    if (!index.isValid()) {
        return QModelIndex();
    }

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if (parentItem == rootNode) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

bool ImageModel::isImage(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (item->itemType() == IT_IMAGE) {
        return true;
    }

    return false;
}

int ImageModel::imageId(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    return item->id();
}

QImage ImageModel::image(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (item->itemType() == IT_IMAGE) {
        return item->image();
    }
    return QImage();
}

ImageInfo ImageModel::imageInfo(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (item->itemType() == IT_IMAGE) {
        return item->imageInfo();
    }

    qDebug() << "ImageModel: supplied index isn't an image - can't get ImageInfo";

    ImageInfo nullInfo;

    return nullInfo;
}

QDate ImageModel::itemDate(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    return item->date();
}


QString ImageModel::imageTemporaryFileName(const QModelIndex &index) const {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (item->itemType() == IT_IMAGE) {
        QFile* file = item->imageFile();
        if (file == NULL) {
            return "";
        }
        return file->fileName();
    }
    return "";
}

int ImageModel::rowCount(const QModelIndex &parent) const {
    TreeItem *parentItem;
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = rootNode;
    } else {
        parentItem = static_cast<TreeItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int ImageModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return 8;
}

bool ImageModel::hasChildren(const QModelIndex &parent) const {
    // No need to go counting rows, etc, to determine if an item has children. In this
    // model everything except images and the loading placeholder has children.

    TreeItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootNode;
    } else {
        parentItem = static_cast<TreeItem*>(parent.internalPointer());
    }

    ItemType type = parentItem->itemType();

    if (type == IT_IMAGE || type == IT_LOADING) {
        return false; // Images and the loading placeholder are leaf nodes.
    }

    // the root, years, months, days and image sources all have children always.
    return true;
}

void ImageModel::processImageLoadRequestQueue() {
    if (loadingImages || imageLoadRequestQueue.isEmpty()) {
        return;
    }
    loadingImages = true;

    ImageLoadRequest req = imageLoadRequestQueue.first();

    dataSource->fetchImageList(req.date, req.imageSourceCode);
}

void ImageModel::imageListReady(QList<ImageInfo> imageList) {

    if (imageList.count() == 0) {
        return; // No images
    }

    // Sort the image list by time and type
    qSort(imageList.begin(), imageList.end(), imageLessThan);

    ImageLoadRequest req = imageLoadRequestQueue.takeFirst();
    loadingImages = false;

    // Remove the 'Loading' item and anything else that shouldn't be there.
    beginRemoveRows(req.index, 0, req.treeItem->childCount() - 1);
    req.treeItem->deleteChildren();
    endRemoveRows();

    QList<int> thumbnailIds;

    beginInsertRows(req.index, 0, imageList.count() - 1);
    int i = 0;
    foreach (ImageInfo img, imageList) {
        QString title = img.title;
        if (title.isEmpty()) {
            title = img.timeStamp.time().toString("h:mm A");
        }
        qDebug() << "Creating node for image" << img.id;
        TreeItem *item = new TreeItem(IT_IMAGE, req.date, req.imageSourceCode,
                                      title, req.treeItem, img.id);
        req.treeItem->appendChild(item);

        ThumbnailRequest thumbReq;
        thumbReq.index = index(i, 0, req.index);
        thumbReq.treeItem = item;
        thumbReq.imageLoaded = false;
        thumbReq.thumbnailLoaded = false;

        pendingThumbnails[img.id] = thumbReq;
        thumbnailIds.append(img.id);
        i++;
    }
    endInsertRows();

    dataSource->fetchThumbnails(thumbnailIds);
    emit lazyLoadingComplete(req.index);

    if (!imageLoadRequestQueue.isEmpty()) {
        processImageLoadRequestQueue();
    }
}

void ImageModel::thumbnailReady(int imageId, QImage thumbnail) {
    if (pendingThumbnails.contains(imageId)) {
        pendingThumbnails[imageId].thumbnailLoaded = true;
        ThumbnailRequest req = pendingThumbnails[imageId];

        if (req.imageLoaded && req.thumbnailLoaded) {
            pendingThumbnails.remove(imageId);
        }

        req.treeItem->setThumbnail(thumbnail);

        emit dataChanged(req.index, req.index);
        emit thumbnailReady(req.index);
    }
}

void ImageModel::imageReady(ImageInfo info, QImage image, QString cacheFile) {

    int imageId = info.id;

    qDebug() << "Id: " << info.id << " FN: " << cacheFile;

    // Cache the image here. We can use this later for drag-drop operations, etc.
    if (pendingThumbnails.contains(imageId)) {

        pendingThumbnails[imageId].imageLoaded = true;
        ThumbnailRequest req = pendingThumbnails[imageId];

        if (req.imageLoaded && req.thumbnailLoaded) {
            pendingThumbnails.remove(imageId);
        }

        req.treeItem->setImage(info, image, cacheFile);

        emit imageReady(req.index);

        // We don't need to emit a model changed here as we're not
        // changing anything exposed to the view. Note that if the
        // raw image is ever exposed as a column or something we'll
        // need to reverse this.
    } else {
        qDebug() << "Image not in thumbnail queue: " << cacheFile;
    }
}

Qt::ItemFlags ImageModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        if (isImage(index)) {
            return Qt::ItemIsDragEnabled | defaultFlags;
        }
    }

    return defaultFlags;
}

Qt::DropActions ImageModel::supportedDragActions () const {
    return Qt::CopyAction;
}


Qt::DropActions ImageModel::supportedDropActions () const {
    return Qt::CopyAction; // We don't support drops.
}

QStringList ImageModel::mimeTypes() const {
    QStringList types;
    types << "text/uri-list";
    return types;
}

QMimeData* ImageModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = new QMimeData();

    QList<QUrl> urls;

    foreach (const QModelIndex &index, indexes) {
        if (index.isValid() &&
                (index.column() == ImageModel::COL_NAME
                 || index.column() == ImageModel::COL_NAME_THUMB)) {
            TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

            QString path = item->imageFile()->fileName();

            urls.append(QUrl::fromLocalFile(path));

        }
    }

    mimeData->setUrls(urls);

    return mimeData;
}

#ifdef QT_DEBUG
bool ImageModel::testFindIndex(QModelIndex index) {
    TreeItem *n = static_cast<TreeItem*>(index.internalPointer());
    qDebug() << n->sourceCode();

    if (isImage(index)) {
        ImageInfo info = imageInfo(index);
        QModelIndex result = findIndex(IT_IMAGE,
                                       info.timeStamp.date(),
                                       info.imageSource.code,
                                       info.timeStamp.time());



        return result == index;
    } else {
        TreeItem *treeNode = static_cast<TreeItem*>(index.internalPointer());

        QModelIndex result = findIndex(treeNode->itemType(),
                                       treeNode->date(),
                                       treeNode->sourceCode());
        return result == index;
    }
}
#endif
