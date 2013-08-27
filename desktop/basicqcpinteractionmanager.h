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

    QPointer<QCustomPlot> plot;
    
};

#endif // BASICQCPINTERACTIONMANAGER_H
