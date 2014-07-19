#include "weatherplotter.h"
#include "Settings.h"

#include <QtDebug>
#include <QMessageBox>

WeatherPlotter::WeatherPlotter(QCustomPlot *chart, QObject *parent) :
    QObject(parent)
{
    this->chart = chart;

    setAxisGridVisible(true);

    populateAxisLabels();

    cacheManager.reset(new CacheManager(this));
    connect(cacheManager.data(), SIGNAL(dataSetsReady(QMap<dataset_id_t,SampleSet>)),
            this, SLOT(dataSetsReady(QMap<dataset_id_t,SampleSet>)));
    connect(cacheManager.data(), SIGNAL(retreivalError(QString)),
            this, SLOT(dataSourceError(QString)));
}

void WeatherPlotter::setDataSource(AbstractDataSource *dataSource)
{
    cacheManager->setDataSource(dataSource);
}

void WeatherPlotter::drawChart(QList<DataSet> dataSets)
{
    foreach (DataSet dataSet, dataSets) {
        this->dataSets[dataSet.id] = dataSet;
    }

    cacheManager->getDataSets(dataSets);
}

void WeatherPlotter::populateAxisLabels() {
    axisLabels.insert(AT_HUMIDITY, "Humidity (%)");
    axisLabels.insert(AT_PRESSURE, "Pressure (hPa)");
    axisLabels.insert(AT_RAINFALL, "Rainfall (mm)");
    axisLabels.insert(AT_TEMPERATURE, "Temperature (\xB0""C)");
    axisLabels.insert(AT_WIND_SPEED, "Wind speed (m/s)");
    axisLabels.insert(AT_WIND_DIRECTION, "Wind direction (degrees)");
}

void WeatherPlotter::reload() {

    int dataSetsToPlot = 0;
    foreach (DataSet dataSet, dataSets.values()) {
        if (dataSet.columns != SC_NoColumns) dataSetsToPlot++;
    }
    if (dataSetsToPlot == 0)
        return; // No columns selected? nothing to do.

    cacheManager->flushCache();

    cacheManager->getDataSets(dataSets.values());
}

void WeatherPlotter::changeDataSetTimespan(dataset_id_t dataSetId, QDateTime start, QDateTime end) {

    DataSet ds = dataSets[dataSetId];

    if (!start.isNull() && start != ds.startTime) {
        ds.startTime = start;
    }
    if (!end.isNull() && end != ds.endTime) {
        ds.endTime = end;
    }
    dataSets[dataSetId] = ds;

    cacheManager->getDataSets(dataSets.values());
}

