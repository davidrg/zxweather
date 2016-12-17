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
    else if (_imageInfo.mimeType == "video/mp4")
        filename += "mp4";
    else
        filename += "dat";

    return filename;
}

void FetchImageWebTask::beginTask() {
    // Firstly, see if the image exists on disk.
    QString filename = getCacheFilename();
    QFile file(filename);

    if (file.exists()) {
        dealWithImage(filename);
        emit finished();
    } else {
        // File isn't cached. Ask the internet for a copy.
        emit httpGet(QNetworkRequest(_imageInfo.fullUrl));
    }
}


void FetchImageWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();
    QString filename = getCacheFilename();
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reply->readAll());
        file.close();
        dealWithImage(filename);
        emit finished();
        return;
    }

    // Error?
    emit failed("Failed to open cache file " + filename);
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
