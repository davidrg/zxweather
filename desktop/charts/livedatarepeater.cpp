#include "livedatarepeater.h"

#include <QtDebug>

LiveDataRepeater::LiveDataRepeater(QObject *parent) : QObject(parent)
{
    lastReceivedValid = false;

    connect(&timer, SIGNAL(timeout()), this, SLOT(repeatLastTransmission()));
}


void LiveDataRepeater::incomingLiveData(LiveDataSet data) {
    if (timer.remainingTime() <= 500) {
        qDebug() << "Intercepting retransmission" << timer.remainingTime();
    }
    timer.stop();

    previousTs = lastReceivedTs;

    lastReceived = data;
    lastReceivedTs = data.timestamp.toMSecsSinceEpoch();

    qDebug() << "Received transmission" << data.timestamp;

    if (data.hw_type == HW_DAVIS) {
        interval = 2500;
    } else if (data.hw_type == HW_FINE_OFFSET) {
        interval = 48000;
    } else {
        interval = 30000; // Unknown station. Assume 30 seconds.
    }

    emit liveData(data);

    // Start a timer to repeat this transmission. We'll add on an extra half second
    // to the timers interval so the weather station has a chance to override this
    timer.setInterval(interval + 500);
    timer.start();
}

void LiveDataRepeater::repeatLastTransmission() {
    lastReceived.timestamp = lastReceived.timestamp.addMSecs(interval);
    qDebug() << "Retransmission for ts" << lastReceived.timestamp;
    emit liveData(lastReceived);
}
