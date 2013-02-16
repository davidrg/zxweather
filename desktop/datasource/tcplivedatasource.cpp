#include "tcplivedatasource.h"
#include "settings.h"
#include "constants.h"
#include "json/json.h"

#include <QVariantMap>

#define STATE_INIT 0
#define STATE_STATION_INFO 1
#define STATE_STATION_INFO_RESPONSE 2
#define STATE_SUBSCRIBE 3
#define STATE_STREAMING 4

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
    Settings& settings = Settings::getInstance();
    qDebug() << "Connect....";

    stationCode = settings.stationCode();
    hostName = settings.serverHostname();
    port = settings.serverPort();

    socket->connectToHost(hostName, port);
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
        data.append("\"/live\r\n");
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

    int expectedLength = 12;

    if (hw_type == HW_DAVIS)
        expectedLength = 20;

    if (parts.length() < expectedLength) {
        qDebug() << "Invalid live data line:" << line;
        return;
    }

    if (parts.at(0) != "l") {
        qDebug() << "Not a sample. Type:" << parts.at(0);
    }

    qDebug() << "LEN::" << parts.count();

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

    // We don't do gust wind speed in the desktop client anymore.
    //lds.gustWindSpeed = parts.at(10).toFloat();

    lds.windDirection = parts.at(11).toInt();
    lds.timestamp = QDateTime::currentDateTime();

    lds.indoorDataAvailable = true;

    lds.hw_type = hw_type;
    if (hw_type == HW_DAVIS) {
        lds.davisHw.barometerTrend = parts.at(12).toInt();
        lds.davisHw.rainRate = parts.at(13).toFloat();
        lds.davisHw.stormRain = parts.at(14).toFloat();
        lds.davisHw.stormDateValid = (parts.at(15) != "None");
        if (lds.davisHw.stormDateValid)
            lds.davisHw.stormStartDate = QDate::fromString(parts.at(15),
                                                           "yyyy-MM-dd");
        lds.davisHw.txBatteryStatus = parts.at(16).toInt();
        lds.davisHw.consoleBatteryVoltage = parts.at(17).toFloat();
        lds.davisHw.forecastIcon = parts.at(18).toInt();
        lds.davisHw.forecastRule = parts.at(19).toInt();
    }

    emit liveData(lds);
}

void TcpLiveDataSource::processStationInfo(QString line) {
    using namespace QtJson;

    line = line.trimmed();
    if (line.isEmpty()) return; // Nothing to process.

    bool ok;

    state = STATE_SUBSCRIBE;

    QVariantMap result = Json::parse(line, ok).toMap();

    if (!ok)
        qWarning() << "Failed to get station information";

    // Lots of other stuff is available in this map too such as station
    // name and description.
    QString hardwareTypeCode = result["hardware_type_code"].toString();

    if (hardwareTypeCode == "DAVIS")
        hw_type = HW_DAVIS;
    else if (hardwareTypeCode == "FOWH1080")
        hw_type = HW_FINE_OFFSET;
    else
        hw_type = HW_GENERIC;
}


void TcpLiveDataSource::readyRead() {
    QByteArray line = socket->readLine();

    while (line.count() != 0) {
        line = line.trimmed();
        qDebug() << "RECV:" << line;
        qDebug() << "STATE:" << state;

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
