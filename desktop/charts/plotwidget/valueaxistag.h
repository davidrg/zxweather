#ifndef VALUEAXISTAG_H
#define VALUEAXISTAG_H

#include "tracingaxistag.h"
#include <QObject>

class PlotWidget;
class QCPGraph;

/** Like the TracingAxisTag but it has its own QCPItemTracer allowing you to just set
 * the value for the axis tag.
 *
 * It is intended for pointing to/highlighting the most recent point in a live chart. It
 * may not work for other arbitrary points in a plot.
 */
class ValueAxisTag : public TracingAxisTag
{
    Q_OBJECT
public:
    /** Constructs the axis tag against the specified axes. Both are required.
     *
     * @param keyAxis Key axis the tag is associated with
     * @param valueAxis Value axis the tag is associated with
     * @param onValueAxis If the tag should be displayed on the value axis rather than the key axis
     * @param arrow If an arrow should be shown pointing to the axis
     * @param parent Parent plot widget
     */
    ValueAxisTag(QCPAxis *keyAxis, QCPAxis *valueAxis, bool onValueAxis, bool arrow, PlotWidget *parent=0);

    /** Constructs the tag for the specified graph. It will be associated with the graphs
     * axes and drawn with the graphs pen.
     *
     * @param graph Graph to construct an axis tag for
     * @param onValueAxis If the tag should be shown on the graphs value axis rather than key axis
     * @param arrow If an arrow should be shown pointing to the axis
     * @param parent Parent plot widget
     */
    ValueAxisTag(QCPGraph *graph, bool onValueAxis, bool arrow, PlotWidget *parent=0);

public slots:
    /** Sets the coordinates for the point the axis tag should display
     *
     * @param key Key axis value.
     * @param value Value axis value.
     */
    void setValue(double key, double value);
};

#endif // VALUEAXISTAG_H
