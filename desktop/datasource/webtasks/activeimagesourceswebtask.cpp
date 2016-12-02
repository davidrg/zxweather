#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

#include "json/json.h"
#include "activeimagesourceswebtask.h"

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

    foreach (QString key, result.keys()) {
        qDebug() << "Image source: " << key;
        anySources = true;
        QVariantMap imageSource = result[key].toMap();

        if (imageSource["is_active"].toBool()) {
            activeSources = true;
            break;
        }
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
