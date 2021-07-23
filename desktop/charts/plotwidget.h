#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include "qcp/qcustomplot.h"

#define AXIS_TYPE "AxisType"

class PlotWidget: public QCustomPlot
{
    Q_OBJECT

public:
    PlotWidget(QWidget *parent=0);

signals:
    void mouseLeave(QEvent *event);

protected:
    virtual void leaveEvent(QEvent *event);
};

#endif // PLOTWIDGET_H
