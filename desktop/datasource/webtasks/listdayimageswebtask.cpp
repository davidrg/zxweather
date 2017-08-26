#include "listdayimageswebtask.h"

#include "datasource/webcachedb.h"

#include "json/json.h"

#include <QtDebug>
#include <QNetworkRequest>

#define DATASET_IMAGE_SOURCES "image_sources.json"

ListDayImagesWebTask::ListDayImagesWebTask(QString baseUrl, QString stationCode,
                                           WebDataSource* ds, QDate date,
                                           QString imageSourceCode)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _date = date;
    _imageSourceCode = imageSourceCode.toLower();
    _gotImageSourceInfo = false; // First we must get the image source info
    _gotCacheStatus = false; // Then we check the cache

    _imagesRoot = _stationBaseUrl +
                QString::number(date.year()) + "/" +
                QString::number(date.month()) + "/" +
                QString::number(date.day()) + "/images/" +
                imageSourceCode.toLower() + "/";
    _url = _imagesRoot + "index.json";
}

void ListDayImagesWebTask::beginTask() {

    imageSource = WebCacheDB::getInstance().getImageSource(
                _stationBaseUrl, _imageSourceCode);

    if (imageSource.code.isNull()) {
        // 1: Get image source details
        emit httpGet(QNetworkRequest(_stationBaseUrl + DATASET_IMAGE_SOURCES));
    } else {
        // Skip 1 & 2, move onto 3: Get image list cache status
        _gotImageSourceInfo = true;
        emit subtaskChanged("Checking image list cache status for " +
                            _date.toString(Qt::SystemLocaleShortDate));
        emit httpHead(QNetworkRequest(_url));
    }
}

void ListDayImagesWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        if (!_gotImageSourceInfo) {
            _gotImageSourceInfo = true;

            // 2: Proces image source details
            imageSourceInfoRequestFinished(reply);
        }
        else if (!_gotCacheStatus) {
            _gotCacheStatus = true;
            // 4: Process image list cache status
            cacheStatusRequestFinished(reply);
        } else {
            // 6: Process and cache image list
            downloadRequestFinished(reply);
        }
    }
}

// 2: Proces image source details
void ListDayImagesWebTask::imageSourceInfoRequestFinished(QNetworkReply *reply) {
    using namespace QtJson;

    bool ok = true;

    QVariantMap result = Json::parse(reply->readAll(), ok).toMap();

    imageSource.code = _imageSourceCode;

    if (result.contains(_imageSourceCode)) {
        QVariantMap src = result[_imageSourceCode].toMap();
        imageSource.name = src["name"].toString();
        imageSource.description = src["description"].toString();
    } else {
        qDebug() << "Image source " << _imageSourceCode << "not found!";
        emit failed("Image source not found: " + _imageSourceCode);
        return;
    }

    // 3: Get image list cache status
    emit subtaskChanged("Checking image list cache status for " +
                        _date.toString(Qt::SystemLocaleShortDate));
    emit httpHead(QNetworkRequest(_url));
}

void ListDayImagesWebTask::getDataset() {
    emit subtaskChanged("Downloading image list for " +
                        _date.toString(Qt::SystemLocaleShortDate));

    // 5: Download image list
    emit httpGet(QNetworkRequest(_url));
}

// 4: Process image list cache status
void ListDayImagesWebTask::cacheStatusRequestFinished(QNetworkReply *reply) {
    image_set_t cache_info =
            WebCacheDB::getInstance().getImageSetCacheInformation(_url);
    qDebug() << "Cache status request for url [" << _url << "] finished.";

    if (reply->hasRawHeader("X-Cache-Lookup")) {
        QString upstreamStatus = QString(reply->rawHeader("X-Cache-Lookup"));
        qDebug() << "Upstream cache status: " << upstreamStatus;
        // Squid inserts headers containing strings such as:
        // HIT from gatekeeper.zx.net.nz:3128
    }

    QDateTime lastModified = reply->header(
                QNetworkRequest::LastModifiedHeader).toDateTime();
    qDebug() << "File on server was last modified" << lastModified;

    if (!lastModified.isValid() || !cache_info.is_valid
            || lastModified != cache_info.last_modified) {
        // Last modified date has changed. We need to investigate
        // further. I used to check content-length here too but something kept
        // resetting it to zero on HEAD requests (likely just when using gzip)
        // so it doesn't seem a reliable option.
        qDebug() << "Last modified date changed (database is"
                 << cache_info.last_modified << "). Full download required.";

        // Fire of a GET to GET the full dataset. Which we'll then process and
        // cache.
        getDataset();
    } else {
        // else the data file we have cached sounds the same as what is on the
        // server. We won't bother redownloading it.

        // Skip to 7: Return list
        returnImageListAndFinish();
    }
}

// 6: Process and cache image list
void ListDayImagesWebTask::downloadRequestFinished(QNetworkReply *reply) {
    using namespace QtJson;

    QByteArray data = reply->readAll();

    qDebug() << "Download completed for" << _date << "[" << _url << "]";
    qDebug() << data;

    emit subtaskChanged("Processing image list for " +
                        _date.toString(Qt::SystemLocaleShortDate));

    QDateTime lastModified = reply->header(
                QNetworkRequest::LastModifiedHeader).toDateTime();
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();

    image_set_t imageSet;
    imageSet.filename = _url;
    imageSet.size = size;
    imageSet.last_modified = lastModified;
    imageSet.station_url = _stationBaseUrl;
    imageSet.is_valid = true;
    imageSet.source = imageSource;

    QVector<ImageInfo> images;

    bool ok;



    QVariantList result = Json::parse(data, ok).toList();

    foreach (QVariant item, result) {
        QVariantMap imageData = item.toMap();

        ImageInfo image;
        image.id = imageData["id"].toInt();
        image.timeStamp = imageData["time_stamp"].toDateTime();
        image.imageTypeCode = imageData["type_code"].toString().toLower();
        image.imageTypeName = imageData["type_name"].toString();
        image.title = imageData["title"].toString();
        image.description = imageData["description"].toString();
        image.mimeType = imageData["mime_type"].toString();
        image.imageSource = imageSource;
        image.fullUrl = _imagesRoot + imageData["image_url"].toString();
        image.hasMetadata = imageData["has_metadata"].toBool();

        if (image.hasMetadata) {
            image.metaUrl = _imagesRoot + imageData["metadata_url"].toString();
        }

        qDebug() << "Image: " << image.fullUrl;

        images.append(image);
    }

    imageSet.images = images;

    emit subtaskChanged("Caching data for " +
                        _date.toString(Qt::SystemLocaleShortDate));
    WebCacheDB::getInstance().cacheImageSet(imageSet);

    // TODO: return the non-cached version seeing as we've already got it
    // lying around.

    // 7: Return list
    returnImageListAndFinish();
}

// 7: Return List
void ListDayImagesWebTask::returnImageListAndFinish() {
    emit subtaskChanged("Building list for " +
                        _date.toString(Qt::SystemLocaleShortDate));

    QVector<ImageInfo> result = WebCacheDB::getInstance()
            .getImagesForDate(_date, _stationBaseUrl, _imageSourceCode);

    QList<ImageInfo> imageList = result.toList();

    qDebug() << "Images from cache:";
    foreach (ImageInfo info, imageList) {
        qDebug() << "Image: " << info.fullUrl;
    }

    emit imageListReady(imageList);

    emit finished();
}
