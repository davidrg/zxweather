#include "latestimageswebtask.h"

#include "json/json.h"
#include "datasource/webcachedb.h"
#include "fetchimagewebtask.h"

#include <QtDebug>

#define DATASET_IMAGE_SOURCES "image_sources.json"

LatestImagesWebTask::LatestImagesWebTask(
        QString baseUrl, QString stationCode, WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{

}

void LatestImagesWebTask::beginTask() {
    QString url = _stationBaseUrl + DATASET_IMAGE_SOURCES;

    QNetworkRequest request(url);
    emit httpGet(request);
}

void LatestImagesWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        QByteArray replyData = reply->readAll();

        processResponse(replyData);

        emit finished();
    }
}

void LatestImagesWebTask::processResponse(QByteArray data) {
    using namespace QtJson;

    qDebug() << data;

    bool ok = true;

    QVariantMap result = Json::parse(data, ok).toMap();

    foreach (QString key, result.keys()) {
        qDebug() << "Image source: " << key;
        QVariantMap imageSource = result[key].toMap();
        QVariantMap latestImage = imageSource["last_image_info"].toMap();

        ImageInfo info;
        info.id = latestImage["id"].toInt(&ok);
        if (!ok) {
            qDebug() << "Invalid ID for latest image on source" << key;
            continue;
        }
        info.timeStamp = QDateTime::fromString(latestImage["timestamp"].toString(),
                Qt::ISODate);
        info.imageTypeCode = latestImage["type_code"].toString().toLower();
        info.title = latestImage["title"].toString();
        info.description = latestImage["description"].toString();
        info.mimeType = latestImage["mime_type"].toString();
        info.imageSource.code = key;
        info.imageSource.name = imageSource["name"].toString();
        info.imageSource.description = imageSource["description"].toString();

        QVariantMap urls = latestImage["urls"].toMap();

        info.fullUrl = urls["full"].toString();
        if (urls.contains("metadata")) {
            info.metaUrl = urls["metadata"].toString();
            info.hasMetadata = true;
        }

        // Store this metadata in the cache DB so other parts of the system
        // can get at it
        image_set_t imageSet;

        imageSet.filename = _stationBaseUrl +
                QString::number(info.timeStamp.date().year()) + "/" +
                QString::number(info.timeStamp.date().month()) + "/" +
                QString::number(info.timeStamp.date().day()) + "/images/" +
                info.imageSource.code.toLower() +
                "/index.json";

        imageSet.images.append(info);

        QDateTime ts = QDateTime(info.timeStamp);
        ts.setTime(QTime(0,0,0));

        // We don't really know when the dataset was last modified so we'll pick
        // an early time on the same date as its most recent image
        imageSet.last_modified = ts;
        imageSet.size = 0;
        imageSet.source.code = info.imageSource.code.toLower();
        imageSet.source.name = info.imageSource.name;
        imageSet.source.description = info.imageSource.description;
        imageSet.station_url = _stationBaseUrl;
        imageSet.is_valid = false;

        WebCacheDB::getInstance().cacheImageSet(imageSet);

        // Queue task to fetch the image
        FetchImageWebTask *task = new FetchImageWebTask(_baseUrl, _stationCode,
                                                        _dataSource, info);
        emit queueTask(task);
    }
}
