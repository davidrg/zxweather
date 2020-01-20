#ifndef LIVEDATAREPEATER_H
#define LIVEDATAREPEATER_H

#include "abstractliveaggregator.h"

#include <QObject>
#include <QTimer>

class LiveDataRepeater : public QObject
{
    Q_OBJECT
public:
    explicit LiveDataRepeater(bool isWebDs, QObject *parent = 0);

signals:
    void liveData(LiveDataSet data);

public slots:
   void incomingLiveData(LiveDataSet data);

private slots:
   void repeatLastTransmission();

private:
   int interval;
   qint64 lastReceivedTs;
   LiveDataSet lastReceived;

   qint64 previousTs;

   bool lastReceivedValid;

   QTimer timer;

   bool runningTotalRain;
   float lastStormRain;
   bool isWebDs;
};

#endif // LIVEDATAREPEATER_H
