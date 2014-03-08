#ifndef SERVERSTATIONLISTER_H
#define SERVERSTATIONLISTER_H

#include <QObject>
#include <QStringList>
#include <QScopedPointer>
#include <QTcpSocket>


/**
 * @brief The ServerStationLister class handles conencting to a remote
 * zxweather server and obtaining a list of all stations available on it.
 */
class ServerStationLister : public QObject
{
    Q_OBJECT
public:
    explicit ServerStationLister(QObject *parent = 0);
    
signals:
    void finished(QStringList stations);
    void error(QString errorMessage);
    void statusUpdate(QString message);

public slots:
    /**
     * @brief fetchStationList begins fetching the station list. When the
     * station list has been retreived the finished(QStringList) signal is
     * emitted with the list of stations. If an error occurrs instead the
     * error(QString) signal is emitted.
     *
     * @param server Server to connect to
     * @param port Port to connect to
     */
    void fetchStationList(QString server, int port);

private slots:
    void connected();
    void disconnected();
    void error(QAbstractSocket::SocketError errorNumber);
    void readyRead();

private:
    QScopedPointer<QTcpSocket> socket;
    QByteArray jsonPayload;
    bool connectionInitFinished;
    bool stationListRequested;

    typedef enum {
        ST_CONNECT,
        ST_CONNECTED,
        ST_SETUP,
        ST_SETUP_COMPLETE,
        ST_REQUEST_STATIONS,
        ST_LISTING_STATIONS,
        ST_COMPLETE
    } State;

    State currentState;

    void sendNextCommand();
    void processStationList(QString jsonData);
};

#endif // SERVERSTATIONLISTER_H
