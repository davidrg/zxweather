#ifndef LIVEPLOT_H
#define LIVEPLOT_H

#include "qcp/qcustomplot.h"
#include "graphstyle.h"

#include <QMap>

/* LivePlot: A QCustomPlot subclass providing some basic interactivity:
 *  - Click a plottable or legend item to select it
 *  - Double-click a plottable or legend item to restyle it
 *  - Track styles for graphs added via addStyledGraph()
 *  - Title management
 *  - Legend management including context menu
 *  - Rename graph, value axis
 *  - Graph context menu
 *  - Save, copy
 *
 * Functionality not implemented:
 *  - Multi-datasets
 *  - Rename key axis
 *  - Grid
 *  - Axis lock (because pan & zoom aren't implemented)
 *
 * Eventually this class may be renamed and used to replace a chunk of the
 * functionality provided by ChartWindow, BasicQCPInteractionManager and
 * WeatherPlotter.
 *
 * Functionality not copied from BasicQCPInteractionManager:
 *   - Zoom
 *   - Pan
 */

class LivePlot : public QCustomPlot
{
    Q_OBJECT

public:
    LivePlot(QWidget *parent);

    QCPGraph* addStyledGraph(QCPAxis *keyAxis, QCPAxis *valueAxis, GraphStyle style);

signals:
   void removingGraph(QCPGraph *graph);
   void addGraphRequested();
   void legendVisibilityChanged(bool visible);
   void titleVisibilityChanged(bool visible);
   void graphStyleChanged(QCPGraph* graph, GraphStyle& style);

public slots:
    void copy();
    void save();
    void changeSelectedGraphStyle();
    void renameSelectedGraph();
    void removeSelectedGraph();
    void addTitle();
    void toggleLegend();
    void toggleTitle();
    void setAddGraphsEnabled(bool enabled);
    void removeAllGraphs();

    void recreateDefaultAxisRect();

private slots:
    // This is implemented in BasicQCPInteractionManager for the ChartWindow stuff
    void plottableClicked(QCPAbstractPlottable* plottableItem,
                         int dataIndex,
                         QMouseEvent* event);

    // This is implemented in ChartWindow and WeatherPlotter
    void plottableDoubleClicked(QCPAbstractPlottable* plottable,
                                int /*item*/,
                                QMouseEvent* /*event*/);


    // from BasicQCPInteractionManager
    void legendClicked(QCPLegend* /*legend*/,
                       QCPAbstractLegendItem *item,
                       QMouseEvent* /*event*/);

    // From ChartWindow and WeatherPlotter
    void legendDoubleClicked(QCPLegend* /*legend*/,
                             QCPAbstractLegendItem* item,
                             QMouseEvent* /*event*/);

    void axisDoubleClicked(QCPAxis *axis, QCPAxis::SelectablePart part,
                           QMouseEvent *event);

    void textElementDoubleClick(QMouseEvent *event);

    void moveLegend();

    void changeLegendFont();

    void chartContextMenuRequested(QPoint point);

    void emitAddGraphRequested();

    void showTitleContextMenu(QPoint point);
    void changeTitleFont();
    void editTitle();

    void showAxisContextMenu(QPoint point, QCPAxis *axis);
    void changeAxisLabelFont();
    void renameSelectedAxis();
    void changeAxisTickLabelFont();

private:
    int nextId;
    QMap<int, GraphStyle> graphStyles;
    QPointer<QCPTextElement> plotTitle;
    QString plotTitleValue;
    QFont plotTitleFont;
    bool plotTitleEnabled;

    bool addGraphsEnabled;

    void changeGraphStyle(QCPGraph* graph);
    void showLegendContextMenu(QPoint point);
    void showChartContextMenu(QPoint point);

    void addTitle(QString title);
    void removeTitle(bool replot);
};

#endif // LIVEPLOT_H
