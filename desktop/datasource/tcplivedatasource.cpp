#include "tcplivedatasource.h"
#include "settings.h"
#include "constants.h"
#include "json/json.h"
#include "webcachedb.h"

#include <QVariantMap>

#define STATE_INIT 0
#define STATE_STATION_INFO 1
#define STATE_STATION_INFO_RESPONSE 2
#define STATE_SUBSCRIBE 3
#define STATE_STREAMING 4

#if (QT_VERSION < QT_VERSION_CHECK(5,2,0))
#include <limits>
#define qQNaN std::numeric_limits<double>::quiet_NaN
#endif

TcpLiveDataSource::TcpLiveDataSource(QObject *parent) :
    AbstractLiveDataSource(parent)
{
    socket.reset(new QTcpSocket());
    connect(socket.data(), SIGNAL(connected()), this, SLOT(connected()));
    connect(socket.data(), SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(socket.data(), SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(error(QAbstractSocket::SocketError)));
    connect(socket.data(), SIGNAL(readyRead()), this, SLOT(readyRead()));

    reconnectTimer.setInterval(5000);
    connect(&reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()));

    LastUpdate = QDateTime::currentDateTime();
    watchdog.setInterval(5000);
    connect(&watchdog, SIGNAL(timeout()), this, SLOT(checkConnection()));

    state = STATE_INIT;
}

TcpLiveDataSource::~TcpLiveDataSource() {
    // If we don't disconnect from the sockets events we may have our slots
    // called after the destructor has completed (causing segfaults, etc).
    socket->disconnect(this, SLOT(connected()));
    socket->disconnect(this, SLOT(disconnected()));
    socket->disconnect(this, SLOT(readyRead()));
    socket->disconnect(this, SLOT(error(QAbstractSocket::SocketError)));
}

void TcpLiveDataSource::enableLiveData() {
    liveDataEnabled = true;
    firstError = true;

    if (!socket->isOpen()) {
        Settings& settings = Settings::getInstance();
        qDebug() << "Connect....";

        stationCode = settings.stationCode().toLower();
        hostName = settings.serverHostname();
        port = settings.serverPort();

        socket->connectToHost(hostName, port);
        watchdog.start();
    }
}

void TcpLiveDataSource::disableLiveData() {
    liveDataEnabled = false;
}

void TcpLiveDataSource::reconnect() {
    reconnectTimer.stop();
    socket->connectToHost(hostName, port);
}

void TcpLiveDataSource::connected() {
    qDebug() << "Connected";
}

void TcpLiveDataSource::disconnected() {
    qDebug() << "Disconnected. Reconnecting in 5.";
    reconnectTimer.start();
    state = STATE_INIT;
}

void TcpLiveDataSource::error(QAbstractSocket::SocketError socketError) {
    qDebug() << "Error:" << socketError;
    qDebug() << "Reconnect attempt in 5";
    reconnectTimer.start();
    state = STATE_INIT;

    if (firstError) {
        emit liveConnectFailed(socket->errorString());
        firstError = false;
    }
}

void TcpLiveDataSource::sendNextCommand() {
    if (state == STATE_INIT) {
        QByteArray data = "set client \"desktop\"/version=\"" +
                Constants::VERSION_STR + "\"\r\n";
        qDebug() << "SND:" << data;
        socket->write(data.constData(), data.length());

        state = STATE_STATION_INFO;
    } else if (state == STATE_STATION_INFO) {
        QByteArray data = "show station \"";
        data += stationCode ;
        data += "\"/json\r\n";
        qDebug() << "SND:" << data;
        socket->write(data.constData(), data.length());
        state = STATE_STATION_INFO_RESPONSE;
    } else if (state == STATE_SUBSCRIBE) {
        // We've sent client details. Now to start streaming.
        QByteArray data("subscribe \"");
        data.append(stationCode);
        data.append("\"/live/samples/any_order/images\r\n");
        qDebug() << "SND:" << data;
        socket->write(data.constData(), data.length());
        state = STATE_STREAMING;
    }
    // No other commands to send.
}

void TcpLiveDataSource::processStreamLine(QString line) {
    line = line.trimmed();

    if (line.isEmpty()) return; // Nothing to process.
    if (line.startsWith("#")) {
        // Its a message of some sort.
        qDebug() << line;
        return;
    }

    QStringList parts = line.split(",");

    if (parts.count() == 0) {
        qDebug() << "Failed to parse line";
        return;
    }

    if (parts[0] == "l") {
        processLiveData(parts);
    } else if (parts[0] == "i") {
        processImageData(parts);
    } else if (parts[0] == "s") {
        processSample(parts);
    } else {
        qDebug() << "Unexpected data type: " << parts[0];
    }
}

#define NullableDouble(i) parts.at(i) == "None" ? qQNaN() : parts.at(i).toDouble();

