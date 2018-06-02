#include "fetchstationinfo.h"

#include "datasource/webcachedb.h"

#include "json/json.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <cfloat>

#define DATASET_SYSCONFIG "sysconfig.json"


FetchStationInfoWebTask::FetchStationInfoWebTask(QString baseUrl, QString stationCode, WebDataSource *ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {

}


void FetchStationInfoWebTask::beginTask() {
    QString url = _dataRootUrl + DATASET_SYSCONFIG;

    QNetworkRequest request(url);

    emit httpGet(request);
}

void FetchStationInfoWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
    } else {
        processResponse(reply->readAll());
        emit finished();
    }
}

bool FetchStationInfoWebTask::processResponse(QByteArray responseData) {
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(responseData, ok).toMap();

    if (!ok) {
        qDebug() << "sysconfig parse error. Data:" << responseData;
        emit failed("JSON parsing failed while loading system configuration.");
        return false;
    }

    QVariantList stations = result["stations"].toList();
    foreach (QVariant station, stations) {
        QVariantMap stationData = station.toMap();

        if (stationData["code"].toString().toLower() == _stationCode) {
            QString stationName = stationData["name"].toString();
            bool isSolarAvailable = false;
            bool isWireless = false;

            QString hw = stationData["hw_type"].toMap()["code"].toString().toUpper();

            int davis_broadcast_id = -1;

            if (hw == "DAVIS") {
                isSolarAvailable = stationData["hw_config"]
                        .toMap()["has_solar_and_uv"].toBool();
                isWireless = stationData["hw_config"]
                        .toMap()["is_wireless"].toBool();

                if (isWireless) {
                    bool ok;
                    davis_broadcast_id = stationData["hw_config"].toMap()["broadcast_id"].toInt(&ok);
                    if (!ok) {
                        davis_broadcast_id = -1;
                    }
                }
            }

            float latitude = FLT_MAX, longitude = FLT_MAX;
            QVariantMap coordinates = stationData["coordinates"].toMap();
            if (!coordinates["latitude"].isNull()) {
                latitude = coordinates["latitude"].toFloat();
            }
            if (!coordinates["longitude"].isNull()) {
                longitude = coordinates["longitude"].toFloat();
            }

            int sample_interval = 5;
            if (stationData.contains("interval")) {
                sample_interval = stationData["interval"].toInt() / 60;
            }

            _dataSource->updateStation(
                    stationName,
                    stationData["desc"].toString(),
                    stationData["hw_type"].toMap()["code"].toString().toLower(),
                    sample_interval,
                    latitude,
                    longitude,
                    stationData["coordinates"].toMap()["altitude"].toFloat(),
                    isSolarAvailable,
                    davis_broadcast_id
            );

            return true;
        }
    }

    return false;
}
