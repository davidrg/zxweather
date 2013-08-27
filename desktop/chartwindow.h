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
    void reload();
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
    void drawChart(SampleSet samples);

    void requestData(SampleColumns columns,
                     bool merge = false,
                     QDateTime start = QDateTime(),
                     QDateTime end = QDateTime());

    // Methods which handle the individual graph types.
    void addTemperatureGraph(SampleSet samples);
    void addIndoorTemperatureGraph(SampleSet samples);
    void addApparentTemperatureGraph(SampleSet samples);
    void addDewPointGraph(SampleSet samples);
    void addWindChillGraph(SampleSet samples);
    void addHumidityGraph(SampleSet samples);
    void addIndoorHumidityGraph(SampleSet samples);
    void addPressureGraph(SampleSet samples);
    void addRainfallGraph(SampleSet samples);
    void addAverageWindSpeedGraph(SampleSet samples);
    void addGustWindSpeedGraph(SampleSet samples);
    void addWindDirectionGraph(SampleSet samples);

    void addGraphs(SampleColumns currentChartColumns, SampleSet samples);

    void mergeSampleSet(SampleSet samples, SampleColumns columns);


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
    QMap<AxisType, int> axisReferences;

    void populateAxisLabels();
    QPointer<QCPAxis> createAxis(AxisType type);
    QPointer<QCPAxis> getValueAxis(AxisType axisType);

    bool yScaleLock;

    bool isAnyYAxisSelected();
    QPointer<QCPAxis> valueAxisWithSelectedParts();

    bool isYAxisLockOn();

    void showLegendContextMenu(QPoint point);

    void removeUnusedAxes();

    SampleColumns availableColumns();

    // For manually implementing RangeDrag on any additional independent
    // Y axes:
    QPoint mDragStart;
    bool mDragging;
    QMap<AxisType, QCPRange> mDragStartVertRange;

    Ui::ChartWindow *ui;
    QScopedPointer<AbstractDataSource> dataSource;

    // Columns currently displayed in the chart.
    SampleColumns currentChartColumns;

    // Columns available in the sample cache.
    SampleColumns dataSetColumns;

    QPointer<QCPPlotTitle> plotTitle;
    QString plotTitleValue;

    SampleSet sampleCache;
    QDateTime startTime;
    QDateTime endTime;
    bool mergeSamples;
    SampleColumns mergeColumns;
};

#endif // CHARTWINDOW_H