void TcpLiveDataSource::processLiveData(QStringList parts) {
    if (parts.at(0) != "l") {
        qDebug() << "Not a live update. Type:" << parts.at(0);
        return;
    }

    int expectedLength = 11;

    if (hw_type == HW_DAVIS) {
        expectedLength = 21;
    }

    if (parts.length() < expectedLength) {
        qDebug() << "Invalid live data line:" << parts.join(",");
        return;
    }

    if (!liveDataEnabled) return;

    LiveDataSet lds;
    lds.temperature = parts.at(1).toFloat();
    lds.dewPoint = parts.at(2).toFloat();
    lds.apparentTemperature = parts.at(3).toFloat();
    lds.windChill = parts.at(4).toFloat();
    lds.humidity = parts.at(5).toInt();
    lds.indoorTemperature = parts.at(6).toFloat();
    lds.indoorHumidity = parts.at(7).toInt();
    lds.pressure = parts.at(8).toFloat();
    lds.windSpeed = parts.at(9).toFloat();
    lds.windDirection = parts.at(10).toInt();
    lds.timestamp = QDateTime::currentDateTime();

    lds.indoorDataAvailable = true;

    lds.hw_type = hw_type;
    if (hw_type == HW_DAVIS) {
        lds.davisHw.barometerTrend = parts.at(11).toInt();
        lds.davisHw.rainRate = parts.at(12).toFloat();
        lds.davisHw.stormRain = parts.at(13).toFloat();
        lds.davisHw.stormDateValid = !(parts.at(14) == "None");
        if (lds.davisHw.stormDateValid)
            lds.davisHw.stormStartDate = QDate::fromString(parts.at(14),
                                                           "yyyy-MM-dd");
        lds.davisHw.txBatteryStatus = parts.at(15).toInt();
        lds.davisHw.consoleBatteryVoltage = parts.at(16).toFloat();
        lds.davisHw.forecastIcon = parts.at(17).toInt();
        lds.davisHw.forecastRule = parts.at(18).toInt();
        lds.davisHw.uvIndex = parts.at(19).toFloat();
        lds.davisHw.solarRadiation = parts.at(20).toFloat();

        if (parts.length() > expectedLength) {
            // We've got extra sensors!
            lds.davisHw.leafWetness1 = NullableDouble(21);
            lds.davisHw.leafWetness2 = NullableDouble(22);
            lds.davisHw.leafTemperature1 = NullableDouble(23);
            lds.davisHw.leafTemperature2 = NullableDouble(24);

            lds.davisHw.soilMoisture1 = NullableDouble(25);
            lds.davisHw.soilMoisture2 = NullableDouble(26);
            lds.davisHw.soilMoisture3 = NullableDouble(27);
            lds.davisHw.soilMoisture4 = NullableDouble(28);

            lds.davisHw.soilTemperature1 = NullableDouble(29);
            lds.davisHw.soilTemperature2 = NullableDouble(30);
            lds.davisHw.soilTemperature3 = NullableDouble(31);
            lds.davisHw.soilTemperature4 = NullableDouble(32);

            lds.davisHw.extraTemperature1 = NullableDouble(33);
            lds.davisHw.extraTemperature2 = NullableDouble(34);
            lds.davisHw.extraTemperature3 = NullableDouble(35);

            lds.davisHw.extraHumidity1 = NullableDouble(36);
            lds.davisHw.extraHumidity2 = NullableDouble(37);
        }
    }

    emit liveData(lds);
}

void TcpLiveDataSource::processSample(QStringList parts) {
    if (parts.at(0) != "s") {
        qDebug() << "Not a sample. Type:" << parts.at(0);
        return;
    }

    int expectedLength = 14;

    if (hw_type == HW_DAVIS) {
        expectedLength = 16;
    }

    if (parts.length() < expectedLength) {
        qDebug() << "Invalid sample data line:" << parts.join(",");
        return;
    }

    Sample s;
    s.timestamp = QDateTime::fromString(parts.at(1), Qt::ISODate);
    // TODO: these values could all be 'None' if there station is having trouble
    // receiving data
    s.temperature = parts.at(2).toDouble();
    s.dewPoint = parts.at(3).toDouble();
    s.apparentTemperature = parts.at(4).toDouble();
    s.windChill = parts.at(5).toDouble();
    s.humidity = parts.at(6).toDouble();
    s.indoorTemperature = parts.at(7).toDouble();
    s.indoorHumidity = parts.at(8).toDouble();
    s.pressure = parts.at(9).toDouble();
    s.averageWindSpeed = parts.at(10).toDouble();
    s.gustWindSpeed = parts.at(11).toDouble();
    s.windDirectionValid = parts.at(12) != "None";
    s.windDirection = parts.at(12).toUInt();
    s.rainfall = parts.at(13).toDouble();

    if (hw_type == HW_DAVIS && parts.length() >= 16) {
        s.solarRadiationValid = true;
        s.uvIndexValid = true;
        s.uvIndex = parts.at(14).toDouble();
        s.solarRadiation = parts.at(15).toDouble();
    }

    emit newSample(s);
}

