#include "fetchstationinfo.h"

#include "datasource/webcachedb.h"

#include "json/json.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <cfloat>
#include <QMap>

#define DATASET_SYSCONFIG "sysconfig.json"


FetchStationInfoWebTask::FetchStationInfoWebTask(QString baseUrl, QString stationCode, WebDataSource *ds)
    : AbstractWebTask(baseUrl, stationCode, ds) {
    _have_sysconfig = false;
    _have_gap_data = false;
    _sysconfig_url = _dataRootUrl + DATASET_SYSCONFIG;
    _gaps_url = _stationDataUrl + "gaps.json";
}


void FetchStationInfoWebTask::beginTask() {
    QNetworkRequest sysConfigRequest(_sysconfig_url);
    emit httpGet(sysConfigRequest);

    // We could check API level >= 20220210 to be sure gaps.json is available
    // but its faster just to do it in parallel and let a 404 tell is the Web UI
    // is too old.
    QNetworkRequest gapsRequest(_gaps_url);
    emit httpGet(gapsRequest);
}

void FetchStationInfoWebTask::networkReplyReceived(QNetworkReply *reply) {
    reply->deleteLater();

    bool isSysConfigResponse = reply->request().url() == _sysconfig_url;

    if (reply->error() != QNetworkReply::NoError) {
        if (isSysConfigResponse) {
            emit failed(reply->errorString());
        } else {
            // The zxweather web UI didn't support marked gaps until august 2021
            // so if we get an error fetching the gaps file its probably just
            // because the server hasn't been upgraded. And even if the Web UI
            // does support supplying gap data and we really did fail to fetch it
            // for some reason its not a big deal - it only impacts how well we can
            // cache data files with known gaps. Not something worth failing the
            // entire process over.
            _have_gap_data = true;
        }
    } else {
        if (isSysConfigResponse) {
            processSysConfigResponse(reply->readAll());
            _have_sysconfig = true;
        } else { // Must be gap data
            processGapDataResponse(reply->readAll());
            _have_gap_data = true;
        }

        if (_have_gap_data && _have_sysconfig) {
            emit finished();
        }
    }
}

bool FetchStationInfoWebTask::processGapDataResponse(QByteArray responseData) {
    using namespace QtJson;

    bool ok;

    QVariantList result = Json::parse(responseData, ok).toList();

    if (!ok) {
        qDebug() << "gaps.json parse error. Data:" << responseData;
        return false;
    }

    qDebug() << "Parsing GAPS data";

    QList<sample_gap_t> gaps;

    foreach(QVariant gapv, result) {
        QVariantMap gap = gapv.toMap();
        sample_gap_t sample_gap;
        sample_gap.start_time = gap["start_time"].toDateTime();
        sample_gap.end_time = gap["end_time"].toDateTime();
        sample_gap.missing_samples = gap["missing_sample_count"].toInt();
        sample_gap.label = gap["label"].toString();
        gaps.append(sample_gap);
    }

    WebCacheDB::getInstance().updateStationGaps(_stationDataUrl, gaps);
    return true;
}

bool FetchStationInfoWebTask::processSysConfigResponse(QByteArray responseData) {
    using namespace QtJson;

    bool ok;

    QVariantMap result = Json::parse(responseData, ok).toMap();

    if (!ok) {
        qDebug() << "sysconfig parse error. Data:" << responseData;
        emit failed(tr("JSON parsing failed while loading system configuration."));
        return false;
    }

    qDebug() << "Parsing SYSCONFIG data";

    _api_level = 0;
    if (result.contains("v")) {
        _api_level = result["v"].toUInt();
    }

    QVariantList stations = result["stations"].toList();
    foreach (QVariant station, stations) {
        QVariantMap stationData = station.toMap();

        qDebug() << "SYSCONFIG: Station:" << stationData["code"].toString();

        if (stationData["code"].toString().toLower() == _stationCode) {
            QString stationName = stationData["name"].toString();
            bool isSolarAvailable = false;
            bool isWireless = false;
            bool isArchived = false;
            QDateTime archivedTime;
            QString archivedMessage;

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

            QMap<ExtraColumn, QString> extraColumnNames;
            ExtraColumns extraColumns;

            FetchStationInfoWebTask::parseSensorConfig(
                        stationData, &extraColumnNames, &extraColumns);

            if (stationData.contains("is_archived")) {
                isArchived = stationData["is_archived"].toBool();

                QVariantMap archiveInfo = stationData["archived"].toMap();

                archivedTime = archiveInfo["time"].toDateTime();
                archivedMessage = archiveInfo["message"].toString();
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
                    davis_broadcast_id,
                    extraColumns,
                    extraColumnNames,
                    isArchived,
                    archivedTime,
                    archivedMessage,
                    _api_level
            );

            return true;
        }
    }

    return false;
}

