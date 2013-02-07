#include "tcplivedatasource.h"
#include "settings.h"
#include "constants.h"

#define STATE_INIT 0
#define STATE_SUBSCRIBE 1
#define STATE_STREAMING 2

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

        state = STATE_SUBSCRIBE;
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

    if (parts.length() < 11) {
        qDebug() << "Invalid live data line:" << line;
        return;
    }

    if (parts.at(0) != "l") {
        qDebug() << "Not a sample. Type:" << parts.at(0);
    }

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
    lds.gustWindSpeed = parts.at(10).toFloat();
    lds.windDirection = parts.at(11).toInt();
    lds.timestamp = QDateTime::currentDateTime();

    emit liveData(lds);
}

void TcpLiveDataSource::readyRead() {
    QByteArray line = socket->readLine();

    while (line.count() != 0) {
        line = line.trimmed();
        qDebug() << "RECV:" << line;
        qDebug() << "STATE:" << state;

        if (line == "_ok" || state == STATE_INIT)
            sendNextCommand();
        else if (state == STATE_STREAMING) {
            processStreamLine(line);
        }
        line = socket->readLine();
    }
}
