#ifndef DATABASEDATASOURCE_H
#define DATABASEDATASOURCE_H

#include <QObject>
#include "abstractdatasource.h"

class DatabaseDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit DatabaseDataSource(QWidget* parentWidget = 0, QObject *parent = 0);
    
    virtual void fetchSamples(
            QDateTime startTime,
            QDateTime endTime=QDateTime::currentDateTime());

private:
    int getStationId();
    QString getStationHwType();
};

#endif // DATABASEDATASOURCE_H
