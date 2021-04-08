#ifndef WEATHERPLOT_H
#define WEATHERPLOT_H

#include "plotwidget.h"
#include "datasource/abstractdatasource.h"
#include "graphstyle.h"
#include "cachemanager.h"
#include "pluscursor.h"

#include <QPointer>

#define COLUMN_TYPE "ColumnType"
#define GRAPH_TYPE "GraphType"
#define GRAPH_AXIS "GraphAxisType"
#define GRAPH_DATASET "GraphDataSet"
#define AXIS_DATASET "AxisDataSet"

#define FEATURE_PLUS_CURSOR

typedef struct _graph_styles {
    QMap<StandardColumn, GraphStyle> standardStyles;
    QMap<ExtraColumn, GraphStyle> extraStyles;
} graph_styles_t;

typedef enum _key_axis_tick_format {
    KATF_Default,
    KATF_DefaultNoYear,
    KATF_Time,
    KATF_Date,
    KATF_Custom
} key_axis_tick_format_t;


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
    explicit WeatherPlotter(PlotWidget* chart, QObject *parent = 0);
    ~WeatherPlotter();
    
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

    /** Adds a new dataset to the plot.
     *
     * @param dataSet Data set to add
     */
    void addDataSet(DataSet dataSet);

    /** Removes the graph for the specified column from the chart.
     *
     * @param dataSetId the dataset to remove the graph from
     * @param column Graph to remove.
     */
    void removeGraph(dataset_id_t dataSetId, StandardColumn column);
    void removeGraph(dataset_id_t dataSetId, ExtraColumn column);

    /** Removes multiple graphs from the chart
     *
     * @param dataSetId Dataset to remove graphs for
     * @param columns Graphs to remove
     */
    void removeGraphs(dataset_id_t dataSetId, SampleColumns columns);

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

    /** Returns the columns currently in the chart for a given dataset.
     *
     * @param dataSetId The data set to get columns for
     * @return Columns currently in the chart.
     */
    SampleColumns selectedColumns(dataset_id_t dataSetId);

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
    graph_styles_t getGraphStyles(dataset_id_t dataSetId);

    /** Sets the tick format for the specified key axis.
     *
     * @param dataSetId Key axis to set the format for.
     * @param format New tick format
     * @param customFormat custom format string if tick format is KATF_Custom
     */
    void setKeyAxisFormat(dataset_id_t dataSetId, key_axis_tick_format_t format, QString customFormat);
    key_axis_tick_format_t getKeyAxisTickFormat(dataset_id_t dataSetId);
    QString getKeyAxisTickFormatString(dataset_id_t dataSetId);

    GraphStyle& getStyleForGraph(dataset_id_t dataSetId, StandardColumn column);
    GraphStyle& getStyleForGraph(dataset_id_t dataSetId, ExtraColumn column);


    GraphStyle& getStyleForGraph(QCPGraph* graph);

     typedef enum {
         RS_TIME = 0, /*!< Align on time only ignoring year, month and day */
         RS_MONTH = 1, /*!< Align on month, day and time ignoring year */
         RS_YEAR = 2 /*!< Align on exact timestamp match */
     } RescaleType;

     RescaleType getCurrentScaleType() { return this->currentScaleType; }

signals:
    void axisCountChanged(int valueAxes, int keyAxes);
    void dataSetRemoved(dataset_id_t dataSetId);
    void legendVisibilityChanged(bool visible);

