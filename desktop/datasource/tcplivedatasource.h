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

    hardware_type_t getHardwareType();

signals:

private slots:
    void connected();
    void disconnected();
    void error(QAbstractSocket::SocketError socketError);
    void readyRead();
    void reconnect();
    
private:
    QScopedPointer<QTcpSocket> socket;
    int state;
    QString stationCode;
    QString hostName;
    int port;
    QTimer reconnectTimer;

    hardware_type_t hw_type;

    void sendNextCommand();
    void processStreamLine(QString line);
    void processLiveData(QStringList parts);
    void processImageData(QStringList parts);
    void processStationInfo(QString line);
};

#endif // TCPLIVEDATASOURCE_H
