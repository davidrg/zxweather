#include "fetchthumbnailwebtask.h"

#include <QDebug>

FetchThumbnailWebTask::FetchThumbnailWebTask(QString baseUrl, QString stationCode,
                           WebDataSource* ds, ImageInfo imageInfo)
    : FetchImageWebTask(baseUrl, stationCode, ds, imageInfo)
{

}

FetchThumbnailWebTask::FetchThumbnailWebTask(QString baseUrl, QString stationCode,
                           WebDataSource* ds, int imageId)
    : FetchImageWebTask(baseUrl, stationCode, ds, imageId)
{

}

void FetchThumbnailWebTask::dealWithImage(QString filename) {
    qDebug() << "Dealing with image: " << filename;
    if (_imageInfo.mimeType.startsWith("image/")) {
        QImage image(filename);

        QImage thumbnailImage = image.scaled(THUMBNAIL_WIDTH,
                                             THUMBNAIL_HEIGHT,
                                             Qt::KeepAspectRatio);

        _dataSource->fireThumbnailReady(_imageInfo.id, thumbnailImage);
        _dataSource->fireImageReady(_imageInfo, image, filename);
    } else if (_imageInfo.mimeType.startsWith("video/")) {
        // TODO: ??
        // We can't exactly thumbnail videos. Do we pull out the first frame
        // and thumbnail that?
    }
}


