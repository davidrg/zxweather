#ifndef WEATHERPLOT_H
#define WEATHERPLOT_H

#include "qcp/qcustomplot.h"
#include "datasource/abstractdatasource.h"
#include "graphstyle.h"
#include "cachemanager.h"

#include <QPointer>

#define GRAPH_TYPE "GraphType"
#define GRAPH_AXIS "GraphAxisType"
#define GRAPH_DATASET "GraphDataSet"

/** WeatherPlotter is responsible for plotting weather data in a QCustomPlot widget.
 *
 * Given a list of DataSets, each with a timespan and set of columns, WeatherPlotter
 * will coordinate with CacheManager to retrieive the data from an AbstractDataSource
 * instance and insert the necessary QCPGraph and QCPAxis objects into the plot.
 */
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

    /** Draws a chart containing the specified datasets. A dataset
     * specifies a set of columns along with a timespan and a unique
     * identifier.
     *
     * This wipes any existing sample cache and re-fetches the data.
     *
     * @param dataSets list of datasets to include in the chart.
     */
    void drawChart(QList<DataSet> dataSets);

    /** Adds the specified columns as graphs to the chart. If the data for
     * the columns is not available from the sample cache it will be fetched.
     *
     * @param dataSetId the dataset to add the graph to
     * @param columns Columns to add to the chart as graphs.
     */
    void addGraphs(dataset_id_t dataSetId, SampleColumns columns);

    /** Removes the graph for the specified column from the chart.
     *
     * @param dataSetId the dataset to remove the graph from
     * @param column Graph to remove.
     */
    void removeGraph(dataset_id_t dataSetId, SampleColumn column);

    /** Returns the default label for the specified axis.
     *
     * @param axis Axis to get the default label for.
     * @return Default label for the axis.
     */
    QString defaultLabelForAxis(QCPAxis* axis);

    /** Returns the set of columns *not* currently in the chart
     * for a given dataset
     *
     * @param dataSetId The dataset to get missing columns for.
     * @return Columns not currently in the chart.
     */
    SampleColumns availableColumns(dataset_id_t dataSetId);

    /** Returns true if axis grids will be visible by default.
     *
     * @return True if axis grids are visible by default.
     */
    bool axisGridVisible() const { return mAxisGridsVisible; }

    /** Returns the graph styles being used by all columns in the
     * specified dataset.
     * @param dataSetId The dataset to get graph styles for.
     * @return Graph styles for the specified dataset.
     */
    QMap<SampleColumn, GraphStyle> getGraphStyles(dataset_id_t dataSetId);

signals:
    void axisCountChanged(int count);

public slots:
    /** Changes the timespan for the specified dataset and replots the chart.
     * @param dataSetId Dataset to update.
     * @param start New start time (default is existing start)
     * @param end New end time (default is existing end)
     */
    void changeDataSetTimespan(dataset_id_t dataSetId, QDateTime start = QDateTime(), QDateTime end = QDateTime());

    /** Flushes data caches and redraws the chart. This results in the
     * datasource being requeried which may be slow depending on the number
     * of datasets and timespans involved.
     */
    void reload();

    /** Controls if axis grids should be visible by default.
     *
     * @param visible If axis grids should be visible by default.
     */
    void setAxisGridVisible(bool visible) { mAxisGridsVisible = visible; }

    /** Sets the styles for columns in the specified dataset.
     * @param styles New styles
     * @param dataSetId Dataset these apply to.
     */
    void setGraphStyles(QMap<SampleColumn, GraphStyle> styles, dataset_id_t dataSetId);

    void rescaleByTime();
    void rescaleByTimeOfYear();
    void rescaleByTimeOfDay();

private slots:    
    /** Called by the CacheManager when its finished obtaining all the
      * requested datasets.
      *
      * @param dataSets The requested datasets
      */
    void dataSetsReady(QMap<dataset_id_t, SampleSet> dataSets);

    /** Called by the CacheManager when an error was encountered.
      *
      * @param message Error message
      */
    void dataSourceError(QString message);

