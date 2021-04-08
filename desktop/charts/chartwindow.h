#ifndef CHARTWINDOW_H
#define CHARTWINDOW_H

#include <QMainWindow>
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

#include "plotwidget/chartmousetracker.h"

namespace Ui {
class ChartWindow;
}

class ChartWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit ChartWindow(QList<DataSet> dataSets,
                         bool solarAvailable,
                         bool isWireless,
                         QWidget *parent = 0);
    ~ChartWindow();
    
protected:
    virtual void closeEvent(QCloseEvent *event);

private slots:
    void chartAxisCountChanged(int valueAxes, int keyAxes);
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
    void toggleTitle();

#ifdef FEATURE_PLUS_CURSOR
    void toggleCursor();
#endif


    // Context menu related stuff
    void chartContextMenuRequested(QPoint point);
    void addTitle();
    void removeTitle();
    void editTitle();
    void showLegendToggle();
    void showTitleToggle();
    void showGridToggle();
    void moveLegend();
    void changeLegendFont();
    void removeSelectedGraph();
    void renameSelectedGraph();
    void changeSelectedGraphStyle();
    void addGraph();
    void addDataSet();
    void renameSelectedKeyAxis();
    void renameSelectedValueAxis();
    void hideSelectedKeyAxis();
    void changeSelectedKeyAxisTimespan();
    void refreshSelectedKeyAxis();
    void removeSelectedKeyAxis();
    void removeDataSet(dataset_id_t dsId);
    void changeTitleFont();
    void changeAxisLabelFont();
    void changeAxisTickLabelFont();

    void showDataSetsWindow();

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
    void setSelectedKeyAxisTickFormat();
    void setSelectedKeyAxisTickFormat(dataset_id_t dsId);

    void setGraphActionsEnabled(bool enabled);
    void setDataSetActionsEnabled(bool enabled);

    void setMouseTrackingEnabled(bool enabled);

signals:
    void axisVisibilityChangedForDataSet(dataset_id_t dataSetId, bool visible);
    void dataSetVisibilityChanged(dataset_id_t dataSetId, bool visible);
    void dataSetAdded(DataSet ds, QString name);
    void dataSetWasRemoved(dataset_id_t dsId);
    void dataSetRenamed(dataset_id_t dsId, QString name);
    void dataSetTimeSpanChanged(dataset_id_t dsId, QDateTime start, QDateTime end);

private:
    void showLegendContextMenu(QPoint point);
    void showTitleContextMenu(QPoint point);
    void showChartContextMenu(QPoint point);
    void showKeyAxisContextMenu(QPoint point, QCPAxis* axis);
    void showValueAxisContextMenu(QPoint point, QCPAxis* axis);

    dataset_id_t getSelectedDataset();

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
    QFont plotTitleFont;

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
    ExtraColumns extraColumns;
    QMap<ExtraColumn, QString> extraColumnNames;

    ChartMouseTracker *mouseTracker;
};

#endif // CHARTWINDOW_H
