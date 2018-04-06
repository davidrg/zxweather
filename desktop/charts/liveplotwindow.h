#ifndef LIVEPLOTWINDOW_H
#define LIVEPLOTWINDOW_H

#include <QMainWindow>
#include <QFlag>
#include <QMap>

#include "datasource/abstractlivedatasource.h"
#include "unit_conversions.h"
#include "abstractliveaggregator.h"
#include "livedatarepeater.h"

#include <QScopedPointer>

namespace Ui {
class LivePlotWindow;
}

class QCPGraph;
class QCPAxis;

class LivePlotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LivePlotWindow(bool solarAvailalble, hardware_type_t hardwareType,
                            QWidget *parent = 0);
    ~LivePlotWindow();

    LiveValues liveValues();
    void addLiveValue(LiveValue v);

private slots:
    void liveData(LiveDataSet ds);

    void showAddGraphDialog(QString message = QString(), QString title = QString());

    void graphRemoving(QCPGraph *graph);

    void selectionChanged();

    void showOptions();

private:
    void updateGraph(LiveValue type, double key, double range, double value);

    Ui::LivePlotWindow *ui;
    AbstractLiveDataSource *ds;

    LiveValues valuesToShow;
    hardware_type_t hwType;
    bool solarAvailable;

    QMap<LiveValue, QCPGraph*> graphs;
    QMap<LiveValue, QCPGraph*> points;
    QMap<UnitConversions::unit_t, QCPAxis*> axis;

    QMap<LiveValue, UnitConversions::unit_t> units;
    QMap<UnitConversions::unit_t, QString> axisLabels;

    int timespanMinutes;

    // Aggregation options
    bool aggregate, maxRainRate, stormRain;
    int aggregateSeconds;
    QScopedPointer<LiveDataRepeater> repeater;
    QScopedPointer<AbstractLiveAggregator> aggregator;

};

#endif // LIVEPLOTWINDOW_H
