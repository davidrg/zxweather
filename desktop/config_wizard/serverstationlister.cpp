#include "serverstationlister.h"
#include "constants.h"
#include "json/json.h"

#include <QtDebug>

ServerStationLister::ServerStationLister(QObject *parent) :
    QObject(parent)
{
    socket.reset(new QTcpSocket());
    connect(socket.data(), SIGNAL(connected()), this, SLOT(connected()));
    connect(socket.data(), SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(socket.data(), SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(error(QAbstractSocket::SocketError)));
    connect(socket.data(), SIGNAL(readyRead()), this, SLOT(readyRead()));

    connectionInitFinished = false;
    stationListRequested = false;
    jsonPayload.clear();
}

void ServerStationLister::fetchStationList(QString server, int port) {

    QString message = "Connecting to weather server...";
    qDebug() << message;
    emit statusUpdate(message);

    connectionInitFinished = false;
    stationListRequested = false;
    jsonPayload.clear();
    currentState = ST_CONNECT;
    socket->connectToHost(server, port);
}

void ServerStationLister::connected() {
    QString message = "Connected to server.";
    qDebug() << message;
    emit statusUpdate(message);
    currentState = ST_CONNECTED;
}

void ServerStationLister::disconnected() {
    QString message = "Disconnected from server.";
    qDebug() << message;
}

void ServerStationLister::error(QAbstractSocket::SocketError errorNumber) {
    QString message = "ERROR: " + socket->errorString() + " (" +
                      QString::number(errorNumber, 16) + ")";
    qDebug() << message;
    emit error(message);
}


void ServerStationLister::sendNextCommand() {
    ////
    // Throw away any data waiting to be read. Once we send
    // the next command it will just cause problems.
    qDebug() << "Discarding waiting socket data: " << socket->readAll();
    // We do this as at the time of writing the server has a
    // bug where it acknowledges each command twice.
    ////

    if (currentState == ST_CONNECTED) {
        // We need to setup the connection.
        emit statusUpdate("Setting up connection...");
        QByteArray data = "set client \"desktop\"/version=\"" +
                Constants::VERSION_STR + "\"\r\n";
        qDebug() << "SND:" << data;
        socket->write(data.constData(), data.length());
        currentState = ST_SETUP;
    } else if (currentState == ST_SETUP_COMPLETE){
        // Setup is finished. Request the station list.
        emit statusUpdate("Requesting station list...");
        QByteArray data = "list stations/json\r\n";
        qDebug() << "SND:" << data;
        socket->write(data.constData(), data.length());
        currentState = ST_REQUEST_STATIONS;
    } else {
        qWarning("Unexepected request to send next command. State=" + currentState);
    }
}

void ServerStationLister::readyRead() {
    QByteArray line = socket->readLine();

    while (line.count() != 0) {
        line = line.trimmed();
        qDebug() << line;

        if (currentState == ST_CONNECTED) {
            // Begin connection setup.
            sendNextCommand();
        } else if (currentState == ST_SETUP && line == "_ok") {
            currentState = ST_SETUP_COMPLETE;
            sendNextCommand(); // Request station list.
        } else if ((currentState == ST_REQUEST_STATIONS|| currentState == ST_LISTING_STATIONS) && line != "_ok") {
            // The station request has been sent and we've received a response.
            // This should be the station list in JSON form.
            currentState = ST_LISTING_STATIONS;
            jsonPayload.append(line);
        } else if (currentState == ST_LISTING_STATIONS && line == "_ok") {
            // We've finished listing stations.
            currentState = ST_COMPLETE;
            processStationList(jsonPayload);
            socket->disconnectFromHost();
            return;
        }

        line = socket->readLine();
    }
}

void ServerStationLister::processStationList(QString jsonData) {
    emit statusUpdate("Processing station list...");
    qDebug() << "Processing station list...";

    qDebug() << "JSON-encoded station data:" << jsonData;

    using namespace QtJson;

    bool ok;

    QList<QVariant> stations = Json::parse(jsonData, ok).toList();

    QStringList availableStations;

    foreach(QVariant station, stations) {
        QVariantMap stationMap = station.toMap();
        availableStations.append(stationMap.value("code").toString());
    }

    qDebug() << "Stations available on server:" << availableStations;
    emit finished(availableStations);
}
