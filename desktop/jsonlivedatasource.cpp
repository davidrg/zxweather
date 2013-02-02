/*****************************************************************************
 *            Created: 23/09/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

#include "jsonlivedatasource.h"

#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QVariantMap>

#include "json/json.h"

JsonLiveDataSource::JsonLiveDataSource(QString url, QObject *parent) :
    AbstractLiveDataSource(parent)
{
    if (!url.endsWith("/"))
        url += "/";
    this->url = url + "live.json";

    notificationTimer.reset(new QTimer());
    notificationTimer->setInterval(48000);
    connect(notificationTimer.data(), SIGNAL(timeout()),
            this, SLOT(liveDataPoll()));

    netAccessManager.reset(new QNetworkAccessManager(this));
    connect(netAccessManager.data(), SIGNAL(finished(QNetworkReply*)),
            this, SLOT(dataReady(QNetworkReply*)));

    // Get some initial data
    liveDataPoll();

    // Then start polling
    notificationTimer->start();
}

AbstractLiveData* JsonLiveDataSource::getLiveData() {
    using namespace QtJson;
    bool ok;

    QVariantMap result = Json::parse(json_data, ok).toMap();

    if (!ok) {
        emit networkError("JSON parsing failed");
    }

    //if (result["age"].toFloat() > 300) return null;

    LiveData *ld = new LiveData();
    ld->setWindDirection(result["wind_direction"].toString());
    ld->setAverageWindSpeed(result["average_wind_speed"].toFloat());
    ld->setTemperature(result["temperature"].toFloat());
    ld->setDewPoint(result["dew_point"].toFloat());
    ld->setWindChill(result["wind_chill"].toFloat());
    ld->setGustWindSpeed(result["gust_wind_speed"].toFloat());
    ld->setRelativeHumidity(result["relative_humidity"].toInt());
    ld->setTimestamp(QDateTime::fromString(
                         result["time_stamp"].toString(), "HH:mm:ss"));
    ld->setApparentTemperature(result["apparent_temperature"].toFloat());
    ld->setAbsolutePressure(result["absolute_pressure"].toFloat());

    // Indoor data is not currently available from the website data feed.
    ld->setIndoorDataAvailable(false);

    return ld;
}

void JsonLiveDataSource::liveDataPoll() {
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("User-Agent", "zxweather-desktop v0.2");

    netAccessManager->get(request);
}

bool JsonLiveDataSource::CheckDataAge() {
    using namespace QtJson;
    bool ok;
    QVariantMap result = Json::parse(json_data, ok).toMap();
    if (!ok) {
        emit networkError("JSON parsing failed");
    }

    if (result["age"].toFloat() > 300) return false;
    return true;
}

void JsonLiveDataSource::dataReady(QNetworkReply* reply) {

    if (reply->error() != QNetworkReply::NoError) {
        // There was an error.
        emit networkError(reply->errorString());
    } else {
        json_data = reply->readAll();
        if (CheckDataAge())
            emit liveDataRefreshed();
    }
}
