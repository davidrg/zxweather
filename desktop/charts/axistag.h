#ifndef AXISTAG_H
#define AXISTAG_H

#include <QObject>
#include <QPointer>

#include "qcp/qcustomplot.h"
#include "graphstyle.h"

class AxisTag : public QObject
{
    Q_OBJECT
public:
    explicit AxisTag(QCPAxis* axis, QObject *parent = 0);
    ~AxisTag();

    void setStyle(const GraphStyle &style);

public slots:
    void setValue(double value);

private:
    QCPAxis *axis;
    QPointer<QCPItemTracer> tracer;
    QPointer<QCPItemLine> arrow;
    QPointer<QCPItemText> label;

    char format;
    int precision;
};

#endif // AXISTAG_H
