#ifndef CHARTMOUSETRACKER_H
#define CHARTMOUSETRACKER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QPointer>

class PlotWidget;
class QCPItemTracer;
class QCPAxis;
class QCPGraph;
class TracingAxisTag;
class QCPAxisRect;
class QMouseEvent;

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

    /** Updates the positions of the axis tags and the point
     *  under the cursor. Call this whenever the plots data
     *  changes.
     */
    void update();

private slots:
    void mouseMove(QMouseEvent *event);
    void mouseLeave(QEvent *event);

private:
    void setupPointTracing(QCPAxisRect *rect);
    void cleanupPointTracing();

    bool enabled;
    QList<QPointer<QCPItemTracer> > pointTracers;
    QMap<QCPAxis*, TracingAxisTag*> keyAxisTags;
    QMap<QCPGraph*, TracingAxisTag*> valueAxisTags;

    /** The axis rect the cursor is currently in
     */
    QPointer<QCPAxisRect> currentAxisRect;

    PlotWidget * chart;

    int currentXCoord;
};

#endif // CHARTMOUSETRACKER_H
