#ifndef AXISRANGEANNOTATION_H
#define AXISRANGEANNOTATION_H

#include <QObject>
#include "charts/qcp/qcustomplot.h"
#include "charts/plotwidget.h"
#include "charts/plotwidget/axistype.h"

class AxisRangeAnnotation : public QObject
{
    Q_OBJECT
public:
    explicit AxisRangeAnnotation(PlotWidget *chart, QCPAxis *valueAxis, QCPAxis *keyAxis, AxisType axisType, QObject *parent = nullptr);

    void addRangeValue(double end, QString label, QColor shadeColour, QColor lineColour);

public slots:
    void update();

private:
    typedef struct _rangeValue {
        double start;
        double end;
        QString label;
        QColor shadeColour;
        QColor lineColour;

        // This is owned by the plot. Call QCustomPlot::removeItem to delete it.
        QPointer<QCPItemLine> line;
    } range_value_t;

    QPointer<PlotWidget> *chart;
    QPointer<QCPAxis> valueAxis;
    QPointer<QCPAxis> keyAxis;
    AxisType axisType;
    double currentRangeStart;

    QList<range_value_t> rangeValues;
};

#endif // AXISRANGEANNOTATION_H
