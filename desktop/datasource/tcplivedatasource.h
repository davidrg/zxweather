#ifndef TCPLIVEDATASOURCE_H
#define TCPLIVEDATASOURCE_H

#include "abstractlivedatasource.h"

#include <QTcpSocket>
#include <QScopedPointer>

class TcpLiveDataSource : public AbstractLiveDataSource
{
    Q_OBJECT
public:
    explicit TcpLiveDataSource(QObject *parent = 0);
    
    void enableLiveData();

signals:

private slots:
    void connected();
    void disconnected();
    void error(QAbstractSocket::SocketError socketError);
    void readyRead();
    
private:
    QScopedPointer<QTcpSocket> socket;
    int state;
    QString stationCode;

    void sendNextCommand();
    void processStreamLine(QString line);
};

#endif // TCPLIVEDATASOURCE_H