private:
    /*************************************
     * Top level chart drawing functions *
     *************************************/

    /** Draws a chart containing the supplied data.
    *
    * @param dataSets The datasets to include in the chart
    */
    void drawChart(QMap<dataset_id_t, SampleSet> sampleSets);

    /** Adds graphs for all of the supplied datasets to the chart.
     * @param dataSets Datasets to add graphs for.
     */
    void addGraphs(QMap<dataset_id_t, SampleSet> dataSets);

    /*************************************************************
     * Functions for adding various sorts of graphs to the plot. *
     *************************************************************/

    /** Adds a generic line graph something like temperature or humidity.
     *
     * @param dataSet Dataset this graph belongs to.
     * @param column The type of data this graph should contain.
     * @param samples The set of samples for the dataset.
     */
    void addGenericGraph(DataSet dataSet, SampleColumn column, SampleSet samples);

    /** Adds a rainfall graph to the plot.
     *
     * @param dataSet Dataset the rainfall graph is for.
     * @param samples Samples for the dataset.
     */
    void addRainfallGraph(DataSet dataSet, SampleSet samples);

    /** Adds a wind direction graph to the plot.
     *
     * @param dataSet Dataset the wind direction graph is for.
     * @param samples Samples for the dataset.
     */
    void addWindDirectionGraph(DataSet dataSet, SampleSet samples);

    typedef enum {
        RS_TIME = 0, /*!< Align on time only ignoring year, month and day */
        RS_MONTH = 1, /*!< Align on month, day and time ignoring year */
        RS_YEAR = 2 /*!< Align on exact timestamp match */
    } RescaleType;

    /** Rescales the plot aligning all x axes with the one that has the
     * largest timespan and scaling them to the same.
     *
     * y axis are scaled normally.
     *
     * If only one data set is present, chart->rescaleAxes() will be
     * called instead.
     */
    void multiRescale(RescaleType rs_type = RS_TIME);

    /*******************
     * Misc            *
     *******************/
    QMap<dataset_id_t, QMap<SampleColumn, GraphStyle> > graphStyles;

    /** Returns the samples from a sampleset for the specified column.
     *
     * @param column Column we want the samples for.
     * @param samples The full set of samples for some dataset.
     * @return The list of samples for a single column from a dataset.
     */
    QVector<double> samplesForColumn(SampleColumn column, SampleSet samples);

    /** Wrapper around a DataSource that lets us fire off a request for multiple
     * different sets of data and get all sets back in one response with that
     * response cached to make future lookups fast.
     */
    QScopedPointer<CacheManager> cacheManager;

    /*******************
     * Axis Management *
     *******************/

    /** Different types of axes. With the exception of AT_KEY there will only
     * ever be one of each axis type in the chart
     */
    typedef enum {
        AT_NONE = 0, /*!< Not a real axis. */
        AT_TEMPERATURE = 1, /*!< axis in degrees celsius */
        AT_WIND_SPEED = 2, /*!< axis in m/s */
        AT_WIND_DIRECTION = 3, /* Axis for wind direction */
        AT_PRESSURE = 4, /*!< Axis for hPa */
        AT_HUMIDITY = 5, /*!< Axis in % */
        AT_RAINFALL = 6, /*!< Axis in mm */
        AT_SOLAR_RADIATION = 7, /*!< Axis in W/m^2 */
        AT_UV_INDEX = 8, //*!< Axis for UV Index - no unit */
        AT_KEY = 100 /*!< X Axis for DataSet 0. AT_KEY+1 for DataSet 1, etc. */
    } AxisType;

    QMap<AxisType, QPointer<QCPAxis> > configuredValueAxes;
    QMap<AxisType, QPointer<QCPAxis> > configuredKeyAxes;
    QMap<QCPAxis*, AxisType> axisTypes;
    QMap<AxisType, int> axisReferences;

    /** Initialises the axisLabels dictionary.
     */
    void populateAxisLabels();

    /** Removes all axis from the plot that no longer have any associated
     * data.
     */
    void removeUnusedAxes();

    /** Labels for the different axis types
     */
    QMap<AxisType, QString> axisLabels;

    /** Creates a new value axis of the specified type.
     *
     * @param type Axis type to create.
     * @return a new axis of the specified type.
     */
    QPointer<QCPAxis> createValueAxis(AxisType type);

    /** Gets the value axis of the specified type. If it does not
     * exist it is created.
     *
     * @param axisType Axis type to fetch
     * @return Value axis of the specified type.
     */
    QPointer<QCPAxis> getValueAxis(AxisType axisType);

    /** Creates a new X axis for the specified dataset.
     *
     * @param dataSetId Dataset to create the axis for.
     * @return The new axis.
     */
    QPointer<QCPAxis> createKeyAxis(dataset_id_t dataSetId);

    /** Gets the X axis for the specified dataset. If it does not exist
     * it will be created.
     * @return X axis for the specified dataset.
     */
    QPointer<QCPAxis> getKeyAxis(dataset_id_t dataSetId);

    /** Returns the type of axis to use for a particular column type.
     * For example, the AT_TEMPERATURE axis for the SC_IndoorTemperature
     * column
     *
     * @param column Column to get the axis for.
     * @return Axis type to use.
     */
    AxisType axisTypeForColumn(SampleColumn column);

    /*******************
     * Misc            *
     *******************/

    /** These are the data sets we're dealing with, keyed by their ID.
     * Each dataset contains a timespan and a list of columns to be
     * fetched from the datasource and plotted in the chart with
     * its own X axis.
     */
    QMap<dataset_id_t,DataSet> dataSets;

    /** The minimum timestamp in each data sets sample set. Populated when
     * the graphs for each dataset are added.
     */
    QMap<dataset_id_t, QDateTime> dataSetMinimumTime;

    /** The maximum timestamp in each data sets sample set. Populated when
     * the graphs for each dataset are added.
     */
    QMap<dataset_id_t, QDateTime> dataSetMaximumTime;

    /** The QCustomPlot instance we're drawing graphs into.
     */
    QCustomPlot* chart;

    bool mAxisGridsVisible; /*!< If axis grids should be visible on creation */
};

#endif // WEATHERPLOT_H
