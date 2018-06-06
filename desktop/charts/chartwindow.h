#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <QList>
#include <QPointer>
#include <QDateTime>
#include <QColor>
#include <QSet>

#include "chartoptionsdialog.h"
#include "datasource/webdatasource.h"
#include "datasource/abstractlivedatasource.h"
#include "qcp/qcustomplot.h"
#include "basicqcpinteractionmanager.h"
#include "weatherplotter.h"
#include "datasetsdialog.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ChartWindow(QList<DataSet> dataSets,
                         bool solarAvailable,
                         bool isWireless,
                         QWidget *parent = 0);
    ~ChartWindow();
    
private slots:
    void chartAxisCountChanged(int count);
    void dataSetRemoved(dataset_id_t dataSetId);

    // chart slots
    void axisDoubleClick(QCPAxis* axis,
                         QCPAxis::SelectablePart part,
                         QMouseEvent* event);
    void textElementDoubleClick(QMouseEvent*event);
    void plottableDoubleClick(QCPAbstractPlottable* plottable,
                              int dataIndex,
                              QMouseEvent* event);
    void legendDoubleClick(QCPLegend* /*legend*/, QCPAbstractLegendItem* item,
                           QMouseEvent* /*event*/);

    void setYAxisLock();
    void toggleYAxisLock();
    void setXAxisLock();
    void toggleXAxisLock();

#ifdef FEATURE_PLUS_CURSOR
    void toggleCursor();
#endif


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
    void hideSelectedKeyAxis();
    void changeSelectedKeyAxisTimespan();
    void removeSelectedKeyAxis();
    void removeDataSet(dataset_id_t dsId);

    void showDataSetsWindow();

    void refresh();

    // Save As slot
    void save();
    void copy();

    void setDataSetAxisVisibility(dataset_id_t dsId, bool visible);
    void setDataSetVisibility(dataset_id_t dsId, bool visible);
    void setDataSetName(dataset_id_t dsId, QString name);
    void selectDataSet(dataset_id_t dsId);
    void changeDataSetTimeSpan(dataset_id_t dsId);
    void changeDataSetTimeSpan(dataset_id_t dsId, QDateTime start, QDateTime end);
    void showAddGraph(dataset_id_t dsId);

signals:
    void axisVisibilityChangedForDataSet(dataset_id_t dataSetId, bool visible);
    void dataSetVisibilityChanged(dataset_id_t dataSetId, bool visible);
    void dataSetAdded(DataSet ds, QString name);
    void dataSetWasRemoved(dataset_id_t dsId);
    void dataSetRenamed(dataset_id_t dsId, QString name);
    void dataSetTimeSpanChanged(dataset_id_t dsId, QDateTime start, QDateTime end);

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
    QPointer<QCPTextElement> plotTitle;
    QString plotTitleValue;
    bool plotTitleEnabled;
    QColor plotTitleColour;

    QBrush plotBackgroundBrush;

    QScopedPointer<BasicQCPInteractionManager> basicInteractionManager;
    QScopedPointer<WeatherPlotter> plotter;

    QList<DataSet> dataSets;
    dataset_id_t nextDataSetId;
    QSet<dataset_id_t> hiddenDataSets;

    QScopedPointer<DataSetsDialog> dds;

    bool solarDataAvailable;
    bool isWireless;
    hardware_type_t hw_type;
};

#endif // CHARTWINDOW_H
