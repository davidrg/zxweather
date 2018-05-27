#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <QWidget>

#include "datasource/abstractlivedatasource.h"

namespace Ui {
class StatusWidget;
}

class StatusWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit StatusWidget(QWidget *parent = 0);
    ~StatusWidget();
    
public slots:
    /** Called when new live data is available.
     *
     * @param data The new live data.
     */
    void refreshLiveData(LiveDataSet lds);

    /** Resets the widget. Call this when ever switching stations.
     */
    void reset();

    /** Turns the transmitter battery status on or off
     *
     * @param visible True for a wireless station, false otherwise
     */
    void setTransmitterBatteryVisible(bool visible);

private:
    Ui::StatusWidget *ui;

    uint updateCount;
};

#endif // STATUSWIDGET_H
