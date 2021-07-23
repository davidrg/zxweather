#include "axisrangeannotation.h"

AxisRangeAnnotation::AxisRangeAnnotation(PlotWidget *chart, QCPAxis *valueAxis, QCPAxis *keyAxis, AxisType axisType, QObject *parent) : QObject(parent)
{
    this->valueAxis = valueAxis;
    this->keyAxis = keyAxis;
    this->axisType = axisType;
    this->chart = chart;

    this->currentRangeStart = 0;

    // Irrometer Watermark sensor range values
    addRangeValue(10, "Saturated Soil", QColor(Qt::blue), QColor(Qt::darkBlue));
    addRangeValue(30, "Soil is adequately wet (except coarse sands, which are drying)", QColor(Qt::lightGray), QColor(Qt::darkGray));
    addRangeValue(60, "Usual range for irrigation (most soils)", QColor(Qt::blue), QColor(Qt::darkBlue));
    addRangeValue(100, "Usual range for irrigation in heavy clay", QColor(Qt::blue), QColor(Qt::darkBlue));
    addRangeValue(200, "Soil is becoming dangerously dry - proceed with caution", QColor(Qt::red), QColor(Qt::darkRed));
}


void AxisRangeAnnotation::addRangeValue(double end, QString label, QColor shadeColour, QColor lineColour) {
    double keyMin = keyAxis->range().minRange;
    double keyMax = keyAxis->range().maxRange;

    range_value_t rv;
    rv.start = currentRangeStart;
    rv.end = end;
    rv.label = label;
    rv.shadeColour = shadeColour;
    rv.lineColour = lineColour;

    rv.line = new QCPItemLine(chart);
    rv.line->setLayer("overlay");
    rv.line->setVisible(false);
    rv.line->setSelectable(false);
    rv.line->start->setType(QCPItemPosition::ptPlotCoords);
    rv.line->start->setAxes(keyAxis, valueAxis);
    rv.line->start->setCoords(keyMin, rv.start);
    rv.line->end->setType(QCPItemPosition::ptPlotCoords);
    rv.line->end->setAxes(keyAxis, valueAxis);
    rv.line->end->setCoords(keyMax, rv.start);
    rv.line->setPen(QPen(rv.lineColour));
    rv.line->setClipToAxisRect(true);


    /* TODO
     *  -> The line needs to be infinite some how (or effectively infinite)
     *       >> Could use QCPItemStraightLine instead. Problem is while its infinite
     *          its not tied to the axes so we have to manage its coordinates manually
     *       >> Could just set the X coordinates of the QCPItemLine to be really small
     *          and big.
     *  -> Provide on/off function that hides and shows everything
     *  -> Make the destructor deregister the QCPItemLine from the plot
     *  -> Labels for whats on either side of each line (above and below)
     *  -> Shading under the item line.
     *       >> QCPGraph can do this but it mixes the colours. Plus we don't really want
     *          the shaded background appearing as a graph in the legend, etc.
     *       >> QCPItemRect is next best. As long as we can have it without a border.
     *          Needs to be transparent too.
     *
     * DO WE ALSO CONSIDER:
     *   A new SampleColumn? Soil Moisture as a percentage (0% = dry, 100% = saturated).
     *   This would require a fair bit of work across the stack through. But the measure
     *   would be far more intuative as a percentage I think.
     */

    currentRangeStart = end;

    rangeValues.append(rv);
}

void AxisRangeAnnotation::update() {
    /*
     * TODO:
     *   Add and/or update QCPLineItems to the plot and shade under for the
     *   specified ranges.
     */
}
