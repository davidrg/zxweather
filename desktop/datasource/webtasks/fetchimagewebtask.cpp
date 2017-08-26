#include "fetchimagewebtask.h"
#include "datasource/webcachedb.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QtDebug>

FetchImageWebTask::FetchImageWebTask(
        QString baseUrl, QString stationCode, WebDataSource* ds,
        ImageInfo imageInfo)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _imageInfo = imageInfo;
}

FetchImageWebTask::FetchImageWebTask(
        QString baseUrl, QString stationCode, WebDataSource* ds,
        int imageId)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _imageInfo = WebCacheDB::getInstance().getImageInfo(_stationBaseUrl,
                                                        imageId);
}

QString FetchImageWebTask::getCacheFilename() {
#if QT_VERSION >= 0x050000
    QString filename = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif

    filename += "/images/" +
            _stationCode + "/" +
            _imageInfo.imageSource.code.toLower() + "/" +
            _imageInfo.imageTypeCode.toLower() + "/" +
            QString::number(_imageInfo.timeStamp.date().year()) + "/" +
            QString::number(_imageInfo.timeStamp.date().month()) + "/";

    // Make sure the target directory actually exists.
    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += QString::number(_imageInfo.timeStamp.date().day()) + "_" +
            QString::number(_imageInfo.timeStamp.time().hour()) + "_" +
            QString::number(_imageInfo.timeStamp.time().minute()) + "_" +
            QString::number(_imageInfo.timeStamp.time().second()) + "_full.";

    // Extension doesn't really matter too much
    if (_imageInfo.mimeType == "image/jpeg")
        filename += "jpeg";
    else if (_imageInfo.mimeType == "image/png")
        filename += "png";
    else if (_imageInfo.mimeType == "video/mp4")
        filename += "mp4";
    else
        filename += "dat";

    return filename;
}

void FetchImageWebTask::beginTask() {
    // Firstly, see if the image exists on disk.
    filename = getCacheFilename();
    qDebug() << "Cache filename:" << filename;
    QFile file(filename);

    needMetadata = !_imageInfo.hasMetadata && !_imageInfo.metaUrl.isNull()
            && _imageInfo.metadata.isNull();
    needImage = !file.exists();

    if (needImage) {
        qDebug() << "Fetch image:" << _imageInfo.fullUrl;
        emit httpGet(QNetworkRequest(_imageInfo.fullUrl));
    }

    if (needMetadata) {
        qDebug() << "Fetch metadata:" << _imageInfo.metaUrl;
        emit httpGet(QNetworkRequest(_imageInfo.metaUrl));
    }

    if (!needImage && !needMetadata) {
        qDebug() << "All data is cached - nothing to do";
        dealWithImage(filename);
        emit finished();
    }
}


void FetchImageWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->request().url() == _imageInfo.fullUrl) {
        qDebug() << "Got image";

        filename = getCacheFilename();
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            needImage = false;
        } else {
            // Error?
            emit failed("Failed to open cache file " + filename);
        }
    } else if (reply->request().url() == _imageInfo.metaUrl) {
        qDebug() << "Got metadata";

        needMetadata = false;
        _imageInfo.hasMetadata = true;
        _imageInfo.metadata = reply->readAll();

        // Update the database with metadata
        WebCacheDB::getInstance().updateImageInfo(_stationBaseUrl, _imageInfo);
    }

    if (!needMetadata && !needImage) {
        dealWithImage(filename);
        emit finished();
    }
}

void FetchImageWebTask::dealWithImage(QString filename) {
    qDebug() << "Dealing with image: " << filename;
    if (_imageInfo.mimeType.startsWith("image/")) {
        QImage image(filename);
        _dataSource->fireImageReady(_imageInfo, image, filename);
    } else if (_imageInfo.mimeType.startsWith("video/")) {
        QImage nullImage;
        // The ImageWidget will delegate display to a video widget passing in
        // the filename when it detects no image and a video/ mime type.
        _dataSource->fireImageReady(_imageInfo, nullImage, filename);
    }
}
