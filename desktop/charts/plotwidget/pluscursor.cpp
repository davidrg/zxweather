#include "pluscursor.h"
#include "basicaxistag.h"
#include "charts/plotwidget.h"


/** A QCPitemLine that is transparent to clicks. If we don't do this
 *  then you can't double click on, eg, a graph while the PlusCursor
 *  is turned on.
 */
class TransparentItemLine: public QCPItemLine {
public:
    explicit TransparentItemLine(QCustomPlot *parentPlot) : QCPItemLine(parentPlot) {}

    double selectTest ( const QPointF &  pos, bool  onlySelectable, QVariant *  details = 0  ) const {
        Q_UNUSED(pos)
        Q_UNUSED(onlySelectable)
        Q_UNUSED(details)
        return -1.0;
    }
};


PlusCursor::PlusCursor(PlotWidget *plotWidget) : QObject(plotWidget)
{
    chart = plotWidget;
    connect(chart, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseMove(QMouseEvent*)));

    connect(chart, SIGNAL(mouseLeave(QEvent*)),
            this, SLOT(mouseLeave(QEvent*)));

    hCursor = new TransparentItemLine(chart);
    hCursor->setLayer("overlay");
    hCursor->setVisible(false);
    hCursor->setSelectable(false);
    hCursor->start->setType(QCPItemPosition::ptAbsolute);
    hCursor->end->setType(QCPItemPosition::ptAbsolute);

    vCursor = new TransparentItemLine(chart);
    vCursor->setLayer("overlay");
    vCursor->setVisible(false);
    vCursor->setSelectable(false);
    vCursor->start->setType(QCPItemPosition::ptAbsolute);
    vCursor->end->setType(QCPItemPosition::ptAbsolute);

    setEnabled(false);
}


void PlusCursor::setEnabled(bool enabled) {
    this->enabled = enabled;

    if (!enabled) {
        cleanup();
    }
}

bool PlusCursor::setup(QCPAxisRect *rect) {
    if (hCursor.isNull() || vCursor.isNull()) {
        qDebug() << "Setup not possible: not initialised";
        return false;
    }

    if (!enabled) {
        qDebug() << "Setup skipped: not enabled";
        return false;
    }

    if (rect == NULL) {
        qDebug() << "Setup skipped: null axis rect";
        return false;
    }

    QPointer<QCPAxis> keyAxis = getVisibleKeyAxis(rect);
    if (keyAxis == NULL) {
        qDebug() << "No key axis in chart - unable to setup!";
        return false;
    }

    QPointer<QCPAxis> valueAxis = getVisibleValueAxis(rect);
    if (valueAxis == NULL) {
        qDebug() << "No value axis in chart - unable to setup!";
        return false;
    }

    qDebug() << "PlusCursor Setup...";

    foreach (QCPAxis *axis, rect->axes()) {
        int type = -1;

        if (axis->property(AXIS_TYPE).isNull()) {
            qDebug() << "Ignoring axis with no AXIS_TYPE: " << axis->label();
            continue;
        } else {
            type = axis->property(AXIS_TYPE).toInt();
        }
        qDebug() << "Setup axis of type" << type;

        bool isValueAxis = axis->axisType() == QCPAxis::atLeft
                || axis->axisType() == QCPAxis::atRight;

        QCPAxis *k, *v;
        if (isValueAxis) {
            k = keyAxis;
            v = axis;
            valueAxes[type] = axis;
            qDebug() << "Creating value axis tag of type" << type;
        } else {
            k = axis;
            v = valueAxis;
            keyAxes[type] = axis;
            qDebug() << "Creating key axis tag of type" << type;
        }

        qDebug() << "Value Axis?" << isValueAxis << "K" << k << "V" << v;

        QPointer<BasicAxisTag> tag = new BasicAxisTag(k, v, isValueAxis, false, this);
        cursorAxisTags[type] = tag;
    }

    // Need at least one key axis and one value axis.
    if (keyAxes.count() == 0 || valueAxes.count() == 0) {
        qDebug() << "Setup failed - no typed axes!";
        cleanup();
        return false;
    }

    currentAxisRect = rect;

    vCursor->setClipAxisRect(rect);
    hCursor->setClipAxisRect(rect);
    vCursor->setClipToAxisRect(true);
    hCursor->setClipToAxisRect(true);
    vCursor->setVisible(true);
    hCursor->setVisible(true);

    // We need to trigger a full replot after making the cursor
    // visible - replotting the overlay layer alone isn't enough (the
    // cursor won't appear until the mouse moves out of the widget)
    chart->replot();

    qDebug() << "Setup complete!";
    return true;
}


/** Removes all axis tags from the chart and hides the cursor. Requests
 * a chart replot when done.
 *
 */
void PlusCursor::cleanup() {
    if (!hCursor.isNull()) {
        hCursor->setVisible(false);
    }
    if (!vCursor.isNull()) {
        vCursor->setVisible(false);
    }

    while(!cursorAxisTags.isEmpty()) {
        delete cursorAxisTags.take(cursorAxisTags.keys().first());
    }

    keyAxes.clear();
    valueAxes.clear();

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    currentAxisRect.clear();
#else
    currentAxisRect = NULL;
#endif

    chart->layer("overlay")->replot();
}

void PlusCursor::hideCursor() {
    // We used to just hide the cursor here but better to just remove it
    // from the plot entirely and add it back. This prevents stuff from
    // ending up in random positions while zooming.
    cleanup();
}

QCPAxis* PlusCursor::getVisibleKeyAxis(QCPAxisRect *rect) {
    if (rect == NULL) {
        return NULL;
    }

    foreach (QCPAxis *ka, rect->axes(QCPAxis::atBottom | QCPAxis::atTop)) {
        if (ka != NULL && ka->visible()) {
            return ka;
        }
    }
    return NULL;
}

