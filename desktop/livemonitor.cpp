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

    lastRefresh = QDateTime::currentDateTime();
}

void LiveMonitor::reconfigure() {
    if (!timer) {
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    }

    Settings& settings = Settings::getInstance();

    if (settings.liveTimeoutEnabled()) {
        interval = settings.liveTimeoutInterval();
        timer->setInterval(interval);
        timer->start();
    } else {
        timer->stop();
    }
}

void LiveMonitor::timeout() {
    if (!enabled) return;

    if (QDateTime::currentMSecsSinceEpoch() - lastRefresh.toMSecsSinceEpoch() < interval) {
        return; // Update was recent enough to be ok.
    }

    emit showWarningPopup(QString(tr("No live data updates have been received since %1."))
                          .arg(lastRefresh.toString()),
                          tr("Live data is late", "title"),
                          tr("Live data is late", "tool-tip"),
                          true);
}

void LiveMonitor::enable() {
    LiveDataRefreshed(); // reset the timer just in case its about to expire.
    enabled = true;
}
