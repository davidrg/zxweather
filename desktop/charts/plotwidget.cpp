#include "plotwidget.h"

PlotWidget::PlotWidget(QWidget *parent): QCustomPlot(parent)
{

}


void PlotWidget::leaveEvent(QEvent *event) {
    emit mouseLeave(event);

    QCustomPlot::leaveEvent(event);
}
