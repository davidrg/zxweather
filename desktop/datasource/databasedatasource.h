#ifndef DATABASEDATASOURCE_H
#define DATABASEDATASOURCE_H

#include <QObject>
#include <QTimer>
#include <QScopedPointer>
#include "abstractdatasource.h"
#include "dbsignaladapter.h"

class DatabaseDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit DatabaseDataSource(QWidget* parentWidget = 0, QObject *parent = 0);
    
    ~DatabaseDataSource();

    virtual void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime());

    void enableLiveData();

private slots:
    void notificationPump();
    void dbError(QString message);

private:
    int getStationId();
    QString getStationHwType();

    // Live data functionality
    void connectToDB();
    QScopedPointer<QTimer> notificationTimer;
    QScopedPointer<DBSignalAdapter> signalAdapter;
};

#endif // DATABASEDATASOURCE_H
