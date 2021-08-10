#include "fetchimagewebtask.h"
#include "datasource/webcachedb.h"

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QtDebug>

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QStandardPaths>
#endif

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

QString FetchImageWebTask::getCacheFilename(bool thumbnail) {
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    QString filename = QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
#else
    QString filename = QDesktopServices::storageLocation(
                QDesktopServices::CacheLocation);
#endif
    filename +=  QDir::separator() + QString("images") +  QDir::separator() +
            _stationCode + QDir::separator() +
            _imageInfo.imageSource.code.toLower() + QDir::separator() +
            _imageInfo.imageTypeCode.toLower() + QDir::separator() +
            QString::number(_imageInfo.timeStamp.date().year()) + QDir::separator() +
            QString::number(_imageInfo.timeStamp.date().month()) + QDir::separator();

    // Make sure the target directory actually exists.
    if (!QDir(filename).exists())
        QDir().mkpath(filename);

    filename += QString::number(_imageInfo.timeStamp.date().day()) + "_" +
            QString::number(_imageInfo.timeStamp.time().hour()) + "_" +
            QString::number(_imageInfo.timeStamp.time().minute()) + "_" +
            QString::number(_imageInfo.timeStamp.time().second()) +
            (thumbnail ? "_thumb." : "_full.");

    // Extension doesn't really matter too much
    if (_imageInfo.mimeType == "image/jpeg")
        filename += "jpeg";
    else if (_imageInfo.mimeType == "image/png")
        filename += "png";
    else if (_imageInfo.mimeType == "video/mp4")
        filename += "mp4";
    else if (_imageInfo.mimeType == "audio/wav")
        filename += "wav";
    else if (_imageInfo.mimeType == "audio/mpeg")
        filename += "mp3";
    else if (_imageInfo.mimeType == "audio/flac")
        filename += "flac";
    else if (_imageInfo.mimeType == "audio/ogg")
        filename += "oga";
    else
        filename += "dat";

    return QDir::cleanPath(filename);
}

void FetchImageWebTask::beginTask() {
    // Firstly, see if the image exists on disk.
    filename = getCacheFilename(false);
    qDebug() << "Cache filename:" << filename;
    QFile file(filename);

    needMetadata = !_imageInfo.hasMetadata && !_imageInfo.metaUrl.isNull()
            && _imageInfo.metadata.isNull();

    needImage = !file.exists();
    if (file.exists() && file.size() == 0) {
        file.remove();
        needImage = true;
    }

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
    if (reply->rawHeader("Content-Type").toLower() == "application/json") {
        qDebug() << "Got metadata";

        needMetadata = false;
        _imageInfo.hasMetadata = true;
        _imageInfo.metadata = reply->readAll();

        // Update the database with metadata
        WebCacheDB::getInstance().updateImageInfo(_stationBaseUrl, _imageInfo);
    } else {
        qDebug() << "Got image";

        filename = getCacheFilename(false);
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            needImage = false;
        } else {
            // Error?
            emit failed(QString(tr("Failed to open cache file %1")).arg(filename));
        }
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
    } else {
        QImage nullImage;
        // The ImageWidget will delegate display to a video widget passing in
        // the filename when it detects no image and a video/ mime type.
        _dataSource->fireImageReady(_imageInfo, nullImage, filename);
    }
}
