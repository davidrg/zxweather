#include "livemonitor.h"
#include "settings.h"

LiveMonitor::LiveMonitor(QObject *parent) :
    QObject(parent)
{
    timer = 0;
    enabled = false;

    reconfigure();
}


void LiveMonitor::LiveDataRefreshed() {
    if (!timer) // Not configured yet.
        return;
    timer->start(); // restart the timer to prevent timeout
    lastRefresh = QDateTime::currentDateTime();
}

void LiveMonitor::reconfigure() {
    if (!timer) {
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    }

    Settings& settings = Settings::getInstance();

    if (settings.liveTimeoutEnabled()) {
        timer->setInterval(settings.liveTimeoutInterval());
        timer->start();
    } else {
        timer->stop();
    }
}

void LiveMonitor::timeout() {
    if (!enabled) return;

    emit showWarningPopup("No live data updates have been received since " +
                          lastRefresh.toString() +
                          ".",
                          "Live data is late",
                          "Live data is late",
                          true);
}

void LiveMonitor::enable() {
    LiveDataRefreshed(); // reset the timer just in case its about to expire.
    enabled = true;
}