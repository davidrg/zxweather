#ifndef WEATHERPLOT_H
#define WEATHERPLOT_H

#include "qcp/qcustomplot.h"
#include "datasource/abstractdatasource.h"

#include <QPointer>

#define GRAPH_TYPE "GraphType"
#define GRAPH_AXIS "GraphAxisType"

class WeatherPlotter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool axisGridVisible READ axisGridVisible WRITE setAxisGridVisible)

public:
    explicit WeatherPlotter(QCustomPlot* chart, QObject *parent = 0);
    
    /** Sets the data source to use. All data required for drawing charts will
     * be retrieved using the specified data source.
     *
     * @param dataSource Data source to fetch chart data from.
     */
    void setDataSource(AbstractDataSource *dataSource);

    /** Draws a chart covering the specified timespan including the specified
     * columns as individual graphs within the chart.
     *
     * This wipes any existing sample cache and re-fetches the data.
     *
     * @param columns Data columns (graphs) to include.
     * @param startTime Start time for the chart.
     * @param endTime End time for the chart.
     */
    void drawChart(SampleColumns columns, QDateTime startTime,
                   QDateTime endTime);

    /** Adds the specified columns as graphs to the chart. If the data for
     * the columns is not available from the sample cache it will be fetched.
     *
     * @param columns Columns to add to the chart as graphs.
     */
    void addGraphs(SampleColumns columns);

    /** Removes the graph for the specified column from the chart.
     *
     * @param column Graph to remove.
     */
    void removeGraph(SampleColumn column);

    /** Returns the default label for the specified axis.
     *
     * @param axis Axis to get the default label for.
     * @return Default label for the axis.
     */
    QString defaultLabelForAxis(QCPAxis* axis);

    /** Returns the set of columns *not* currently in the chart.
     *
     * @return Columns not currently in the chart.
     */
    SampleColumns availableColumns();

    /** Returns true if axis grids will be visible by default.
     *
     * @return True if axis grids are visible by default.
     */
    bool axisGridVisible() const { return mAxisGridsVisible; }

signals:
    void axisCountChanged(int count);

public slots:
    void refresh(QDateTime start = QDateTime(), QDateTime end = QDateTime());
    void reload();

    /** Controls if axis grids should be visible by default.
     *
     * @param visible If axis grids should be visible by default.
     */
    void setAxisGridVisible(bool visible) { mAxisGridsVisible = visible; }

private slots:
    void samplesReady(SampleSet samples);
    void samplesError(QString message);

private:
    void drawChart(SampleSet samples);

    /** Requests data from the data source and sets up the charts state to
     * handle the data sources response (so that merging and chart re-drawing
     * happens).
     *
     * @param columns Columns to request.
     * @param merge If the data should be merged with the existing cached
     *              data when it arrives or not.
     * @param start Start time for the required data set. Must match the
     *              start time of the cached data set if merge is True. The
     *              default is to use the previous start time.
     * @param end End time for the required data set. Must match the end time
     *            of the cached data set if merge is True. The default is to
     *            use the previous end time.
     */
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

    void removeUnusedAxes();



    QScopedPointer<AbstractDataSource> dataSource;


    typedef enum {
        AT_TEMPERATURE,
        AT_WIND_SPEED,
        AT_WIND_DIRECTION,
        AT_PRESSURE,
        AT_HUMIDITY,
        AT_RAINFALL
    } AxisType;

    QMap<AxisType, QPointer<QCPAxis> > configuredAxes;
    QMap<QCPAxis*, AxisType> axisTypes;
    QMap<AxisType, int> axisReferences;

    /** Initialises the axisLabels dictionary.
     */
    void populateAxisLabels();

    /** Labels for the different axis types
     */
    QMap<AxisType, QString> axisLabels;

    QPointer<QCPAxis> createAxis(AxisType type);
    QPointer<QCPAxis> getValueAxis(AxisType axisType);

    // Columns currently displayed in the chart.
    SampleColumns currentChartColumns;

    // For managing the sample cache
    SampleSet sampleCache;
    QDateTime startTime;
    QDateTime endTime;
    bool mergeSamples;
    SampleColumns mergeColumns;
    // Columns available in the sample cache.
    SampleColumns dataSetColumns;

    QCustomPlot* chart;

    bool mAxisGridsVisible; /*!< If axis grids should be visible on creation */
};

#endif // WEATHERPLOT_H
