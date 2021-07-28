#ifndef LIVEPLOTWINDOW_H
#define LIVEPLOTWINDOW_H

#include <QMainWindow>
#include <QFlag>
#include <QMap>

#include "datasource/abstractlivedatasource.h"
#include "unit_conversions.h"
#include "abstractliveaggregator.h"
#include "livedatarepeater.h"
#include "graphstyle.h"
#include "charts/plotwidget/axistype.h"

#include <QScopedPointer>

namespace Ui {
class LivePlotWindow;
}

class QCPGraph;
class QCPAxis;
class QCPMarginGroup;
class ValueAxisTag;
class PlusCursor;
class ChartMouseTracker;

class LivePlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LivePlotWindow(LiveValues initialGraphs, bool solarAvailalble,
                            hardware_type_t hardwareType, ExtraColumns extraColumns,
                            QMap<ExtraColumn, QString> extraColumnNames,
                            QWidget *parent = 0);
    ~LivePlotWindow();

    LiveValues liveValues();
    void addLiveValue(LiveValue v);

private slots:
    void liveData(LiveDataSet ds);

    void showAddGraphDialog(QString message = QString(), QString title = QString());

    void graphRemoving(QCPGraph *graph);
    void graphStyleChanged(QCPGraph *graph, GraphStyle& newStyle);

    void selectionChanged();

    void showOptions();

    void legendVisibilityChanged(bool visible);

    void toggleCursor();
    void setMouseTrackingEnabled(bool enabled);

private:
    void updateGraph(LiveValue type, double key, double range, double value);
    void addLiveValues(LiveValues columns);
    void ensureLegend(bool show=false);
    void moveLegendToBottom();
    void resetPlot();
    void resetData();

    bool axisRectExists(LiveValue type);
    QCPAxisRect* createAxisRectForGraph(LiveValue type);
    QCPAxisRect* axisRectForGraph(LiveValue type);
    QCPAxis* keyAxisForGraph(LiveValue type);
    QCPAxis* valueAxisForGraph(LiveValue type);

    Ui::LivePlotWindow *ui;
    AbstractLiveDataSource *ds;

    LiveValues valuesToShow;
    hardware_type_t hwType;
    bool solarAvailable;
    ExtraColumns extraColumns;
    QMap<ExtraColumn, QString> extraColumnNames;

    bool imperial;
    bool kmh;

    QMap<LiveValue, QPointer<QCPGraph> > graphs;
    QMap<LiveValue, QPointer<QCPGraph> > points;
    QMap<LiveValue, QPointer<ValueAxisTag> > tags;
    QMap<LiveValue, QPointer<QCPAxisRect> > axisRects;
    QSharedPointer<QCPAxisTicker> ticker;
    QPointer<QCPLayoutGrid> legendLayout;

    QMap<UnitConversions::unit_t, QPointer<QCPAxis>> axis;

    QMap<LiveValue, UnitConversions::unit_t> units;
    QMap<LiveValue, QString> valueNames;
    QMap<LiveValue, ExtraColumn> extraColumnMapping;
    QMap<LiveValue, AxisType> axisTypes;
    QMap<UnitConversions::unit_t, QString> axisLabelUnitSuffixes;
    QMap<UnitConversions::unit_t, QString> axisLabels;
    QString axisLabel(LiveValue value);

    int timespanMinutes;

    // Aggregation options
    bool aggregate, maxRainRate, stormRain;
    int aggregateSeconds;
    QScopedPointer<LiveDataRepeater> repeater;
    QScopedPointer<AbstractLiveAggregator> aggregator;

    bool axisTags;
    bool multipleAxisRects;
    QPointer<QCPMarginGroup> marginGroup;

    QPointer<ChartMouseTracker> mouseTracker;
    QPointer<PlusCursor> plusCursor;
};

#endif // LIVEPLOTWINDOW_H
