#include "fetchimagedatelistwebtask.h"

#include "json/json.h"
#include "datasource/webcachedb.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtDebug>
#include <QMap>

#define DATASET_IMAGE_SOURCE_COUNTS "image_source_date_counts.json"

FetchImageDateListWebTask::FetchImageDateListWebTask(QString baseUrl,
                                                     QString stationCode,
                                                     WebDataSource* ds)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
}

void FetchImageDateListWebTask::beginTask() {
    // Progress: Loading image source information
    emit subtaskChanged(tr("Downloading source dates..."));
    emit httpGet(QNetworkRequest(_stationBaseUrl + DATASET_IMAGE_SOURCE_COUNTS));
}

void FetchImageDateListWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    processDateList(reply->readAll());
}

QList<ImageSource> FetchImageDateListWebTask::processSourceInfo(QVariantMap sources) {
    using namespace QtJson;

    // Progress:
    emit subtaskChanged(tr("Processing image source information"));

    QList<ImageSource> result;

    foreach (QString key, sources.keys()) {
        qDebug() << "Date with images: " << key;
        QVariantMap imageSourceData = sources[key].toMap();

        ImageSource imageSource;
        imageSource.code = key.toLower();
        imageSource.name = imageSourceData["name"].toString();
        imageSource.description = imageSourceData["description"].toString();

        result.append(imageSource);
    }

    return result;
}

void FetchImageDateListWebTask::processDateList(QString data) {
    using namespace QtJson;

    // Progress:
    QList<ImageDate> imageDates;
    QMap<QString, QMap<QDate, int> > imageDatesBySource;

    bool ok = true;

    QVariantMap result = Json::parse(data, ok).toMap();

    if (!ok) {
        qWarning() << "Failed to parse json document!";
        qDebug() << data;
    }

    QVariantMap dateCounts = result["date_counts"].toMap();
    QVariantMap sources = result["sources"].toMap();

    QList<ImageSource> imageSources = processSourceInfo(sources);

    emit subtaskChanged(tr("Processing source dates..."));

    foreach (QString key, dateCounts.keys()) {
        // Key is the date in ISO format. The value is a map of image source to image count

        QVariantMap sourceCounts = dateCounts[key].toMap();

        ImageDate imageDate;

        imageDate.date = QDate::fromString(key, Qt::ISODate);
        imageDate.sourceCodes = sourceCounts.keys();

        // Convert source codes to lowercase & add to map
        for (int i = 0; i < imageDate.sourceCodes.count(); i++) {
            QString code = imageDate.sourceCodes[i];
            QVariant count = sourceCounts[code];
            code = code.toLower();
            imageDate.sourceCodes[i] = code;

            if (!imageDatesBySource.contains(code)) {
                imageDatesBySource[code] = QMap<QDate,int>();
            }

            if (count.isNull()) {
                imageDatesBySource[code][imageDate.date] = -1;
            } else {
                imageDatesBySource[code][imageDate.date] = count.toInt();
            }
        }

        imageDates.append(imageDate);
    }

    emit dateListReady(imageDates, imageSources);

    WebCacheDB::getInstance().updateImageDateList(_stationDataUrl, imageDatesBySource);

    emit finished();
}
