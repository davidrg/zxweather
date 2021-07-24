#include "valueaxistag.h"

#include "charts/plotwidget.h"


/** A variant of QCPItemTracer that is transparent to mouse clicks
 * and allows setting axes in the constructor.
 */
class TransparentAxisItemTracer: public QCPItemTracer {

public:
    explicit TransparentAxisItemTracer(QCPAxis* keyAxis, QCPAxis* valueAxis, QCustomPlot *parentPlot)
        : QCPItemTracer(parentPlot) {
        position->setAxisRect(keyAxis->axisRect());
        position->setAxes(keyAxis, valueAxis);
    }

    explicit TransparentAxisItemTracer(QCPGraph* graph, QCustomPlot *parentPlot)
        : QCPItemTracer(parentPlot) {
        setGraph(graph);
        position->setAxisRect(graph->valueAxis()->axisRect());
        position->setAxes(graph->keyAxis(), graph->valueAxis());
    }

    double selectTest ( const QPointF &  pos, bool  onlySelectable, QVariant *  details = 0  ) const {
        Q_UNUSED(pos)
        Q_UNUSED(onlySelectable)
        Q_UNUSED(details)
        return -1.0;
    }
};



ValueAxisTag::ValueAxisTag(QCPAxis *keyAxis, QCPAxis *valueAxis,
                           bool onValueAxis, bool arrow, PlotWidget *parent):
    TracingAxisTag(onValueAxis ? valueAxis : keyAxis,
                   arrow,
                   new TransparentAxisItemTracer(keyAxis, valueAxis, parent)
        )
{
    tracer->setVisible(false);
    tracer->position->setTypeX(QCPItemPosition::ptAxisRectRatio);
    tracer->position->setTypeY(QCPItemPosition::ptPlotCoords);
    tracer->position->setCoords(1, 0);
}

ValueAxisTag::ValueAxisTag(QCPGraph *graph,
                           bool onValueAxis, bool arrow, PlotWidget *parent):
    TracingAxisTag(onValueAxis ? graph->valueAxis() : graph->keyAxis(),
                   arrow,
                   new TransparentAxisItemTracer(graph, parent)
        )
{
    tracer->setVisible(false);
    tracer->position->setTypeX(QCPItemPosition::ptAxisRectRatio);
    tracer->position->setTypeY(QCPItemPosition::ptPlotCoords);
    tracer->position->setCoords(1, 0);
}



void ValueAxisTag::setValue(double key, double value) {
    tracer->position->setCoords(key, value);
    update();
}
