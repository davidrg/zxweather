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

#include <QScopedPointer>

namespace Ui {
class LivePlotWindow;
}

class QCPGraph;
class QCPAxis;
class QCPMarginGroup;
class ValueAxisTag;

class LivePlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LivePlotWindow(bool solarAvailalble, hardware_type_t hardwareType,
                            ExtraColumns extraColumns,
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

    QMap<LiveValue, QCPGraph*> graphs;
    QMap<LiveValue, QCPGraph*> points;
    QMap<LiveValue, ValueAxisTag*> tags;
    QMap<LiveValue, QCPAxisRect*> axisRects;
    QSharedPointer<QCPAxisTicker> ticker;
    QCPLayoutGrid *legendLayout;

    QMap<UnitConversions::unit_t, QCPAxis*> axis;

    QMap<LiveValue, UnitConversions::unit_t> units;
    QMap<UnitConversions::unit_t, QString> axisLabels;

    int timespanMinutes;

    // Aggregation options
    bool aggregate, maxRainRate, stormRain;
    int aggregateSeconds;
    QScopedPointer<LiveDataRepeater> repeater;
    QScopedPointer<AbstractLiveAggregator> aggregator;

    bool axisTags;
    bool multipleAxisRects;
    QPointer<QCPMarginGroup> marginGroup;
};

#endif // LIVEPLOTWINDOW_H
