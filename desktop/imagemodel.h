#ifndef IMAGEMODEL_H
#define IMAGEMODEL_H

#include <QAbstractItemModel>
#include <QMap>
#include <QImage>

#include "datasource/abstractdatasource.h"


class TreeItem;
class QMimeData;

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

class ImageModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit ImageModel(AbstractDataSource *dataSource, QObject *parent = 0);
    ~ImageModel();

//    QVariant headerData(int section, Qt::Orientation orientation,
//                        int role = Qt::DisplayRole) const;
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

    // Drag-drop functionality
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDragActions () const;
    Qt::DropActions supportedDropActions () const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;

    // Pass in a "Loading..." item to cause immediate loading of that node.
    void loadItem(const QModelIndex &index) const;

signals:

public slots:

private slots:
    void imageDatesReady(QList<ImageDate> dates,QList<ImageSource> sources);
    void processImageLoadRequestQueue();
    void imageListReady(QList<ImageInfo> imageList);
    void thumbnailReady(int imageId, QImage thumbnail);
    void imageReady(ImageInfo info, QImage image, QString cacheFile);

private:
    void resetTree();

    QList<ImageLoadRequest> imageLoadRequestQueue;
    QMap<int, ThumbnailRequest> pendingThumbnails;
    bool loadingImages;

    AbstractDataSource *dataSource;

    TreeItem *rootNode;
};

#endif // IMAGEMODEL_H