QCPAxis* PlusCursor::getVisibleValueAxis(QCPAxisRect *rect) {
    if (rect == NULL) {
        return NULL;
    }

    foreach (QCPAxis *ka, rect->axes(QCPAxis::atLeft | QCPAxis::atRight)) {
        if (ka != NULL && ka->visible()) {
            return ka;
        }
    }
    return NULL;
}

void PlusCursor::mouseMove(QMouseEvent* event) {
    QCPAxisRect *rect = chart->axisRectAt(event->pos());

    if (!enabled) {
        cleanup();
        return;
    } else if (!chart->rect().contains(event->pos())) {
        qDebug() << "Mouse outside chart - hide";
        // Mouse has left the widget. Hide the cursor
        hideCursor();
        return;
    } else if (rect == NULL) {
        qDebug() << "Mouse not in an axis rect! cleaning up and returning.";
        cleanup();
        return;
    } else if (currentAxisRect.data() != rect) {
        qDebug() << "Mouse moved to a different axis rect - resetting";
        // Mouse has moved to a different axis rect!
        cleanup();
    }

    // If we're not setup then setup.
    if (currentAxisRect.isNull()) {
        qDebug() << "Not currently setup!";
        if (!setup(rect)) {
            qDebug() << "Setup failed.";
            return; // setup failed.
        }
    }

    //qDebug() << "Everything looks ok - drawing cursor for rect" << currentAxisRect;

    // Update the cursor
    vCursor->start->setCoords(event->pos().x(),0);
    vCursor->end->setCoords(event->pos().x(), chart->height());

    hCursor->start->setCoords(0, event->pos().y());
    hCursor->end->setCoords(chart->width(), event->pos().y());

    // update all axis tags

    double left = currentAxisRect->bottomLeft().x();
    double right = currentAxisRect->bottomRight().x();

    foreach (int type, cursorAxisTags.keys()) {
        QPointer<BasicAxisTag> tag = cursorAxisTags[type];
        if (tag.isNull()) {
            qWarning() << "Tag for axis type" << type << "is null.";
            continue;
        }

        if (valueAxes.contains(type)) {
            // Its a value axis (Y)
            QCPAxis* axis = valueAxes[type];

            // Find a visible key axis. Any will do.
            QPointer<QCPAxis> keyAxis = getVisibleKeyAxis(currentAxisRect);
            if (keyAxis.isNull()) {
                qWarning() << "No key axis for axis rect!";
                return;
            }

            double axisValue = axis->pixelToCoord(event->pos().y());

            QCPRange range = axis->range();
            if (axisValue < range.lower || axisValue > range.upper) {
                tag->setVisible(false);
            } else {
                tag->setVisible(true);

                // All this PlusCursor code was refactored out of WeatherPlotter.
                // so previously we checked if type == AT_HUMIDITY but the AxisType
                // enum is private to WeatherPlotter so we've not got access to that
                // anymore. Really we need to get rid of all the axis-type stuff from
                // here someday and make it a bit more generic.
                if (type == 5 /* AT_HUMIDITY */) {
                    tag->setText(QString::number(axisValue, 'f', 0));
                } else {
                    tag->setText(QString::number(axisValue, 'f', 1));
                }

                double leftPos = keyAxis->pixelToCoord(left - axis->offset());

                // +1 to align with axis rect border:
                double rightPos = keyAxis->pixelToCoord(right + axis->offset() + 1);

                tag->setCoords(axis->axisType() == QCPAxis::atLeft ? leftPos : rightPos,
                               axisValue);
            }
        } else {
            // Its a key axis (X)

            double axisValue = tag->axis()->pixelToCoord(event->pos().x());

            QCPRange r = tag->axis()->range();
            if (axisValue < r.lower || axisValue > r.upper) {
                tag->setVisible(false);
            } else {
                tag->setVisible(true);

#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
                QLocale locale;
                auto ts = QDateTime::fromMSecsSinceEpoch(axisValue * 1000);
                tag->setText(locale.toString(ts, locale.dateFormat(QLocale::ShortFormat)));
#else
                tag->setText(QDateTime::fromMSecsSinceEpoch(axisValue * 1000).toString(Qt::SystemLocaleShortDate));
#endif

                QPointer<QCPAxis> valueAxis = getVisibleValueAxis(currentAxisRect);
                if (valueAxis.isNull()) {
                    qDebug() << "No value axis for axis rect!";
                    return;
                }

                double valueZero = valueAxis->pixelToCoord(currentAxisRect->bottomLeft().y());
                double valueMax = valueAxis->pixelToCoord(currentAxisRect->topRight().y() -1); // -1 to align with border

                QFontMetrics m(tag->font());
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
                double halfWidth = m.horizontalAdvance(tag->text()) / 2;
#else
                double halfWidth = m.width(tag->text()) / 2;
#endif

                double minPos = tag->axis()->pixelToCoord(halfWidth + left);
                double maxPos = tag->axis()->pixelToCoord(right - halfWidth);

                // Prevent the tag from going off the end of the chart.
                double xValue = axisValue;
                if (xValue < minPos) {
                    xValue = minPos;
                } else if (xValue > maxPos) {
                    xValue = maxPos;
                }

                if (tag->axis()->axisType() == QCPAxis::atTop) {
                    // +1 to align with axis rect border
                    tag->setCoords(xValue, valueMax);
                } else {
                    tag->setCoords(xValue, valueZero);
                }
            }
        }

    }

    chart->layer("overlay")->replot();
}

void PlusCursor::mouseLeave(QEvent *event) {
    Q_UNUSED(event)

    //qDebug() << "Mouse has left the widget! Triggering cleanup.";
    cleanup();
}
