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

#include "settings.h"

typedef enum {
    IT_ROOT,
    IT_YEAR,
    IT_MONTH,
    IT_DAY,
    IT_IMAGE_SOURCE,
    //IT_IMAGE_TYPE,
    IT_IMAGE,
    IT_LOADING
} ItemType;

class TreeItem{
public:
    TreeItem(ItemType type, QDate date, QString sourceCode, QString text = "",
             TreeItem *parent = 0, int imageId = -1);
    ~TreeItem();
    ItemType itemType() const;
    QString text() const;
    void appendChild(TreeItem* child);
    TreeItem* parent() const;
    TreeItem* child(int row);
    int childCount() const;
    int row() const;
    bool childrenLoaded() const;
    QIcon icon() const;
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
    QImage thumbnail;
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
    this->imageId = imageId;
    temporaryImageFile = NULL;

    if (type == IT_DAY || type == IT_IMAGE_SOURCE) {
        // Add a place-holder "Loading..." item. This will be removed
        // automatically when the first child is added.
        this->childItems.append(new TreeItem(IT_LOADING,
                                             date,
                                             sourceCode,
                                             "Loading...",
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
    thumbnail = thumbnailImage;
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
        } else {
            icon.addFile(":/icons/image", QSize(16,16));
            icon.addFile(":/icons/image-32", QSize(32, 32));
        }

        if (!thumbnail.isNull()) {
            icon.addPixmap(QPixmap::fromImage(thumbnail));
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
                    QStandardPaths::CacheLocation);
#else
        QString filename = QDesktopServices::storageLocation(
                    QDesktopServices::TempLocation);
#endif

        filename += "/zxweather/";

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
    this->dataSource = dataSource;

    // Create the root item
    rootNode = 0;
    resetTree();
    loadingImages = false;

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
                                          QDate(),
                                          "",
                                          QString::number(year),
                                          rootNode,
                                          -1);
        rootNode->appendChild(yearNode);
        foreach (int month, sourceTree[year].keys()) {
            QString monthName = QDate::longMonthName(month);
            TreeItem *monthNode = new TreeItem(IT_MONTH,
                                               QDate(),
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
    endResetModel();
}

ImageModel::~ImageModel() {
    if (rootNode) {
        delete rootNode;
    }
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
    case Qt::DisplayRole:
        return item->text();
    case Qt::DecorationRole:
        return item->icon();
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

//QVariant ImageModel::headerData(int section, Qt::Orientation orientation,
//                                int role) const {

//    /* If we supported multiple columns we'd store the column names in the
//     * root node and do something like this:
//     * if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
//     *     return rootNode->data(section);
//     * }
//     *
//     * For now we don't want headers at all. So...
//     */

//    return tr("Time");

//    //return QVariant();
//}


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
    if (!index.isValid() || index.column() > 0) {
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
    return 1; // Only one column: timestamp

    // If we were to support more than one column:
//    if (parent.isValid()) {
//        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
//    } else {
//        return rootNode->columnCount();
//    }
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

    if (!imageLoadRequestQueue.isEmpty()) {
        processImageLoadRequestQueue();
    }
}

void ImageModel::thumbnailReady(int imageId, QImage thumbnail) {
    if (pendingThumbnails.contains(imageId)) {
        emit layoutAboutToBeChanged();

        pendingThumbnails[imageId].thumbnailLoaded = true;
        ThumbnailRequest req = pendingThumbnails[imageId];

        if (req.imageLoaded && req.thumbnailLoaded) {
            pendingThumbnails.remove(imageId);
        }

        req.treeItem->setThumbnail(thumbnail);

        emit dataChanged(req.index, req.index);

        emit layoutChanged();
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
        if (index.isValid()) {
            TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

            QString path = item->imageFile()->fileName();

            urls.append(QUrl::fromLocalFile(path));

        }
    }

    mimeData->setUrls(urls);

    return mimeData;
}
