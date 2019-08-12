#include "fetchimagedatelistwebtask.h"

#include "json/json.h"
#include "datasource/webcachedb.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtDebug>
#include <QMap>

#define DATASET_IMAGE_SOURCES "image_sources.json"
#define DATASET_IMAGE_SOURCE_DATES "image_sources_by_date.json"

FetchImageDateListWebTask::FetchImageDateListWebTask(QString baseUrl,
                                                     QString stationCode,
                                                     WebDataSource* ds, bool cacheResult)
    : AbstractWebTask(baseUrl, stationCode, ds)
{
    _haveStationInfo = false;
    _cacheResult = cacheResult;
}

void FetchImageDateListWebTask::beginTask() {
    // Progress: Loading image source information

    emit httpGet(QNetworkRequest(_stationBaseUrl + DATASET_IMAGE_SOURCES));
}

void FetchImageDateListWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (!_haveStationInfo) {
        processStationList(reply->readAll());
    } else {
        processDateList(reply->readAll());
    }
}

void FetchImageDateListWebTask::processStationList(QString data) {
    using namespace QtJson;

    _haveStationInfo = true;

    // Progress:
    emit subtaskChanged("Processing image source information");

    bool ok = true;

    QVariantMap result = Json::parse(data, ok).toMap();

    foreach (QString key, result.keys()) {
        qDebug() << "Date with images: " << key;
        QVariantMap imageSourceData = result[key].toMap();

        ImageSource imageSource;
        imageSource.code = key.toLower();
        imageSource.name = imageSourceData["name"].toString();
        imageSource.description = imageSourceData["description"].toString();

        _imageSources.append(imageSource);
    }

    // Progress:
    emit subtaskChanged("Downloading source dates...");

    emit httpGet(QNetworkRequest(_stationBaseUrl + DATASET_IMAGE_SOURCE_DATES));
}

void FetchImageDateListWebTask::processDateList(QString data) {
    using namespace QtJson;

    // Progress:
    emit subtaskChanged("Processing source dates...");

    QList<ImageDate> imageDates;
    QMap<QString, QList<QDate> > imageDatesBySource;

    bool ok = true;

    QVariantMap result = Json::parse(data, ok).toMap();

    foreach (QString key, result.keys()) {
        // Key is the date in ISO format. The value is a list of image source codes
        ImageDate imageDate;

        imageDate.date = QDate::fromString(key, Qt::ISODate);
        imageDate.sourceCodes = result[key].toStringList();

        // Convert source codes to lowercase & add to map
        for (int i = 0; i < imageDate.sourceCodes.count(); i++) {
            QString code = imageDate.sourceCodes[i].toLower();
            imageDate.sourceCodes[i] = code;

            if (_cacheResult) {
                if (!imageDatesBySource.contains(code)) {
                    imageDatesBySource[code] = QList<QDate>();
                }
                imageDatesBySource[code].append(imageDate.date);
            }
        }

        imageDates.append(imageDate);
    }

    emit dateListReady(imageDates, _imageSources);
    if (_cacheResult) {
        WebCacheDB::getInstance().updateImageDateList(_stationDataUrl, imageDatesBySource);
    }
    emit finished();
}
