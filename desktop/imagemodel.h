#ifndef IMAGEMODEL_H
#define IMAGEMODEL_H

#include <QAbstractItemModel>
#include <QMap>
#include <QImage>

#include "datasource/abstractdatasource.h"


class TreeItem;
class QMimeData;
typedef enum {
    IT_ROOT = 0,
    IT_YEAR = 1,
    IT_MONTH = 2,
    IT_DAY = 3,
    IT_IMAGE_SOURCE = 4,
    //IT_IMAGE_TYPE = 5,
    IT_IMAGE = 6,
    IT_LOADING = 7
} ItemType;


struct ImageLoadRequest {
    QDate date;
    QString imageSourceCode;
    TreeItem *treeItem; /*!< Item to load the images into */
    QModelIndex index;
};

struct ThumbnailRequest {
    QModelIndex index;
    TreeItem *treeItem;
    bool thumbnailLoaded;
    bool imageLoaded;
};

//#define COL_NAME    0
//#define COL_TIME    1
//#define COL_TYPE    2
//#define COL_SIZE    3
//#define COL_DESCRIPTION 4
//#define COL_MIME_TYPE   5
//#define COL_IMAGE_SOURCE    6
//#define COL_NAME_THUMB  7

class ImageModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit ImageModel(AbstractDataSource *dataSource, QObject *parent = 0);
    ~ImageModel();

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool hasChildren(const QModelIndex &parent) const;

    bool isImage(const QModelIndex &index) const;
    int imageId(const QModelIndex &index) const;
    QImage image(const QModelIndex &index) const;
    ImageInfo imageInfo(const QModelIndex &index) const;
    QString imageTemporaryFileName(const QModelIndex &index) const;
    QDate itemDate(const QModelIndex &index) const;

    // Drag-drop functionality
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDragActions () const;
    Qt::DropActions supportedDropActions () const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;

    // Pass in a "Loading..." item to cause immediate loading of that node.
    void loadItem(const QModelIndex &index) const;

    typedef enum {
        COL_NAME = 0,
        COL_TIME = 1,
        COL_TYPE = 2,
        COL_SIZE = 3,
        COL_DESCRIPTION = 4,
        COL_MIME_TYPE = 5,
        COL_IMAGE_SOURCE = 6,

        // Same as COL_NAME but uses image thumbnails instead of icons.
        COL_NAME_THUMB = 7
    } Column;

#ifdef QT_DEBUG
    bool testFindIndex(QModelIndex index);
#endif

signals:
    void lazyLoadingComplete(QModelIndex index);
    void thumbnailReady(QModelIndex index);
    void imageReady(QModelIndex index);

public slots:
    // Connect this to an AbstractLiveDataSource to auto-refresh station-day nodes when
    // new images are available.
    void newImage(NewImageInfo info);

private slots:
    void imageDatesReady(QList<ImageDate> dates,QList<ImageSource> sources);
    void processImageLoadRequestQueue();
    void imageListReady(QList<ImageInfo> imageList);
    void thumbnailReady(int imageId, QImage thumbnail);
    void imageReady(ImageInfo info, QImage image, QString cacheFile);

private:
    void resetTree();
    void buildTree(QList<ImageDate> dates, QList<ImageSource> sources);
    void updateTree(QList<ImageDate> dates, QList<ImageSource> sources);

    // Gets the index for a tree node based on its timestamp and type. Does not
    // support finding IT_LOADING nodes.
    QModelIndex findIndex(ItemType type, int year=1, int month=1, int day=1,
                          QString source=QString::null, QTime time=QTime());
    QModelIndex findIndex(ItemType type, QDate date, QString source=QString::null,
                          QTime time=QTime());
    QModelIndex findIndex(QModelIndex index, ItemType type, QDate date,
                          QString source, QTime time);

    QList<ImageLoadRequest> imageLoadRequestQueue;
    QMap<int, ThumbnailRequest> pendingThumbnails;
    bool loadingImages;

    AbstractDataSource *dataSource;

    TreeItem *rootNode;
    bool treeBuilt;
};

#endif // IMAGEMODEL_H
