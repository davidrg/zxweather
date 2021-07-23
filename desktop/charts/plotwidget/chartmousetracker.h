#ifndef CHARTMOUSETRACKER_H
#define CHARTMOUSETRACKER_H

#include <QObject>
#include <QList>
#include <QMap>

class PlotWidget;
class QCPItemTracer;
class QCPAxis;
class QCPGraph;
class TracingAxisTag;

/** Highlights the point nearest the mouse cursors X coordinate on all
 * graphs in the default axis rect and tags those points coordinates on
 * their axes
 */
class ChartMouseTracker : public QObject
{
    Q_OBJECT
public:
    explicit ChartMouseTracker(PlotWidget *plotWidget);

    /** If the Chart Mouse Tracker is currently enabled or not
     *
     * @return Current enabled state
     */
    bool isEnabled() { return enabled; }

public slots:
    /** Turns the Chart Mouse Tracker on or off
     *
     * @param enabled New enabled state
     */
    void setEnabled(bool enabled);

private slots:
    void mouseMove(QMouseEvent *event);
    void mouseLeave(QEvent *event);

private:
    void setupPointTracing();
    void cleanupPointTracing();

    bool enabled;
    QList<QCPItemTracer*> pointTracers;
    QMap<QCPAxis*, TracingAxisTag*> keyAxisTags;
    QMap<QCPGraph*, TracingAxisTag*> valueAxisTags;

    PlotWidget * chart;
};

#endif // CHARTMOUSETRACKER_H
