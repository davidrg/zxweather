#ifndef ABSTRACTLIVEAGGREGATOR_H
#define ABSTRACTLIVEAGGREGATOR_H

#include <QObject>

#include "datasource/abstractlivedatasource.h"

class AbstractLiveAggregator : public QObject
{
    Q_OBJECT
public:
    explicit AbstractLiveAggregator(QObject *parent = 0);

signals:
    void liveData(LiveDataSet data);

public slots:
    virtual void incomingLiveData(LiveDataSet data) = 0;
};

#endif // ABSTRACTLIVEAGGREGATOR_H
