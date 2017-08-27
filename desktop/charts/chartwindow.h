#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>
#include <QPointer>
#include <QDateTime>
#include <QColor>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"
#include "datasource/abstractlivedatasource.h"
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
    explicit ChartWindow(QList<DataSet> dataSets,
                         bool solarAvailable,
                         QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void chartAxisCountChanged(int count);
    void dataSetRemoved(dataset_id_t dataSetId);

    // chart slots
    void axisDoubleClick(QCPAxis* axis,
                         QCPAxis::SelectablePart part,
                         QMouseEvent* event);
    void titleDoubleClick(QMouseEvent*event, QCPPlotTitle*title);
    void plottableDoubleClick(QCPAbstractPlottable* plottable,
                              QMouseEvent* event);
    void legendDoubleClick(QCPLegend* /*legend*/, QCPAbstractLegendItem* item,
                           QMouseEvent* /*event*/);

    void setYAxisLock();
    void toggleYAxisLock();
    void setXAxisLock();
    void toggleXAxisLock();

    // Context menu related stuff
    void chartContextMenuRequested(QPoint point);
    void addTitle();
    void removeTitle(bool replot=true);
    void showLegendToggle();
    void showTitleToggle();
    void showGridToggle();
    void moveLegend();
    void removeSelectedGraph();
    void renameSelectedGraph();
    void changeSelectedGraphStyle();
    void addGraph();
    void customiseChart();
    void addDataSet();
    void renameSelectedKeyAxis();
    void changeSelectedKeyAxisTimespan();
    void removeSelectedKeyAxis();

    void refresh();

    // Save As slot
    void save();

private:
    void showLegendContextMenu(QPoint point);
    void showChartContextMenu(QPoint point);
    void showKeyAxisContextMenu(QPoint point, QCPAxis* axis);
    void showValueAxisContextMenu(QPoint point, QCPAxis* axis);

    void changeGraphStyle(QCPGraph* graph);

    QList<QCPAxis*> valueAxes();

    void reloadDataSets(bool rebuildChart);

    bool gridVisible;


    Ui::ChartWindow *ui;

    void addTitle(QString title);
    QPointer<QCPPlotTitle> plotTitle;
    QString plotTitleValue;
    bool plotTitleEnabled;
    QColor plotTitleColour;

    QBrush plotBackgroundBrush;

    QScopedPointer<BasicQCPInteractionManager> basicInteractionManager;
    QScopedPointer<WeatherPlotter> plotter;

    QList<DataSet> dataSets;

    bool solarDataAvailable;
    hardware_type_t hw_type;
};

#endif // CHARTWINDOW_H
