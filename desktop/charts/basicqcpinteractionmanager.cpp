#include "basicqcpinteractionmanager.h"

BasicQCPInteractionManager::BasicQCPInteractionManager(QCustomPlot *plot, QObject *parent) :
    QObject(parent)
{
    this->plot = plot;
    xAxisLock = false;
    yAxisLock = false;
    mDragging = false;

    connect(plot, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(mousePress(QMouseEvent*)));
    connect(plot, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMove(QMouseEvent*)));
    connect(plot, SIGNAL(mouseRelease(QMouseEvent*)),
            this, SLOT(mouseRelease()));
    connect(plot, SIGNAL(mouseWheel(QWheelEvent*)),
            this, SLOT(mouseWheel(QWheelEvent*)));
    connect(plot, SIGNAL(selectionChangedByUser()),
            this, SLOT(axisSelectionChanged()));
    connect(plot, SIGNAL(selectionChangedByUser()),
            this, SLOT(graphSelectionChanged()));
    connect(plot, SIGNAL(legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)),
            this, SLOT(legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)));
    connect(plot, SIGNAL(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)),
            this, SLOT(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)));

    plot->setInteractions(QCP::iRangeZoom |
                          QCP::iSelectAxes |
                          QCP::iRangeDrag |
                          QCP::iSelectPlottables |
                          QCP::iSelectLegend);
    plot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    plot->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    plot->legend->setSelectableParts(QCPLegend::spItems);
}

void BasicQCPInteractionManager::legendClick(QCPLegend *legend,
                                             QCPAbstractLegendItem *item,
                                             QMouseEvent *event) {
    Q_UNUSED(legend)
    Q_UNUSED(event)
    /*
     * Select the plottable associated with a legend item when the legend
     * item is selected.
     */

    QCPPlottableLegendItem* plotItem = qobject_cast<QCPPlottableLegendItem*>(item);

    if (plotItem == NULL) {
        qDebug() << "Not a plottable legend item.";
        // The legend item isn't for a plottable. nothing to do here.
        return;
    }

    QCPAbstractPlottable* plottable = plotItem->plottable();

    // Deselect any other selected plottables.
    QCustomPlot* plot = plottable->parentPlot();
    for (int i = 0; i < plot->plottableCount(); i++) {
        // This will deselect everything.
        plot->plottable(i)->setSelection(QCPDataSelection(QCPDataRange(0, 0)));
    }

    // Then select the plottable associated with this legend item.
    if (plotItem->selected()) {
        // Any arbitrary selection range will select the whole lot plottable
        // when the selection mode is Whole
        plottable->setSelection(QCPDataSelection(QCPDataRange(0,1)));
        emit graphSelected(true);
    }
}

void BasicQCPInteractionManager::plottableClick(QCPAbstractPlottable* plottable,
                                                int dataIndex,
                                                QMouseEvent* event) {
    Q_UNUSED(dataIndex);
    Q_UNUSED(event);

    if (plottable->selected()) {
        QCustomPlot* plot = plottable->parentPlot();

        // Clear selected items.
        for (int i = 0; i < plot->legend->itemCount(); i++) {
            QCPAbstractLegendItem* item = plot->legend->item(i);
            item->setSelected(false);
        }

        QCPPlottableLegendItem *lip = plot->legend->itemWithPlottable(plottable);
        lip->setSelected(true);
    }
}

