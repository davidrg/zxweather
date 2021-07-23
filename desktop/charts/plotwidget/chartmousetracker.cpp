#include "chartmousetracker.h"
#include "charts/plotwidget.h"
#include "tracingaxistag.h"

/* TODO:
 *  -> It would be nice if this could be done per-axis-rect. The current implementation
 *     only supports the default so its not much good for plots with multiple axis rects.
 *     For that we'd really need axis-rect enter/leave events rather than chart enter/leave
 *     events. The setupPointTracing() function then needs to only look at the current axis
 *     rect. Shouldn't be hard to do really.
 *
 *     Doing this would let it be used on the live plot window!
 */


/** A variant of QCPItemTracer that is transparent to mouse clicks.
 * This allows the underlying graph to be clicked on when the tracer
 * is following the mouse cursor.
 */
class TransparentItemTracer: public QCPItemTracer {

public:
    explicit TransparentItemTracer(QCustomPlot *parentPlot) : QCPItemTracer(parentPlot) {}

    double selectTest ( const QPointF &  pos, bool  onlySelectable, QVariant *  details = 0  ) const {
        Q_UNUSED(pos)
        Q_UNUSED(onlySelectable)
        Q_UNUSED(details)
        return -1.0;
    }
};

ChartMouseTracker::ChartMouseTracker(PlotWidget *plotWidget) : QObject(plotWidget)
{
    this->chart = plotWidget;
    connect(plotWidget, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMove(QMouseEvent*)));
    connect(plotWidget, SIGNAL(mouseLeave(QEvent*)),
            this, SLOT(mouseLeave(QEvent*)));
    enabled = true;
}

void ChartMouseTracker::setEnabled(bool enabled) {
    this->enabled = enabled;
    if (!enabled) {
        cleanupPointTracing();
        this->chart->replot();
    }
}

void ChartMouseTracker::setupPointTracing() {
    for (int i = 0; i < chart->graphCount(); i++) {
        QCPGraph *graph = chart->graph(i);

        if (!graph->visible()) {
            pointTracers.append(NULL);
            continue;
        }

        QCPItemTracer *pointTracer = new TransparentItemTracer(chart);
        pointTracer->setInterpolating(false);
        pointTracer->setStyle(QCPItemTracer::tsCircle);
        pointTracer->setPen(graph->pen());
        pointTracer->setBrush(graph->pen().color());
        pointTracer->setSize(7);
        pointTracer->setGraph(graph);
        pointTracer->setLayer("overlay");

        pointTracers.append(pointTracer);

        QCPAxis *xAxis = graph->keyAxis();
        QCPAxis *yAxis = graph->valueAxis();

        if (!keyAxisTags.contains(xAxis) && xAxis->visible()) {
            keyAxisTags[xAxis] = new TracingAxisTag(xAxis, false, pointTracer, this);
        }

        if (!valueAxisTags.contains(graph)) {
            valueAxisTags[graph] = new TracingAxisTag(yAxis, false, pointTracer, this);
        }
    }
}

void ChartMouseTracker::cleanupPointTracing() {
    while (!pointTracers.isEmpty()) {
        QCPItemTracer *tracer = pointTracers.takeFirst();
        if (tracer != NULL) {
            chart->removeItem(tracer);
        }
    }

    while(!keyAxisTags.isEmpty()) {
        delete keyAxisTags.take(keyAxisTags.keys().first());
    }

    while(!valueAxisTags.isEmpty()) {
        delete valueAxisTags.take(valueAxisTags.keys().first());
    }
    chart->replot();
}

void ChartMouseTracker::mouseMove(QMouseEvent* event) {
    if (pointTracers.count() != chart->graphCount() && enabled) {
        cleanupPointTracing();
        setupPointTracing();
    }
    if (enabled) {
        for(int i = 0; i < chart->graphCount(); i++) {
            QCPItemTracer *pointTracer = pointTracers.at(i);
            if (pointTracer == NULL) {
                continue;
            }

            QCPGraph* graph = pointTracer->graph();
            if (!graph->visible()) {
                continue;
            }

            QCPAxis *keyAxis = graph->keyAxis();

            pointTracer->setGraphKey(keyAxis->pixelToCoord(event->pos().x()));
            pointTracer->updatePosition();

            if (keyAxis->visible()) {
                keyAxisTags[keyAxis]->update();
            }
            valueAxisTags[graph]->update();
        }
        chart->replot();
    }
}

void ChartMouseTracker::mouseLeave(QEvent *event) {
    Q_UNUSED(event)

    cleanupPointTracing();
}
