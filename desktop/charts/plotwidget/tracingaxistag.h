#ifndef TRACINGAXISTAG_H
#define TRACINGAXISTAG_H

#include "abstractaxistag.h"

class QCPItemTracer;

/** An axis tag that follows a QCPItemTracer. Whenever you want to refresh the
 * axis tags position just call the update() slot.
 */
class TracingAxisTag : public AbstractAxisTag
{
    Q_OBJECT
public:
    /** Constructs a new TracingAxisTag
     *
     * @param axis Axis the tag lives on
     * @param arrow If the tag should be rendered with an arrow pointing to the axis
     * @param itemTracer The QCPItemTracer the tags value will be obtained from. The TracingAxisTag
     *      does not take ownership of the QCPItemTracer.
     * @param parent Parent object.
     */
    TracingAxisTag(QCPAxis* axis, bool arrow, QCPItemTracer *itemTracer, QObject *parent = 0);

public slots:
    /** Updates the position of the axis tag based on the current position of the associated
     * QCPitemTracer.
     */
    void update();

private:
    QPointer<QCPItemTracer> tracer;

    void setCoords(double x, double y);
};

#endif // TRACINGAXISTAG_H