void BasicQCPInteractionManager::mousePress(QMouseEvent *event) {
    // Only allow panning in the direction of the selected axis
    if (isAnyXAxisSelected() && !isXAxisLockEnabled()) {
        QPointer<QCPAxis> axis = keyAxisWithSelectedParts();
        plot->axisRect()->setRangeDrag(axis->orientation());
        plot->axisRect()->setRangeDragAxes(axis, plot->yAxis);
    } else if (isAnyYAxisSelected() && !isYAxisLockEnabled()) {
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        plot->axisRect()->setRangeDrag(axis->orientation());
        plot->axisRect()->setRangeDragAxes(plot->xAxis, axis);
    } else {
        /* No specific axis selected. Pan all the axes!
         *
         * Problem: QCustomPlot can't pan more than one set of axes. Damn.
         *
         * Solution: Let QCustomPlot pan X1,Y1 and we'll have to manually
         *           pan all the remaining Y axes. :|
         *
         */
        plot->axisRect()->setRangeDragAxes(plot->xAxis, plot->yAxis);

        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y Axis Lock must
            // be on. We'll only pan vertically.
            plot->axisRect()->setRangeDrag(Qt::Vertical);
        } else if (isAnyXAxisSelected()) {
            plot->axisRect()->setRangeDrag(Qt::Horizontal);
        } else {
            plot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
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

            mDragStartHorizRange.clear();
            foreach (QCPAxis* axis, keyAxes()) {
                QCPRange range = axis->range();
                mDragStartHorizRange[axis] = range;
            }
        }
    }
}

