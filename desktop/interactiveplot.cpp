#include "interactiveplot.h"

InteractivePlot::InteractivePlot(QWidget *parent) :
    QCustomPlot(parent)
{
    connect(this, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePress(QMouseEvent*)));
    connect(this, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMove(QMouseEvent*)));
    connect(this, SIGNAL(mouseRelease(QMouseEvent*)),
            this, SLOT(mouseRelease()));
    connect(this, SIGNAL(mouseWheel(QWheelEvent*)),
            this, SLOT(mouseWheel(QWheelEvent*)));
    connect(this, SIGNAL(selectionChangedByUser()),
            this, SLOT(axisSelectionChanged()));

    setInteractions(QCP::iRangeZoom |
                    QCP::iSelectAxes |
                    QCP::iRangeDrag |
                    QCP::iSelectPlottables);
    axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
}

void InteractivePlot::mousePress(QMouseEvent *event) {
    // Only allow panning in the direction of the selected axis
    if (xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeDrag(xAxis->orientation());
    else if (isAnyYAxisSelected() && !isYAxisLockEnabled()) {
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        axisRect()->setRangeDrag(axis->orientation());
        axisRect()->setRangeDragAxes(xAxis, axis);
    } else {
        /* No specific axis selected. Pan all the axes!
         *
         * Problem: QCustomPlot can't pan more than one set of axes. Damn.
         *
         * Solution: Let QCustomPlot pan X1,Y1 and we'll have to manually
         *           pan all the remaining Y axes. :|
         *
         */
        axisRect()->setRangeDragAxes(xAxis, yAxis);

        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y Axis Lock must
            // be on. We'll only pan vertically.
            axisRect()->setRangeDrag(Qt::Vertical);
        } else {
            axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
        }

        mDragStart = event->pos();
        if (event->buttons() & Qt::LeftButton) {
            mDragging = true;
            // We probably don't need to mess with AA settings -
            // QCPAxisRect::mousePressEvent(QMouseEvent*) should fire next
            // and take care of that for us.

            // We have to store multiple vertical start ranges (one for each
            // Y axis).
            mDragStartVertRange.clear();
            foreach (QCPAxis* axis, valueAxes()) {
                QCPRange range = axis->range();
                mDragStartVertRange[axis] = range;
            }
        }
    }
}

void InteractivePlot::mouseMove(QMouseEvent *event){
    if (mDragging) {
        foreach(QCPAxis* axis, valueAxes()) {

            // QCustomPlot will handle RangeDrag for y1. We only need to do
            // all the others.
            if (axis == yAxis)
                continue;

            // This logic is exactly the same as what can be found in
            // QCPAxisRect::mouseMoveEvent(QMouseEvent*) from QCustomPlot 1.0.0

            if (axis->scaleType() == QCPAxis::stLinear) {

              double diff = axis->pixelToCoord(mDragStart.y())
                      - axis->pixelToCoord(event->pos().y());

              axis->setRange(
                          mDragStartVertRange[axis].lower + diff,
                          mDragStartVertRange[axis].upper + diff);

            } else if (axis->scaleType() == QCPAxis::stLogarithmic) {
              double diff = axis->pixelToCoord(mDragStart.y())
                      / axis->pixelToCoord(event->pos().y());
              axis->setRange(
                          mDragStartVertRange[axis].lower * diff,
                          mDragStartVertRange[axis].upper * diff);
            }
        }
        // We shouldn't need to do a replot -
        // QCPAxisRect::mouseMoveEvent(QMouseEvent*) should fire next and
        // handle it for us.
    }
}

void InteractivePlot::mouseRelease() {
    mDragging = false;
    // QCPAxisRect::mouseReleaseEvent(QMouseEvent *) should fire next and
    // deal with the AA stuff so we shouldn't need to here.
}

void InteractivePlot::mouseWheel(QWheelEvent* event) {
    // Zoom on what ever axis is selected (if one is selected)
    if (xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeZoom(xAxis->orientation());
    else if (isAnyYAxisSelected() && !isYAxisLockEnabled()) {
        // A Y axis is selected and axis lock is not on. So we'll just scale
        // that one axis.
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        axisRect()->setRangeZoom(axis->orientation());
        axisRect()->setRangeZoomAxes(xAxis, axis);
    } else {
        /* No specific axis selected. Zoom all the axes!
         *
         * Problem: QCustomPlot can't zoom multiple axes. Damn.
         *
         * Solution: Let it zoom x1 and y1 and we'll handle zooming all the
         *           other y axes manually :|
         */
        axisRect()->setRangeZoomAxes(xAxis, yAxis);

        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y axis lock must
            // be on. We'll only scale vertically.
            axisRect()->setRangeZoom(Qt::Vertical);
        } else {
            axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
        }

        // This is the same logic as used by QCustomPlot v1.0.0 in
        // QCPAxisRect::wheelEvent(QWheelEvent*)

        // a single step delta is +/-120 usually
        double wheelSteps = event->delta()/120.0;
        double verticalRangeZoomFactor =
                axisRect()->rangeZoomFactor(Qt::Vertical);

        foreach (QCPAxis* axis, valueAxes()) {
            double factor = pow(verticalRangeZoomFactor, wheelSteps);
            // We don't want to scale y1 - QCustomPlot will handle that.
            if (axis != yAxis) {
                axis->scaleRange(factor,
                                 axis->pixelToCoord(event->pos().y()));
            }
        }
    }
}

bool InteractivePlot::isAnyYAxisSelected() {
    bool yAxisPartSelected = false;
    foreach(QCPAxis* axis, valueAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            yAxisPartSelected = true;
    }

    return yAxisPartSelected;
}

QPointer<QCPAxis> InteractivePlot::valueAxisWithSelectedParts() {

    foreach(QCPAxis* axis, valueAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            return axis;
    }
    return 0;
}

QList<QCPAxis*> InteractivePlot::valueAxes() {
    return axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight);
}

void InteractivePlot::axisSelectionChanged() {
    // If either x axis or its tick labels is selected, select both axes
    if (xAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
            xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
            xAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
            xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels)) {

        xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }

    // If either y axis or its tick labels are selected, select both axes
    if (isAnyYAxisSelected()) {
        // If one part (tick labels or the actual axis) is selected, ensure
        // both are.

        if (isYAxisLockEnabled()) {
            // ALL axes should be selected
            foreach (QCPAxis* axis, valueAxes()) {
                axis->setSelectedParts(QCPAxis::spAxis |
                                       QCPAxis::spTickLabels);
            }
        } else {
            // Just ensure the axis is fully selected.
            QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
            axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        }
    }
}
