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
            SampleColumns columns,
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime(),
            AggregateFunction aggregateFunction = AF_None,
            AggregateGroupType groupType = AGT_None,
            uint32_t groupMinutes = 0);

    void enableLiveData();

    hardware_type_t getHardwareType();

private slots:
    void notificationPump(bool force = false);
    void dbError(QString message);

private:
    int getStationId();
    QString getStationHwType();

    //QString buildSelectForColumns(SampleColumns columns);

    // Live data functionality
    void connectToDB();
    QScopedPointer<QTimer> notificationTimer;
    QScopedPointer<DBSignalAdapter> signalAdapter;
};

#endif // DATABASEDATASOURCE_H
