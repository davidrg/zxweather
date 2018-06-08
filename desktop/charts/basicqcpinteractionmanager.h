#ifndef BASICQCPINTERACTIONMANAGER_H
#define BASICQCPINTERACTIONMANAGER_H

#include <QObject>
#include <QPointer>
#include "qcp/qcustomplot.h"

class BasicQCPInteractionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool yAxisLock READ isYAxisLockEnabled WRITE setYAxisLockEnabled)
public:
    explicit BasicQCPInteractionManager(QCustomPlot* plot, QObject *parent = 0);
    
    bool isYAxisLockEnabled() const { return yAxisLock; }
    bool isXAxisLockEnabled() const { return xAxisLock; }

signals:
    graphSelected(bool);
    keyAxisSelected(bool);
    zooming();

public slots:
    void setYAxisLockEnabled(bool enabled) { yAxisLock = enabled;}
    void setXAxisLockEnabled(bool enabled) { xAxisLock = enabled;}

private slots:
    void mousePress(QMouseEvent* event);
    void mouseMove(QMouseEvent* event);
    void mouseRelease();
    void mouseWheel(QWheelEvent *event);

    void axisSelectionChanged();
    void graphSelectionChanged();

    void legendClick(QCPLegend* legend,
                     QCPAbstractLegendItem* item,
                     QMouseEvent* event);
    void plottableClick(QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent* event);

private:
    bool isAnyYAxisSelected();
    bool isAnyXAxisSelected();
    QPointer<QCPAxis> valueAxisWithSelectedParts();
    QPointer<QCPAxis> keyAxisWithSelectedParts();
    QList<QCPAxis*> valueAxes();
    QList<QCPAxis*> keyAxes();

    // Axis Lock
    bool yAxisLock;
    bool xAxisLock;

    // Handles panning
    QPoint mDragStart;
    bool mDragging;
    QMap<QCPAxis*, QCPRange> mDragStartVertRange;
    QMap<QCPAxis*, QCPRange> mDragStartHorizRange;

    QPointer<QCustomPlot> plot;
    
};

#endif // BASICQCPINTERACTIONMANAGER_H