void TcpLiveDataSource::processImageData(QStringList parts) {
    if (parts.at(0) != "i") {
        qDebug() << "Not an image. Type: " << parts.at(0);
        return;
    }

    if (parts.length() < 7) {
        qDebug() << "Unexpected image data format: fewer than 6 parameters: "
                 << parts.join(",");
        return;
    }

    QString stationCode = parts[1];
    QString stationUrl = Settings::getInstance().webInterfaceUrl() +
            "data/" + stationCode + "/";

    // Minimal metadata for storage in the WebCacheDB. This is enough for the
    // WebDataSource to go off and fetch it from on-disk cache or download it
    // from the internet. The missing bits may be filled out later if the image
    // browser is opened later.
    ImageInfo image;
    image.id = parts[6].toInt();
    image.timeStamp = QDateTime::fromString(parts[4], Qt::ISODate);
    image.imageTypeCode = parts[3];
    image.title = tr("<unknown>");
    image.description = tr("<partial metadata received via TCPLiveDataSource>");
    image.mimeType = parts[5];
    image.imageSource.code = parts[2];
    image.imageSource.name = tr("<unknown>");
    image.imageSource.description = tr("<partial metadata received via TCPLiveDataSource>");

    QString extension = "jpeg";
    if (image.mimeType == "image/jpeg")
        extension = "jpeg";
    else if (image.mimeType == "image/png")
        extension = "png";
    else if (image.mimeType == "image/gif")
        extension = "gif";
    else if (image.mimeType == "video/mp4")
        extension = "mp4";
    else if (image.mimeType == "video/webm")
        extension = "webm";
    else if (image.mimeType == "audio/wav")
        extension = "wav";

    // This is fairly nasty :(
    image.fullUrl = (stationUrl +
            QString::number(image.timeStamp.date().year()) + "/" +
            QString::number(image.timeStamp.date().month()) + "/" +
            QString::number(image.timeStamp.date().day()) + "/" +
            "images/" +
            image.imageSource.code + "/" +
            image.timeStamp.time().toString("HH_mm_ss") + "/" +
            image.imageTypeCode + "_full." + extension).toLower();


    // Store the image metadata somewhere so other parts of the application can
    // grab the metadata by image ID only (this is something that could have
    // been designed better).
    WebCacheDB::getInstance().storeImageInfo(stationUrl, image);

    // Prepare the minimal new-image-info data for broadcast to the rest of the
    // application. Most of the app is really only interested in the image ID
    // which will be looked up in either the WebCacheDB or the main database.
    NewImageInfo nii;
    nii.stationCode = stationCode;
    nii.imageSourceCode = image.imageSource.code;
    nii.timestamp = image.timeStamp;
    nii.imageId = image.id;

    emit newImage(nii);
}


void TcpLiveDataSource::processStationInfo(QString line) {
    using namespace QtJson;

    line = line.trimmed();
    if (line.isEmpty()) return; // Nothing to process.

    stationInfoBuffer += line;

    bool ok;

    QVariantMap result = Json::parse(stationInfoBuffer, ok).toMap();

    if (!ok) {
        qDebug() << "Failed to process station information - assuming more data required.";
        qDebug() << "Received data:" << line;
        qDebug() << "Buffer:" << stationInfoBuffer;
        return;
    }

    state = STATE_SUBSCRIBE;

    qDebug() << "Processing station info...";

    // Lots of other stuff is available in this map too such as station
    // name and description.
    QString hardwareTypeCode = result["hardware_type_code"].toString();

    bool solar_available = false;
    QString station_name = result["name"].toString();

    qDebug() << "Hardware type code:" << hardwareTypeCode;
    qDebug() << "Station Name:" << station_name;

    if (hardwareTypeCode == "DAVIS") {
        hw_type = HW_DAVIS;
        solar_available = result["config"].toMap()["has_solar_and_uv"].toBool();
        qDebug() << "Solar available:" << solar_available;
    }
    else if (hardwareTypeCode == "FOWH1080")
        hw_type = HW_FINE_OFFSET;
    else
        hw_type = HW_GENERIC;

    emit stationName(station_name);
    emit isSolarDataEnabled(solar_available);
    stationInfoBuffer = "";
}

void TcpLiveDataSource::readyRead() {
    LastUpdate = QDateTime::currentDateTime();

    QByteArray line = socket->readLine();

    while (line.count() != 0) {
        line = line.trimmed();
//        qDebug() << "RECV:" << line;
//        qDebug() << "STATE:" << state;

        if (line == "_ok" || state == STATE_INIT)
            sendNextCommand();
        else if (state == STATE_STATION_INFO_RESPONSE)
            processStationInfo(line);
        else if (state == STATE_STREAMING)
            processStreamLine(line);

        line = socket->readLine();
    }
}

hardware_type_t TcpLiveDataSource::getHardwareType() {
    return HW_GENERIC;
}

void TcpLiveDataSource::checkConnection() {
    int delta = QDateTime::currentDateTime().toTime_t() - LastUpdate.toTime_t();

    if (delta > 300) {
        // So we don't try reconnecting for another five minutes
        LastUpdate = QDateTime::currentDateTime();

        qDebug() << "Silent connection - reconnecting...";
        // 5 minutes since last communication with the server. Reset the connection.

        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->disconnectFromHost();
        } else {
            enableLiveData();
        }

    }
}