public slots:
    /** Changes the timespan for the specified dataset and replots the chart.
     * @param dataSetId Dataset to update.
     * @param start New start time (default is existing start)
     * @param end New end time (default is existing end)
     */
    void changeDataSetTimespan(dataset_id_t dataSetId, QDateTime start = QDateTime(), QDateTime end = QDateTime());

    /** Refreshes the dataset from the datasource.
     *
     * @param dataSetId Dataset to refresh.
     */
    void refreshDataSet(dataset_id_t dataSetId);

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
    void setGraphStyles(QMap<StandardColumn, GraphStyle> styles, dataset_id_t dataSetId);\
    void setGraphStyles(QMap<ExtraColumn, GraphStyle> styles, dataset_id_t dataSetId);

    void rescale();
    void rescaleByTime();
    void rescaleByTimeOfYear();
    void rescaleByTimeOfDay();

    PlusCursor* cursor() {return plusCursor;}

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
    void addGenericGraph(DataSet dataSet, StandardColumn column, SampleSet samples);
    void addGenericGraph(DataSet dataSet, ExtraColumn column, SampleSet samples);

    /** Adds a rainfall graph to the plot.
     *
     * @param dataSet Dataset the rainfall graph is for.
     * @param samples Samples for the dataset.
     */
    void addRainfallGraph(DataSet dataSet, SampleSet samples, StandardColumn column);

    /** Adds a wind direction graph to the plot.
     *
     * @param dataSet Dataset the wind direction graph is for.
     * @param samples Samples for the dataset.
     */
    void addWindDirectionGraph(DataSet dataSet, SampleSet samples, StandardColumn column);

    /** The last used rescale type. Used by the rescale() slot to reset the plot scale
     * to the last used.
     */
    RescaleType currentScaleType;

    /** Rescales the plot aligning all x axes with the one that has the
     * largest timespan and scaling them to the same.
     *
     * y axis are scaled normally.
     *
     * If only one data set is present, chart->rescaleAxes() will be
     * called instead.
     */
    void multiRescale(RescaleType rs_type = RS_TIME);

    QCPGraph* getGraph(dataset_id_t dataSetId, StandardColumn column);
    QCPGraph* getGraph(dataset_id_t dataSetId, ExtraColumn column);

    void removeGraph(QCPGraph* graph, dataset_id_t dataSetId, StandardColumn column);
    void removeGraph(QCPGraph* graph, dataset_id_t dataSetId, ExtraColumn column);

    /*******************
     * Misc            *
     *******************/
    QMap<dataset_id_t, QMap<StandardColumn, GraphStyle> > graphStyles;
    QMap<dataset_id_t, QMap<ExtraColumn, GraphStyle> > extraGraphStyles;

    /** Returns the samples from a sampleset for the specified column.
     *
     * @param column Column we want the samples for.
     * @param samples The full set of samples for some dataset.
     * @return The list of samples for a single column from a dataset.
     */
    QVector<double> samplesForColumn(StandardColumn column, SampleSet samples);
    QVector<double> samplesForColumn(ExtraColumn column, SampleSet samples);

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
        AT_HUMIDITY = 5, /*!< Axis in % */  /* NOTE: The value (type==5) is used in pluscursor.cpp*/
        AT_RAINFALL = 6, /*!< Axis in mm */
        AT_SOLAR_RADIATION = 7, /*!< Axis in W/m^2 */
        AT_UV_INDEX = 8, /*!< Axis for UV Index - no unit */
        AT_RAIN_RATE = 9, /*!< Axis for Rain rate in mm/h */
        AT_RECEPTION = 10, /*!< Axis for wireless reception (%) */
        AT_EVAPOTRANSPIRATION = 11, /*!< Axis for Evapotrainspiration in mm */
        AT_SOIL_MOISTURE = 12, /*!< Axis for soil moisture in centibar */
        AT_LEAF_WETNESS = 13, /*!< Axis for leaf wetness */
        AT_KEY = 100 /*!< X Axis for DataSet 0. AT_KEY+1 for DataSet 1, etc. */
    } AxisType;

    QMap<AxisType, QPointer<QCPAxis> > configuredValueAxes;
    QMap<AxisType, QPointer<QCPAxis> > configuredKeyAxes;
    QMap<QCPAxis*, AxisType> axisTypes;
    QMap<AxisType, int> axisReferences;
    QMap<dataset_id_t, key_axis_tick_format_t> keyAxisTickFormats;
    QMap<dataset_id_t, QString> keyAxisTickCustomFormats;

    /** Initialises the axisLabels dictionary.
     */
    void populateAxisLabels();

    /** Removes all axis from the plot that no longer have any associated
     * data.
     */
    void removeUnusedAxes();

    void removeDataSet(dataset_id_t dataset_id);

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
     * @param referenceCount If the reference should be counted
     * @return Value axis of the specified type.
     */
    QPointer<QCPAxis> getValueAxis(AxisType axisType, bool referenceCount=true);

    /** Creates a new X axis for the specified dataset.
     *
     * @param dataSetId Dataset to create the axis for.
     * @return The new axis.
     */
    QPointer<QCPAxis> createKeyAxis(dataset_id_t dataSetId);

    /** Gets the X axis for the specified dataset. If it does not exist
     * it will be created.
     *
     * @param dataSetId the ID of the dataset to get the key axis for
     * @param referenceCount If a reference should be counted
     * @return X axis for the specified dataset.
     */
    QPointer<QCPAxis> getKeyAxis(dataset_id_t dataSetId,
                                 bool referenceCount=true);

    /** Returns the type of axis to use for a particular column type.
     * For example, the AT_TEMPERATURE axis for the SC_IndoorTemperature
     * column
     *
     * @param column Column to get the axis for.
     * @return Axis type to use.
     */
    AxisType axisTypeForColumn(StandardColumn column);
    AxisType axisTypeForColumn(ExtraColumn column);

    PlusCursor *plusCursor;

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
    PlotWidget* chart;

    bool mAxisGridsVisible; /*!< If axis grids should be visible on creation */
};

#endif // WEATHERPLOT_H
