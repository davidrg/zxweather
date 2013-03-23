#ifndef LIVEMONITOR_H
#define LIVEMONITOR_H

#include <QObject>
#include <QTimer>
#include <QDateTime>

/** Alerts the user if live data and generates an alert if live data stops
 * arriving.
 */
class LiveMonitor : public QObject
{
    Q_OBJECT
public:
    explicit LiveMonitor(QObject *parent = 0);
    
signals:
    void showWarningPopup(QString message, QString title, QString tooltip,
                          bool setWarningIcon);
public slots:
    void LiveDataRefreshed();
    void reconfigure();
    void enable();
    void disable() {enabled = false;}

private slots:
    void timeout();

private:
    QTimer* timer;
    QDateTime lastRefresh;
    bool enabled;
};

#endif // LIVEMONITOR_H
