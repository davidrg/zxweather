#include "chartmousetracker.h"
#include "charts/qcp/qcustomplot.h"


/* TODO:
 *  -> It would be nice if this could be done per-axis-rect. The current implementation
 *     only supports the default so its not much good for plots with multiple axis rects.
 */


/** A variant of QCPItemTracer that is transparent to mouse clicks.
 * This allows the underlying graph to be clicked on when the tracer
 * is following the mouse cursor.
 */
class TransparentItemTracer: public QCPItemTracer {

public:
    explicit TransparentItemTracer(QCustomPlot *parentPlot) : QCPItemTracer(parentPlot) {}

    double selectTest ( const QPointF &  pos, bool  onlySelectable, QVariant *  details = 0  ) const {
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

QCPItemText* ChartMouseTracker::makeAxisTag(QCPAxis* axis) {
    QCPItemText* tag = new QCPItemText(chart);
    tag->setLayer("overlay");
    tag->setClipToAxisRect(false);
    tag->setPadding(QMargins(3,0,3,0));
    tag->setBrush(QBrush(Qt::white));
    tag->setPen(QPen(Qt::black));
    tag->setSelectable(false);
    if (axis->axisType() == QCPAxis::atTop) {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    } else if (axis->axisType() == QCPAxis::atBottom) {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    } else if (axis->axisType() == QCPAxis::atLeft) {
        tag->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else {
        tag->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    tag->setText("0.0");

    return tag;
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
            QCPItemText* tag = makeAxisTag(xAxis);
            tag->position->setAxes(xAxis, yAxis);
            keyAxisTags[xAxis] = tag;
        }

        if (!valueAxisTags.contains(graph)) {
            QCPItemText* tag = makeAxisTag(yAxis);
            tag->position->setAxes(xAxis, yAxis);
            tag->setPen(graph->pen());
            valueAxisTags[graph] = tag;
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
        chart->removeItem(keyAxisTags.take(keyAxisTags.keys().first()));
    }

    while(!valueAxisTags.isEmpty()) {
        chart->removeItem(valueAxisTags.take(valueAxisTags.keys().first()));
    }
    chart->replot();
}

void ChartMouseTracker::updateKeyAxisTag(QCPItemText* tag, double axisValue, QCPAxis* axis) {
    tag->setText(QDateTime::fromMSecsSinceEpoch(axisValue * 1000).toString(Qt::SystemLocaleShortDate));

    QPointer<QCPAxis> valueAxis = tag->position->valueAxis();
    double valueZero = valueAxis->pixelToCoord(chart->axisRect()->bottomLeft().y());
    double valueMax = valueAxis->pixelToCoord(chart->axisRect()->topRight().y() -1); // -1 to align with border

    QFontMetrics m(tag->font());
    double halfWidth = m.width(tag->text()) / 2;

    double left = chart->axisRect()->bottomLeft().x();
    double right = chart->axisRect()->bottomRight().x();

    double minPos = axis->pixelToCoord(halfWidth + left);
    double maxPos = axis->pixelToCoord(right - halfWidth);

    // Prevent the tag from going off the end of the chart.
    double xValue = axisValue;
    if (xValue < minPos) {
        xValue = minPos;
    } else if (xValue > maxPos) {
        xValue = maxPos;
    }

    if (axis->axisType() == QCPAxis::atTop) {
        // +1 to align with axis rect border
        tag->position->setCoords(xValue, valueMax);
    } else {
        tag->position->setCoords(xValue, valueZero);
    }
}

void ChartMouseTracker::updateValueAxisTag(QCPItemText *tag, double axisValue, QCPAxis *axis) {

    QPointer<QCPAxis> keyAxis = tag->position->keyAxis();

    QCPRange range = axis->range();
    if (axisValue < range.lower || axisValue > range.upper) {
        tag->setVisible(false);
    } else {
        tag->setVisible(true);
        tag->setText(QString::number(axisValue));

        if (axis->axisType() == QCPAxis::atLeft) {
            tag->position->setCoords(
                        keyAxis->pixelToCoord(chart->axisRect()->bottomLeft().x() - axis->offset()), axisValue);
        } else {
            // +1 to align with axis rect border
            tag->position->setCoords(
                        keyAxis->pixelToCoord(chart->axisRect()->bottomRight().x() + axis->offset() + 1),
                        axisValue);
        }
    }
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

            pointTracer->setGraphKey(graph->keyAxis()->pixelToCoord(event->pos().x()));
            pointTracer->updatePosition();

            QCPAxis *xAxis = graph->keyAxis();
            QCPAxis *yAxis = graph->valueAxis();

            double xValue = pointTracer->position->coords().x();
            double yValue = pointTracer->position->coords().y();

            if (xAxis->visible()) {
                updateKeyAxisTag(keyAxisTags[xAxis], xValue, xAxis);
            }
            updateValueAxisTag(valueAxisTags[graph], yValue, yAxis);
        }
        chart->replot();
    }
}

void ChartMouseTracker::mouseLeave(QEvent *event) {
    cleanupPointTracing();
}