void FetchStationInfoWebTask::parseSensorConfig(
        QVariantMap stationData,
        QMap<ExtraColumn, QString> *columnNames,
        ExtraColumns* enabledColumns) {

    QMap<ExtraColumn, QString> extraColumnNames;
    ExtraColumns extraColumns = EC_NoColumns;

    QVariantMap hw_config = stationData["hw_config"].toMap();

    qDebug() << "HWConfig keys:" << hw_config.keys();

    if (hw_config.contains("sensor_config")) {
        QVariantMap sensorConfig = hw_config["sensor_config"].toMap();
        qDebug() << "sensor_config keys:" << sensorConfig.keys();

        if (sensorConfig.contains("leaf_wetness_1")) {
            QVariantMap sensor = sensorConfig["leaf_wetness_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_LeafWetness1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_LeafWetness1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_LeafWetness1] = tr("Leaf Wetness 1");
                }
            }
        }

        if (sensorConfig.contains("leaf_wetness_2")) {
            QVariantMap sensor = sensorConfig["leaf_wetness_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_LeafWetness2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_LeafWetness2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_LeafWetness2] = tr("Leaf Wetness 2");
                }
            }
        }

        if (sensorConfig.contains("leaf_temperature_1")) {
            QVariantMap sensor = sensorConfig["leaf_temperature_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_LeafTemperature1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_LeafTemperature1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_LeafTemperature1] = tr("Leaf Temperature 1");
                }
            }
        }

        if (sensorConfig.contains("leaf_temperature_2")) {
            QVariantMap sensor = sensorConfig["leaf_temperature_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_LeafTemperature2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_LeafTemperature2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_LeafTemperature2] = tr("Leaf Temperature 2");
                }
            }
        }

        if (sensorConfig.contains("soil_moisture_1")) {
            QVariantMap sensor = sensorConfig["soil_moisture_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilMoisture1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilMoisture1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilMoisture1] = tr("Soil Moisture 1");
                }
            }
        }

        if (sensorConfig.contains("soil_moisture_2")) {
            QVariantMap sensor = sensorConfig["soil_moisture_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilMoisture2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilMoisture2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilMoisture2] = tr("Soil Moisture 2");
                }
            }
        }

        if (sensorConfig.contains("soil_moisture_3")) {
            QVariantMap sensor = sensorConfig["soil_moisture_3"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilMoisture3;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilMoisture3] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilMoisture3] = tr("Soil Moisture 3");
                }
            }
        }

        if (sensorConfig.contains("soil_moisture_4")) {
            QVariantMap sensor = sensorConfig["soil_moisture_4"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilMoisture4;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilMoisture4] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilMoisture4] = tr("Soil Moisture 4");
                }
            }
        }

        if (sensorConfig.contains("soil_temperature_1")) {
            QVariantMap sensor = sensorConfig["soil_temperature_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilTemperature1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilTemperature1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilTemperature1] = tr("Soil Temperature 1");
                }
            }
        }

        if (sensorConfig.contains("soil_temperature_2")) {
            QVariantMap sensor = sensorConfig["soil_temperature_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilTemperature2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilTemperature2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilTemperature2] = tr("Soil Temperature 2");
                }
            }
        }

        if (sensorConfig.contains("soil_temperature_3")) {
            QVariantMap sensor = sensorConfig["soil_temperature_3"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilTemperature3;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilTemperature3] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilTemperature3] = tr("Soil Temperature 3");
                }
            }
        }

        if (sensorConfig.contains("soil_temperature_4")) {
            QVariantMap sensor = sensorConfig["soil_temperature_4"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_SoilTemperature4;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_SoilTemperature4] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_SoilTemperature4] = tr("Soil Temperature 4");
                }
            }
        }

        if (sensorConfig.contains("extra_temperature_1")) {
            QVariantMap sensor = sensorConfig["extra_temperature_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_ExtraTemperature1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_ExtraTemperature1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_ExtraTemperature1] = tr("Extra Temperature 1");
                }
            }
        }

        if (sensorConfig.contains("extra_temperature_2")) {
            QVariantMap sensor = sensorConfig["extra_temperature_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_ExtraTemperature2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_ExtraTemperature2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_ExtraTemperature2] = tr("Extra Temperature 2");
                }
            }
        }

        if (sensorConfig.contains("extra_temperature_3")) {
            QVariantMap sensor = sensorConfig["extra_temperature_3"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_ExtraTemperature3;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_ExtraTemperature3] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_ExtraTemperature3] = tr("Extra Temperature 3");
                }
            }
        }

        if (sensorConfig.contains("extra_humidity_1")) {
            QVariantMap sensor = sensorConfig["extra_humidity_1"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_ExtraHumidity1;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_ExtraHumidity1] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_ExtraHumidity1] = tr("Extra Humidity 1");
                }
            }
        }

        if (sensorConfig.contains("extra_humidity_2")) {
            QVariantMap sensor = sensorConfig["extra_humidity_2"].toMap();
            if (sensor.contains("enabled") && sensor["enabled"].toBool()) {
                extraColumns |= EC_ExtraHumidity2;
                if (sensor.contains("name")) {
                    extraColumnNames[EC_ExtraHumidity2] = sensor["name"].toString();
                } else {
                    extraColumnNames[EC_ExtraHumidity2] = tr("Extra Humidity 2");
                }
            }
        }
    }

    *enabledColumns = extraColumns;
    *columnNames = extraColumnNames;
}