void BasicQCPInteractionManager::mouseMove(QMouseEvent *event){
    if (mDragging) {
        foreach(QCPAxis* axis, valueAxes()) {

            // QCustomPlot will handle RangeDrag for y1. We only need to do
            // all the others.
            if (axis == plot->yAxis)
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
        foreach(QCPAxis* axis, keyAxes()) {

            // QCustomPlot will handle RangeDrag for x1. We only need to do
            // all the others.
            if (axis == plot->xAxis)
                continue;

            // This logic is exactly the same as what can be found in
            // QCPAxisRect::mouseMoveEvent(QMouseEvent*) from QCustomPlot 1.0.0

            if (axis->scaleType() == QCPAxis::stLinear) {

              double diff = axis->pixelToCoord(mDragStart.x())
                      - axis->pixelToCoord(event->pos().x());

              axis->setRange(
                          mDragStartHorizRange[axis].lower + diff,
                          mDragStartHorizRange[axis].upper + diff);

            } else if (axis->scaleType() == QCPAxis::stLogarithmic) {
              double diff = axis->pixelToCoord(mDragStart.x())
                      / axis->pixelToCoord(event->pos().x());
              axis->setRange(
                          mDragStartHorizRange[axis].lower * diff,
                          mDragStartHorizRange[axis].upper * diff);
            }
        }
        // We shouldn't need to do a replot -
        // QCPAxisRect::mouseMoveEvent(QMouseEvent*) should fire next and
        // handle it for us.
    }
}

void BasicQCPInteractionManager::mouseRelease() {
    mDragging = false;
    // QCPAxisRect::mouseReleaseEvent(QMouseEvent *) should fire next and
    // deal with the AA stuff so we shouldn't need to here.
}

void BasicQCPInteractionManager::mouseWheel(QWheelEvent* event) {
    // Zoom on what ever axis is selected (if one is selected)
    //if (plot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))

    emit zooming();

    if (isAnyXAxisSelected() && !isXAxisLockEnabled()) {
        // A X axis is selected and axis lock is not on. So we'll just scale
        // that one axis.
        QPointer<QCPAxis> axis = keyAxisWithSelectedParts();
        plot->axisRect()->setRangeZoom(axis->orientation());
        plot->axisRect()->setRangeZoomAxes(axis, plot->yAxis);
    } else if (isAnyYAxisSelected() && !isYAxisLockEnabled()) {
        // A Y axis is selected and axis lock is not on. So we'll just scale
        // that one axis.
        QPointer<QCPAxis> axis = valueAxisWithSelectedParts();
        plot->axisRect()->setRangeZoom(axis->orientation());
        plot->axisRect()->setRangeZoomAxes(plot->xAxis, axis);
    } else {
        /* No specific axis selected. Zoom all the axes!
         *
         * Problem: QCustomPlot can't zoom multiple axes. Damn.
         *
         * Solution: Let it zoom x1 and y1 and we'll handle zooming all the
         *           other y axes manually :|
         */
        plot->axisRect()->setRangeZoomAxes(plot->xAxis, plot->yAxis);

        if (isAnyYAxisSelected()) {
            // A Y axis is selected. If we got in here then Y axis lock must
            // be on. We'll only scale vertically.
            plot->axisRect()->setRangeZoom(Qt::Vertical);
        } else if (isAnyXAxisSelected()) {
            plot->axisRect()->setRangeZoom(Qt::Horizontal);
        } else {
            plot->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
        }

        // This is the same logic as used by QCustomPlot v1.0.0 in
        // QCPAxisRect::wheelEvent(QWheelEvent*)

        // a single step delta is +/-120 usually
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        double wheelSteps = event->delta()/120.0;
#else
        double wheelSteps = event->angleDelta().y()/120.0;
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        const QPointF pos = event->pos();
#else
        const QPointF pos = event->position();
#endif

        double verticalRangeZoomFactor =
                plot->axisRect()->rangeZoomFactor(Qt::Vertical);
        double horizontalRangeZoomFactor =
                plot->axisRect()->rangeZoomFactor(Qt::Horizontal);

        // Rescale value axes
        foreach (QCPAxis* axis, valueAxes()) {
            double factor = pow(verticalRangeZoomFactor, wheelSteps);
            // We don't want to scale y1 - QCustomPlot will handle that.
            if (axis != plot->yAxis) {
                axis->scaleRange(factor,
                                 axis->pixelToCoord(pos.y()));
            }
        }

        if (!isAnyYAxisSelected()) {
            // Rescale key axes. Only do this if we're scaling all axes and
            // not just a Y axis.
            foreach (QCPAxis* axis, keyAxes()) {
                double factor = pow(horizontalRangeZoomFactor, wheelSteps);
                // We don't want to scale x1 - QCustomPlot will handle that.
                if (axis != plot->xAxis) {
                    axis->scaleRange(factor,
                                     axis->pixelToCoord(pos.x()));
                }
            }
        }

    }
}

bool BasicQCPInteractionManager::isAnyYAxisSelected() {
    bool yAxisPartSelected = false;
    foreach(QCPAxis* axis, valueAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            yAxisPartSelected = true;
    }

    return yAxisPartSelected;
}

bool BasicQCPInteractionManager::isAnyXAxisSelected() {
    bool xAxisPartSelected = false;
    foreach(QCPAxis* axis, keyAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            xAxisPartSelected = true;
    }

    return xAxisPartSelected;
}

QPointer<QCPAxis> BasicQCPInteractionManager::valueAxisWithSelectedParts() {

    foreach(QCPAxis* axis, valueAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            return axis;
    }
    return 0;
}

QPointer<QCPAxis> BasicQCPInteractionManager::keyAxisWithSelectedParts() {

    foreach(QCPAxis* axis, keyAxes()) {
        if (axis->selectedParts().testFlag(QCPAxis::spAxis) ||
                axis->selectedParts().testFlag(QCPAxis::spTickLabels))
            return axis;
    }
    return 0;
}


QList<QCPAxis*> BasicQCPInteractionManager::valueAxes() {
    return plot->axisRect()->axes(QCPAxis::atLeft | QCPAxis::atRight);
}

QList<QCPAxis*> BasicQCPInteractionManager::keyAxes() {
    return plot->axisRect()->axes(QCPAxis::atTop | QCPAxis::atBottom);
}

void BasicQCPInteractionManager::graphSelectionChanged() {
    bool selected = false;
    for(int i = 0; i < plot->plottableCount(); i++) {
        if (plot->plottable(i)->selected()) {
            selected = true;
            break;
        }
    }
    emit graphSelected(selected);
}

void BasicQCPInteractionManager::axisSelectionChanged() {
    bool keySelected = isAnyXAxisSelected();
    emit keyAxisSelected(keySelected);

    // If either x axis or its tick labels are selected, select both axes
    if (keySelected) {
        // If one part (tick labels or the actual axis) is selected, ensure
        // both are.

        if (isXAxisLockEnabled()) {
            // ALL axes should be selected
            foreach (QCPAxis* axis, keyAxes()) {
                axis->setSelectedParts(QCPAxis::spAxis |
                                       QCPAxis::spTickLabels);
            }
        } else {
            // Just ensure the axis is fully selected.
            QPointer<QCPAxis> axis = keyAxisWithSelectedParts();
            axis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        }
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