QPointer<QCPAxis> WeatherPlotter::createValueAxis(AxisType type) {
    Q_ASSERT_X(type < AT_KEY, "createValueAxis", "Axis type must not be for a key axis");

    QCPAxis* axis = NULL;
    if (configuredValueAxes.isEmpty()) {

        axis = chart->yAxis;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else if (configuredValueAxes.count() == 1) {
        axis = chart->yAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else {
        // Every second axis can go on the right.
        if (configuredValueAxes.count() % 2 == 0)
            axis = chart->axisRect()->addAxis(QCPAxis::atLeft);
        else
            axis = chart->axisRect()->addAxis(QCPAxis::atRight);
    }
    axis->grid()->setVisible(axisGridVisible());
    configuredValueAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    emit axisCountChanged(configuredKeyAxes.count() + configuredValueAxes.count());

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::getValueAxis(AxisType axisType) {
    Q_ASSERT_X(axisType < AT_KEY, "getValueAxis", "Axis type must not be for a key axis");

    QPointer<QCPAxis> axis = NULL;
    if (!configuredValueAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createValueAxis(axisType);
    else
        // Axis already exists
        axis = configuredValueAxes[axisType];

    if (!axisReferences.contains(axisType))
        axisReferences.insert(axisType,0);
    axisReferences[axisType]++;

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::createKeyAxis(dataset_id_t dataSetId) {
    AxisType type = (AxisType)(AT_KEY + dataSetId);

    QCPAxis* axis = NULL;
    if (configuredKeyAxes.isEmpty()) {
        axis = chart->xAxis;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else if (configuredKeyAxes.count() == 1) {
        axis = chart->xAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else {
        // Every second axis can go on the top.
        if (configuredKeyAxes.count() % 2 == 0)
            axis = chart->axisRect()->addAxis(QCPAxis::atBottom);
        else
            axis = chart->axisRect()->addAxis(QCPAxis::atTop);
    }
    axis->setTickLabelType(QCPAxis::ltDateTime);
    axis->grid()->setVisible(axisGridVisible());
    configuredKeyAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    emit axisCountChanged(configuredKeyAxes.count() + configuredValueAxes.count());

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::getKeyAxis(dataset_id_t dataSetId) {
    AxisType axisType = (AxisType)(AT_KEY + dataSetId);

    QPointer<QCPAxis> axis = NULL;
    if (!configuredKeyAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createKeyAxis(dataSetId);
    else
        // Axis already exists
        axis = configuredKeyAxes[axisType];

    if (!axisReferences.contains(axisType))
        axisReferences.insert(axisType,0);
    axisReferences[axisType]++;

    return axis;
}

WeatherPlotter::AxisType WeatherPlotter::axisTypeForColumn(SampleColumn column) {
    switch (column) {
    case SC_Temperature:
    case SC_IndoorTemperature:
    case SC_ApparentTemperature:
    case SC_WindChill:
    case SC_DewPoint:
        return AT_TEMPERATURE;

    case SC_Humidity:
    case SC_IndoorHumidity:
        return AT_HUMIDITY;

    case SC_Pressure:
        return AT_PRESSURE;

    case SC_Rainfall:
        return AT_RAINFALL;

    case SC_AverageWindSpeed:
    case SC_GustWindSpeed:
        return AT_WIND_SPEED;

    case SC_WindDirection:
        return AT_WIND_DIRECTION;

    case SC_NoColumns:
    case SC_Timestamp:
    default:
        // This should never happen.
        return AT_NONE;
    }
}

QVector<double> WeatherPlotter::samplesForColumn(SampleColumn column, SampleSet samples) {

    Q_ASSERT_X(column != SC_WindDirection, "samplesForColumn", "WindDirection is unsupported");
    Q_ASSERT_X(column != SC_NoColumns, "samplesForColumn", "Invalid column SC_NoColumns");
    Q_ASSERT_X(column != SC_Timestamp, "samplesForColumn", "Invalid column SC_Timestamp");

    switch (column) {
    case SC_Temperature:
        return samples.temperature;
    case SC_IndoorTemperature:
        return samples.indoorTemperature;
    case SC_ApparentTemperature:
        return samples.apparentTemperature;
    case SC_WindChill:
        return samples.windChill;
    case SC_DewPoint:
        return samples.dewPoint;
    case SC_Humidity:
        return samples.humidity;
    case SC_IndoorHumidity:
        return samples.indoorHumidity;
    case SC_Pressure:
        return samples.pressure;
    case SC_Rainfall:
        return samples.rainfall;
    case SC_AverageWindSpeed:
        return samples.averageWindSpeed;
    case SC_GustWindSpeed:
        return samples.gustWindSpeed;

    case SC_WindDirection:
    case SC_NoColumns:
    case SC_Timestamp:
    default:
        // This should never happen.
        return QVector<double>();
    }
}

void WeatherPlotter::addGenericGraph(DataSet dataSet, SampleColumn column, SampleSet samples) {
    qDebug() << "Adding graph for dataset" << dataSet.id << "column" << (int)column;
    AxisType axisType = axisTypeForColumn(column);

    QCPGraph* graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(axisType));
    graph->setKeyAxis(getKeyAxis(dataSet.id));
    graph->setData(samples.timestamp, samplesForColumn(column,samples));

    GraphStyle gs;
    if (graphStyles.contains(column))
        gs = graphStyles[dataSet.id][column];
    else {
        gs = column;
        graphStyles[dataSet.id][column] = gs;
    }
    gs.applyStyle(graph);

    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addRainfallGraph(DataSet dataSet, SampleSet samples)
{
    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(AT_RAINFALL));
    graph->setKeyAxis(getKeyAxis(dataSet.id));
    // How do you plot rainfall data so it doesn't look stupid?
    // I don't know. Needs to be lower resolution I guess.
    graph->setData(samples.timestamp, samples.rainfall);

    SampleColumn column = SC_Rainfall;
    GraphStyle gs;
    if (graphStyles.contains(column))
        gs = graphStyles[dataSet.id][column];
    else {
        gs = column;
        graphStyles[dataSet.id][column] = gs;
    }
    gs.applyStyle(graph);

    //graph->setLineStyle(QCPGraph::lsNone);
    //graph->setScatterStyle(QCP::ssCross);
//            QCPBars *bars = new QCPBars(ui->chart->xAxis, ui->chart->yAxis);
//            ui->chart->addPlottable(bars);
//            bars->setData(samples.timestamp, samples.rainfall);
//            bars->setName("Rainfall");
//            bars->setPen(QPen(Qt::darkBlue));
//            bars->setBrush(QBrush(Qt::green));
//            bars->setWidth(1000);
    // set pen
    graph->setProperty(GRAPH_TYPE, SC_Rainfall);
    graph->setProperty(GRAPH_AXIS, AT_RAINFALL);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addWindDirectionGraph(DataSet dataSet, SampleSet samples)
{
    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(AT_WIND_DIRECTION));
    graph->setKeyAxis(getKeyAxis(dataSet.id));

    QList<uint> keys = samples.windDirection.keys();
    qSort(keys.begin(), keys.end());
    QVector<double> timestamps;
    QVector<double> values;
    foreach(uint key, keys) {
        timestamps.append(key);
        values.append(samples.windDirection[key]);
    }
    graph->setData(timestamps,values);

    SampleColumn column = SC_WindDirection;
    GraphStyle gs;
    if (graphStyles.contains(column))
        gs = graphStyles[dataSet.id][column];
    else {
        gs = column;
        graphStyles[dataSet.id][column] = gs;
    }
    gs.applyStyle(graph);

    graph->setProperty(GRAPH_TYPE, SC_WindDirection);
    graph->setProperty(GRAPH_AXIS, AT_WIND_DIRECTION);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addGraphs(QMap<dataset_id_t, SampleSet> sampleSets)
{
    foreach (dataset_id_t dataSetId, sampleSets.keys()) {
        DataSet ds = dataSets[dataSetId];
        SampleSet samples = sampleSets[dataSetId];

        qDebug() << "Adding graphs" << (int)ds.columns << "for dataset" << ds.id;

        if (ds.columns.testFlag(SC_Temperature))
            addGenericGraph(ds, SC_Temperature, samples);
            //addTemperatureGraph(samples);

        if (ds.columns.testFlag(SC_IndoorTemperature))
            addGenericGraph(ds, SC_IndoorTemperature, samples);
            //addIndoorTemperatureGraph(samples);

        if (ds.columns.testFlag(SC_ApparentTemperature))
            addGenericGraph(ds, SC_ApparentTemperature, samples);
            //addApparentTemperatureGraph(samples);

        if (ds.columns.testFlag(SC_DewPoint))
            addGenericGraph(ds, SC_DewPoint, samples);
            //addDewPointGraph(samples);

        if (ds.columns.testFlag(SC_WindChill))
            addGenericGraph(ds, SC_WindChill, samples);
            //addWindChillGraph(samples);

        if (ds.columns.testFlag(SC_Humidity))
            addGenericGraph(ds, SC_Humidity, samples);
            //addHumidityGraph(samples);

        if (ds.columns.testFlag(SC_IndoorHumidity))
            addGenericGraph(ds, SC_IndoorHumidity, samples);
            //addIndoorHumidityGraph(samples);

        if (ds.columns.testFlag(SC_Pressure))
            addGenericGraph(ds, SC_Pressure, samples);
            //addPressureGraph(samples);

        if (ds.columns.testFlag(SC_Rainfall))
            addRainfallGraph(ds, samples); // keep

        if (ds.columns.testFlag(SC_AverageWindSpeed))
            addGenericGraph(ds, SC_AverageWindSpeed, samples);
            //addAverageWindSpeedGraph(samples);

        if (ds.columns.testFlag(SC_GustWindSpeed))
            addGenericGraph(ds, SC_GustWindSpeed, samples);
            //addGustWindSpeedGraph(samples);

        if (ds.columns.testFlag(SC_WindDirection))
            addWindDirectionGraph(ds, samples); // keep
    }
}

void WeatherPlotter::drawChart(QMap<dataset_id_t, SampleSet> sampleSets)
{
    qDebug() << "Drawing Chart...";

    chart->clearGraphs();
    chart->clearPlottables();
    foreach(AxisType type, axisReferences.keys())
        axisReferences[type] = 0;
    removeUnusedAxes();

    addGraphs(sampleSets);

    if (chart->graphCount() > 1)
        chart->legend->setVisible(true);
    else
        chart->legend->setVisible(false);

    chart->rescaleAxes();
    chart->replot();
}

void WeatherPlotter::dataSetsReady(QMap<dataset_id_t, SampleSet> samples) {
    qDebug() << "Data received from cache manager. Drawing chart for"
             << samples.keys().count() << "datasets...";
    drawChart(samples);
}

void WeatherPlotter::dataSourceError(QString message)
{
    // TODO: Find a better way of doing this
    QMessageBox::critical(0, "Error", message);
}

void WeatherPlotter::removeUnusedAxes()
{
    foreach(AxisType type, axisReferences.keys()) {
        if (axisReferences[type] == 0) {
            // Axis is now unused. Remove it.
            QPointer<QCPAxis> axis;

            if (type >= AT_KEY) {
                axis = configuredKeyAxes[type];
                configuredKeyAxes.remove(type);
            } else {
                axis = configuredValueAxes[type];
                configuredValueAxes.remove(type);
            }

            // Remove all the tracking information.

            qDebug() << "Removing axis type" << type;

            axisTypes.remove(axis);
            axisReferences.remove(type);

            // And then the axis itself.
            if (axis == chart->yAxis) {
                chart->yAxis->setVisible(false);
                chart->yAxis->setTickLabels(false);
            } else if (axis == chart->yAxis2) {
                chart->yAxis2->setVisible(false);
                chart->yAxis2->setTickLabels(false);
            } else if (axis == chart->xAxis) {
                chart->xAxis->setVisible(false);
                chart->xAxis->setTickLabels(false);
            } else if (axis == chart->xAxis2) {
                chart->xAxis2->setVisible(false);
                chart->xAxis2->setTickLabels(false);
            } else {
                chart->axisRect()->removeAxis(axis);
            }
        }
    }
    emit axisCountChanged(configuredValueAxes.count() + configuredKeyAxes.count());
}

SampleColumns WeatherPlotter::availableColumns(dataset_id_t dataSetId)
{
    //SampleColumns availableColumns = ~currentChartColumns;
    SampleColumns availableColumns = ~dataSets[dataSetId].columns;

    // This will have gone and set all the unused bits in the int too.
    // Go clear anything we don't use.
    availableColumns &= ALL_SAMPLE_COLUMNS;

    // Then unset the timestamp flag if its set (its not a valid option here).
    if (availableColumns.testFlag(SC_Timestamp))
        availableColumns &= ~SC_Timestamp;

    return availableColumns;
}

void WeatherPlotter::addGraphs(dataset_id_t dataSetId, SampleColumns columns) {
    dataSets[dataSetId].columns |= columns;

    cacheManager->getDataSets(dataSets.values());
}

void WeatherPlotter::removeGraph(dataset_id_t dataSetId, SampleColumn column) {

    // Try to find the graph that goes with this column.
    QCPGraph* graph = 0;
    for (int i = 0; i < chart->graphCount(); i++) {
        SampleColumn graphColumn =
                (SampleColumn)chart->graph(i)->property(GRAPH_TYPE).toInt();
        dataset_id_t graphDataSetId =
                (dataset_id_t)chart->graph(i)->property(GRAPH_DATASET).toUInt();

        if (graphColumn == column && graphDataSetId == dataSetId)
            graph = chart->graph(i);
    }

    if (graph == 0) {
        qWarning() << "Couldn't find graph to remove for column" << column;
        return;
    }

    dataSets[dataSetId].columns &= ~column;

    // One less use of this particular axis.
    AxisType axisType = (AxisType)graph->property(GRAPH_AXIS).toInt();
    axisReferences[axisType]--;

    removeUnusedAxes();

    chart->removeGraph(graph);
    chart->replot();
}

QString WeatherPlotter::defaultLabelForAxis(QCPAxis *axis) {
    AxisType type = axisTypes[axis];

    if (type >= AT_KEY) {
        // Its an X axis. Its label comes from the dataset.
        dataset_id_t dataSetId = type - AT_KEY;
        QString label = dataSets[dataSetId].axisLabel;
        if (label.isEmpty())
            return "Time";
        return label;
    } else {
        return axisLabels[type];
    }
}

QMap<SampleColumn, GraphStyle> WeatherPlotter::getGraphStyles(dataset_id_t dataSetId) {
    return graphStyles[dataSetId];
}

void WeatherPlotter::setGraphStyles(QMap<SampleColumn, GraphStyle> styles, dataset_id_t dataSetId) {
    graphStyles[dataSetId] = styles;
}
