#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>
#include <QPointer>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"
#include "qcp/qcustomplot.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChartWindow(SampleColumns columns,
                         QDateTime startTime,
                         QDateTime endTime,
                         QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void refresh();
    void samplesReady(SampleSet samples);
    void samplesError(QString message);

    // chart slots
    void mousePress(QMouseEvent* event);
    void mouseMove(QMouseEvent* event);
    void mouseRelease();
    void mouseWheel(QWheelEvent *event);
    void selectionChanged();
    void axisDoubleClick(QCPAxis* axis,
                         QCPAxis::SelectablePart part,
                         QMouseEvent* event);
    void titleDoubleClick(QMouseEvent*event, QCPPlotTitle*title);
    void axisLockToggled();

    void chartContextMenuRequested(QPoint point);
    void addTitle();
    void removeTitle();

    // Save As slot
    void save();

private:

    typedef enum {
        AT_TEMPERATURE,
        AT_WIND_SPEED,
        AT_WIND_DIRECTION,
        AT_PRESSURE,
        AT_HUMIDITY,
        AT_RAINFALL
    } AxisType;

    QMap<AxisType, QString> axisLabels;
    QMap<AxisType, QPointer<QCPAxis> > configuredAxes;
    QMap<QCPAxis*, AxisType> axisTypes;

    void populateAxisLabels();
    QPointer<QCPAxis> createAxis(AxisType type);
    QPointer<QCPAxis> getValueAxis(AxisType axisType);

    bool yScaleLock;

    bool isAnyYAxisSelected();
    QPointer<QCPAxis> valueAxisWithSelectedParts();

    bool isYAxisLockOn();

    // For manually implementing RangeDrag on any additional independent
    // Y axes:
    QPoint mDragStart;
    bool mDragging;
    QMap<AxisType, QCPRange> mDragStartVertRange;

    Ui::ChartWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;
    SampleColumns columns;

    QPointer<QCPPlotTitle> plotTitle;
};

#endif // CHARTWINDOW_H
