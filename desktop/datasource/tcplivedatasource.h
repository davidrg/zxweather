#ifndef TCPLIVEDATASOURCE_H
#define TCPLIVEDATASOURCE_H

#include "abstractlivedatasource.h"
#include "abstractdatasource.h"

#include <QTcpSocket>
#include <QScopedPointer>
#include <QTimer>

class TcpLiveDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit TcpLiveDataSource(QObject *parent = 0);
    ~TcpLiveDataSource();

    void enableLiveData();
    void disableLiveData();

    hardware_type_t getHardwareType();

private slots:
    void connected();
    void disconnected();
    void error(QAbstractSocket::SocketError socketError);
    void readyRead();
    void reconnect();
    void checkConnection();
    
private:
    QScopedPointer<QTcpSocket> socket;
    int state;
    QString stationCode;
    QString hostName;
    int port;
    QTimer reconnectTimer;
    QTimer watchdog;
    QDateTime LastUpdate;
    bool liveDataEnabled;

    QString stationInfoBuffer;

    hardware_type_t hw_type;

    void sendNextCommand();
    void processStreamLine(QString line);
    void processLiveData(QStringList parts);
    void processImageData(QStringList parts);
    void processSample(QStringList parts);
    void processStationInfo(QString line);
};

#endif // TCPLIVEDATASOURCE_H
