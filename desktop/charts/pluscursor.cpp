#include "pluscursor.h"

PlusCursor::PlusCursor(PlotWidget *parent): QObject(parent)
{
    chart = parent;
    connect(chart, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(updateCursor(QMouseEvent*)));
    connect(chart, SIGNAL(mouseLeave(QEvent*)),
            this, SLOT(hideCursor()));

    hCursor = new QCPItemLine(chart);
    hCursor->setLayer("overlay");
    hCursor->setVisible(false);
    hCursor->setSelectable(false);
    hCursor->start->setType(QCPItemPosition::ptAbsolute);
    hCursor->end->setType(QCPItemPosition::ptAbsolute);

    vCursor = new QCPItemLine(chart);
    vCursor->setLayer("overlay");
    vCursor->setVisible(false);
    vCursor->setSelectable(false);
    vCursor->start->setType(QCPItemPosition::ptAbsolute);
    vCursor->end->setType(QCPItemPosition::ptAbsolute);

    setCursorEnabled(true);
}

void PlusCursor::registerValueAxis(int type, QCPAxis *axis, bool atLeft) {
    QPointer<QCPItemText> tag = new QCPItemText(chart);
    tag->setLayer("overlay");
    tag->setClipToAxisRect(false);
    tag->setPadding(QMargins(3,0,3,0));
    tag->setBrush(QBrush(Qt::white));
    tag->setPen(QPen(Qt::black));
    tag->setSelectable(false);
    if (atLeft) {
        tag->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else {
        tag->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    tag->setText("0.0");
    tag->position->setAxes(chart->xAxis, axis);

    cursorAxisTags[type] = tag;

    valueAxes[type] = axis;
}

void PlusCursor::registerKeyAxis(int type, QCPAxis *axis, bool atTop) {
    QPointer<QCPItemText> tag = new QCPItemText(chart);
    tag->setLayer("overlay");
    tag->setClipToAxisRect(false);
    tag->setPadding(QMargins(3,0,3,0));
    tag->setBrush(QBrush(Qt::white));
    tag->setPen(QPen(Qt::black));
    tag->setSelectable(false);
    if (atTop) {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    } else {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }

    tag->setText("0.0");
    tag->position->setAxes(axis, chart->yAxis);

    cursorAxisTags[type] = tag;

    keyAxes[type] = axis;
}

void PlusCursor::unregisterAxis(int type, QCPAxis *axis) {
    if (cursorAxisTags.contains(type)) {
        QPointer<QCPItemText> tag = cursorAxisTags[type];
        if (!tag.isNull()) {
            chart->removeItem(tag.data());
            qDebug() << "Tag for axis" << type << "is null?" << tag.isNull();
        }
        cursorAxisTags.remove(type);
    }

    if (keyAxes.contains(type)) {
        keyAxes.remove(type);
    } else if (valueAxes.contains(type)) {
        valueAxes.remove(type);
    }
}


void PlusCursor::setEnabled(bool enabled) {
    this->cursorEnabled = enabled;

    if (!enabled) {
        hideCursor();
    }
}

bool PlusCursor::isEnabled() {
    return cursorEnabled;
}

void PlusCursor::hideCursor() {
    if (!hCursor.isNull()) {
        hCursor->setVisible(false);
    }
    if (!vCursor.isNull()) {
        vCursor->setVisible(false);
    }

    foreach (int type, cursorAxisTags.keys()) {
        if (!cursorAxisTags[type].isNull()) {
            cursorAxisTags[type]->setVisible(false);
        }
    }

    chart->layer("overlay")->replot();
}

void PlusCursor::updateCursor(QMouseEvent *event) {

    if (!this->cursorEnabled) {
        return;
    }

    if (hCursor.isNull() || vCursor.isNull()) {
        return; // Cursor not initialised.
    }

    if (keyAxes.isEmpty() || valueAxes.isEmpty()) {
        hCursor->setVisible(false);
        vCursor->setVisible(false);
        return; // There shouldn't be any graphs when there are no key or value axes.
    }

    if (!chart->rect().contains(event->pos())) {
        // Mouse has left the widget. Hide the cursor
        hideCursor();
        return;
    }

    // Update the cursor
    vCursor->start->setCoords(event->pos().x(),0);
    vCursor->end->setCoords(event->pos().x(), chart->height());
    vCursor->setVisible(true);

    hCursor->start->setCoords(0, event->pos().y());
    hCursor->end->setCoords(chart->width(), event->pos().y());
    hCursor->setVisible(true);

    // Update the tags
    foreach (int type, cursorAxisTags.keys()) {
        QPointer<QCPItemText> tag = cursorAxisTags[type];
        if (tag.isNull()) {
            qWarning() << "Tag for axis type" << type << "is null.";
            continue;
        }

        if (valueAxes.contains(type)) {
            // Its a value axis (Y)
            QCPAxis* axis = valueAxes[type];

            QPointer<QCPAxis> keyAxis = tag->position->keyAxis();

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

        } else {
            // Its a key axis (X)

            QCPAxis* axis = keyAxes[type];

            double axisValue = axis->pixelToCoord(event->pos().x());

            QCPRange r = axis->range();
            if (axisValue < r.lower || axisValue > r.upper) {
                tag->setVisible(false);
            } else {
                tag->setVisible(true);

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
        }

    }

    chart->layer("overlay")->replot();
}
