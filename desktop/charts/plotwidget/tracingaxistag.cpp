#include "tracingaxistag.h"

#include "charts/plotwidget.h"

#include <QtDebug>
#include <QLocale>

/*
 * The tracing axis tag makes the following assumptions:
 *      -> The item tracer will be managed externally and we'll be told whenever
 *         it changes via the update() method.
 *      -> The key axis is always on the top or bottom and always in miliseconds
 *         since the epoch
 *      -> Value axis tags should always be drawn using the same pen as what ever
 *         graph the item tracer is attached to
 * This is really built to just support whatever ChartMouseTracker needs.
 */

TracingAxisTag::TracingAxisTag(QCPAxis *axis, bool arrow, QCPItemTracer *itemTracer,
                               QObject *parent):
    AbstractAxisTag(itemTracer->graph() != NULL ? itemTracer->graph()->keyAxis() : itemTracer->position->keyAxis(),
                    itemTracer->graph() != NULL ? itemTracer->graph()->valueAxis() : itemTracer->position->valueAxis(),
                    axis==(itemTracer->graph() != NULL ? itemTracer->graph()->valueAxis() : itemTracer->position->valueAxis()),
                    arrow,
                    parent),
    tracer(itemTracer)
{
    QCPGraph *graph = itemTracer->graph();
    if (graph != NULL) {
        if (label && onValueAxis) {
            setPen(graph->pen());
        }
    }
}

void TracingAxisTag::setCoords(double x, double y) {
    if (arrow) {
        arrow->end->setCoords(x, y);
    } else {
        label->position->setCoords(x, y);
    }
}

void TracingAxisTag::update() {
    if (tracer.isNull()) {
        return;
    }

    Q_ASSERT_X(format == 'f' || format == 'i', "TracingAxisTag::update", "Format must be f or i (unexpected format character)");

    double axisValue;
    if (onValueAxis) {
        axisValue = tracer->position->coords().y();
    } else {
        axisValue = tracer->position->coords().x();
    }

    // This is for the Value axis:
    if (onValueAxis) {
        QCPRange range = axis()->range();
        if (axisValue < range.lower || axisValue > range.upper) {
            label->setVisible(false);
        } else {
            label->setVisible(true);
            label->setText(QString::number(axisValue, format, precision));

            if (axis()->axisType() == QCPAxis::atLeft) {
                setCoords(keyAxis->pixelToCoord(axis()->axisRect()->bottomLeft().x() - axis()->offset()),
                          axisValue);
            } else {
                // +1 to align with axis rect border
                setCoords(keyAxis->pixelToCoord(axis()->axisRect()->bottomRight().x() + axis()->offset() + 1),
                          axisValue);
            }
        }
    } else {
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
        QLocale locale;
        auto ts = QDateTime::fromMSecsSinceEpoch(axisValue * 1000);
        label->setText(locale.toString(ts, locale.dateFormat(QLocale::ShortFormat)));
#else
        label->setText(QDateTime::fromMSecsSinceEpoch(axisValue * 1000).toString(Qt::SystemLocaleShortDate));
#endif

        QPointer<QCPAxis> valueAxis = label->position->valueAxis();
        double valueZero = valueAxis->pixelToCoord(axis()->axisRect()->bottomLeft().y());
        double valueMax = valueAxis->pixelToCoord(axis()->axisRect()->topRight().y() -1); // -1 to align with border

        QFontMetrics m(font());
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
        double halfWidth = m.horizontalAdvance(text()) / 2;
#else
        double halfWidth = m.width(text()) / 2;
#endif

        double left = axis()->axisRect()->bottomLeft().x();
        double right = axis()->axisRect()->bottomRight().x();

        double minPos = axis()->pixelToCoord(halfWidth + left);
        double maxPos = axis()->pixelToCoord(right - halfWidth);

        // Prevent the tag from going off the end of the chart.
        double xValue = axisValue;
        if (xValue < minPos) {
            xValue = minPos;
        } else if (xValue > maxPos) {
            xValue = maxPos;
        }

        if (axis()->axisType() == QCPAxis::atTop) {
            setCoords(xValue, valueMax);
        } else {
            setCoords(xValue, valueZero);
        }
    }
}
