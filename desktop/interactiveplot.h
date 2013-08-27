#ifndef INTERACTIVEPLOT_H
#define INTERACTIVEPLOT_H

#include "qcp/qcustomplot.h"
#include <QPointer>

class InteractivePlot : public QCustomPlot
{
    Q_OBJECT

    Q_PROPERTY(bool yAxisLock READ isYAxisLockEnabled WRITE setYAxisLockEnabled)

public:
    explicit InteractivePlot(QWidget *parent = 0);   

    bool isYAxisLockEnabled() const { return yAxisLock; }
    
public slots:
    void setYAxisLockEnabled(bool enabled) { yAxisLock = enabled;}
    
private slots:
    void mousePress(QMouseEvent* event);
    void mouseMove(QMouseEvent* event);
    void mouseRelease();
    void mouseWheel(QWheelEvent *event);

    void axisSelectionChanged();

private:
    bool isAnyYAxisSelected();
    QPointer<QCPAxis> valueAxisWithSelectedParts();
    QList<QCPAxis*> valueAxes();

    // Axis Lock
    bool yAxisLock;

    // Handles panning
    QPoint mDragStart;
    bool mDragging;
    QMap<QCPAxis*, QCPRange> mDragStartVertRange;
};

#endif // INTERACTIVEPLOT_H
