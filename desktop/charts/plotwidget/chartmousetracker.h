#ifndef CHARTMOUSETRACKER_H
#define CHARTMOUSETRACKER_H

#include <QObject>
#include <QList>
#include <QMap>

#include "charts/qcp/qcustomplot.h"
#include "charts/plotwidget.h"

/** Highlights the point nearest the mouse cursors X coordinate on all
 * graphs in the default axis rect and tags those points coordinates on
 * their axes
 */
class ChartMouseTracker : public QObject
{
    Q_OBJECT
public:
    explicit ChartMouseTracker(PlotWidget *plotWidget);

    bool isEnabled() { return enabled; }

public slots:
    void setEnabled(bool enabled);

private slots:
    void mouseMove(QMouseEvent *event);
    void mouseLeave(QEvent *event);

private:
    void setupPointTracing();
    void cleanupPointTracing();
    QCPItemText* makeAxisTag(QCPAxis* axis);
    void updateKeyAxisTag(QCPItemText* tag, double axisValue, QCPAxis *axis);
    void updateValueAxisTag(QCPItemText* tag, double axisValue, QCPAxis *axis);

    bool enabled;
    QList<QCPItemTracer*> pointTracers;
    QMap<QCPAxis*, QCPItemText*> keyAxisTags;
    QMap<QCPGraph*, QCPItemText*> valueAxisTags;

    PlotWidget * chart;
};

#endif // CHARTMOUSETRACKER_H
