#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <QVector>

#include "json/json.h"
#include "activeimagesourceswebtask.h"
#include "datasource/webcachedb.h"

#define DATASET_IMAGE_SOURCES "image_sources.json"

ActiveImageSourcesWebTask::ActiveImageSourcesWebTask(
        QString baseUrl, QString stationCode, WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{

}

void ActiveImageSourcesWebTask::beginTask() {
    QString url = _stationBaseUrl + DATASET_IMAGE_SOURCES;

    QNetworkRequest request(url);
    emit httpGet(request);
}

void ActiveImageSourcesWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        QByteArray replyData = reply->readAll();

        processResponse(replyData);

        emit finished();
    }
}

void ActiveImageSourcesWebTask::processResponse(QByteArray data) {
    using namespace QtJson;

    qDebug() << data;

    bool ok;

    QVariantMap result = Json::parse(data, ok).toMap();

    bool activeSources = false;
    bool anySources = false;

    WebCacheDB &db = WebCacheDB::getInstance();

    foreach (QString key, result.keys()) {
        qDebug() << "Image source: " << key;
        anySources = true;
        QVariantMap imageSource = result[key].toMap();

        if (imageSource["is_active"].toBool()) {
            activeSources = true;
        }

        // Cache the metadata for the most recent image from this source.
        // This will be ignored if its already been cached via some other means
        QVariantMap latestImage = imageSource["latest_image_info"].toMap();
        ImageInfo info;
        info.id = latestImage["id"].toInt();
        info.timeStamp = latestImage["timestamp"].toDateTime();
        info.imageTypeCode = latestImage["type_code"].toString().toLower();
        info.title = latestImage["title"].toString();
        info.description = latestImage["description"].toString();
        info.mimeType = latestImage["mime_type"].toString();
        info.imageSource.code = key.toLower();
        info.imageSource.name = imageSource["name"].toString();
        info.imageSource.description = imageSource["description"].toString();
        info.fullUrl = latestImage["urls"].toMap()["full"].toString();
        db.storeImageInfo(_stationBaseUrl, info);
    }

    if (anySources) {
        qDebug() << "Active image sources task finds archived image sources";
        emit archivedImagesAvailable();
    }

    if (activeSources) {
        qDebug() << "Active image sources task finds active image sources";
        emit activeImageSourcesAvailable();
    }
}
