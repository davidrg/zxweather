#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>
#include <QPointer>
#include <QDateTime>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"
#include "qcp/qcustomplot.h"
#include "basicqcpinteractionmanager.h"
#include "weatherplotter.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChartWindow(SampleColumns currentChartColumns,
                         QDateTime startTime,
                         QDateTime endTime,
                         QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void chartAxisCountChanged(int count);

    // chart slots
    void axisDoubleClick(QCPAxis* axis,
                         QCPAxis::SelectablePart part,
                         QMouseEvent* event);
    void titleDoubleClick(QMouseEvent*event, QCPPlotTitle*title);

    void setYAxisLock();

    // Context menu related stuff
    void chartContextMenuRequested(QPoint point);
    void addTitle();
    void removeTitle();
    void showLegendToggle();
    void showTitleToggle();
    void moveLegend();
    void removeSelectedGraph();
    void addGraph();

    // Save As slot
    void save();

private:
    void showLegendContextMenu(QPoint point);

    Ui::ChartWindow *ui;

    QPointer<QCPPlotTitle> plotTitle;
    QString plotTitleValue;

    QScopedPointer<BasicQCPInteractionManager> basicInteractionManager;
    QScopedPointer<WeatherPlotter> plotter;
};

#endif // CHARTWINDOW_H
