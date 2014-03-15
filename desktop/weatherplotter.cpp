#include "weatherplotter.h"
#include "Settings.h"

#include <QtDebug>
#include <QMessageBox>

GraphStyle::GraphStyle(SampleColumn column) {
    ChartColours colours = Settings::getInstance().getChartColours();

    QColor colour;

    switch (column) {
    case SC_Temperature:
        colour = colours.temperature;
        name = "Temperature";
        break;
    case SC_IndoorTemperature:
        colour = colours.indoorTemperature;
        name = "Indoor Temperature";
        break;
    case SC_ApparentTemperature:
        colour = colours.apparentTemperature;
        name = "Apparent Temperature";
        break;
    case SC_WindChill:
        colour = colours.windChill;
        name = "Wind Chill";
        break;
    case SC_DewPoint:
        colour = colours.dewPoint;
        name = "Dew Point";
        break;
    case SC_Humidity:
        colour = colours.humidity;
        name = "Humidity";
        break;
    case SC_IndoorHumidity:
        colour = colours.indoorHumidity;
        name = "Indoor Humidity";
        break;
    case SC_Pressure:
        colour = colours.pressure;
        name = "Pressure";
        break;
    case SC_Rainfall:
        colour = colours.rainfall;
        name = "Rainfall";
        break;
    case SC_AverageWindSpeed:
        colour = colours.averageWindSpeed;
        name = "Average Wind Speed";
        break;
    case SC_GustWindSpeed:
        colour = colours.gustWindSpeed;
        name = "Gust Wind Speed";
        break;
    case SC_WindDirection:
        colour = colours.windDirection;
        name = "Wind Direction";
        break;
    case SC_NoColumns:
    case SC_Timestamp:
    default:
        // This should never happen.
        colour = Qt::black;
        name = "Invalid Graph";
    }

    pen = QPen(colour);
    scatterStyle = QCPScatterStyle::ssNone;
    brush = QBrush();
    lineStyle = QCPGraph::lsLine;
}

void GraphStyle::applyStyle(QCPGraph *graph) {
    graph->setName(getName());
    graph->setPen(getPen());
    graph->setScatterStyle(getScatterStyle());
    graph->setBrush(getBrush());
    graph->setLineStyle(getLineStyle());
}


WeatherPlotter::WeatherPlotter(QCustomPlot *chart, QObject *parent) :
    QObject(parent)
{
    this->chart = chart;

    setAxisGridVisible(true);

    populateAxisLabels();

    // Configure chart
    chart->xAxis->setLabel("Time");
    chart->xAxis->setTickLabelType(QCPAxis::ltDateTime);

    // Keep axis ranges locked
    connect(chart->xAxis, SIGNAL(rangeChanged(QCPRange)),
            chart->xAxis2, SLOT(setRange(QCPRange)));
}

void WeatherPlotter::setDataSource(AbstractDataSource *dataSource)
{
    connect(dataSource, SIGNAL(samplesReady(SampleSet)),
            this, SLOT(samplesReady(SampleSet)));
    connect(dataSource, SIGNAL(sampleRetrievalError(QString)),
            this, SLOT(samplesError(QString)));

    this->dataSource.reset(dataSource);
}

void WeatherPlotter::drawChart(SampleColumns columns, QDateTime startTime,
                            QDateTime endTime)
{
    currentChartColumns = columns;
    requestData(columns, false, startTime, endTime);
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

    // No columns selected? nothing to do.
    if (currentChartColumns == SC_NoColumns) return;

    requestData(currentChartColumns);
}

void WeatherPlotter::refresh(QDateTime start, QDateTime end) {

    bool cacheValid = true;

    if (!start.isNull() && start != startTime) {
        cacheValid = false;
        startTime = start;
    }
    if (!end.isNull() && end != endTime) {
        cacheValid = false;
        endTime = end;
    }

    if (cacheValid) {
        qDebug() << "Time range not changed. Refreshing with sample cache...";
        drawChart(sampleCache);
    } else {
        qDebug() << "Requesting new data and redrawing...";
        requestData(currentChartColumns, false);
    }
}

void WeatherPlotter::requestData(SampleColumns columns,
                              bool merge,
                              QDateTime start,
                              QDateTime end) {

    if (start.isNull())
        start = startTime;
    if (end.isNull())
        end = endTime;

    mergeSamples = merge;
    if (merge) {
        dataSetColumns |= columns;
        mergeColumns = columns;
    } else
        dataSetColumns = columns;
    startTime = start;
    endTime = end;

    qDebug() << "Fetching columns" << columns << "between" << start << "and" << end;

    dataSource->fetchSamples(columns, start, end);
}

QPointer<QCPAxis> WeatherPlotter::createAxis(AxisType type) {
    QCPAxis* axis = NULL;
    if (configuredAxes.isEmpty()) {

        axis = chart->yAxis;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else if (configuredAxes.count() == 1) {
        axis = chart->yAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);
    } else {
        // Every second axis can go on the right.
        if (configuredAxes.count() % 2 == 0)
            axis = chart->axisRect()->addAxis(QCPAxis::atLeft);
        else
            axis = chart->axisRect()->addAxis(QCPAxis::atRight);
    }
    axis->grid()->setVisible(axisGridVisible());
    configuredAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    emit axisCountChanged(configuredAxes.count());

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::getValueAxis(AxisType axisType) {
    QPointer<QCPAxis> axis = NULL;
    if (!configuredAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createAxis(axisType);
    else
        // Axis already exists
        axis = configuredAxes[axisType];

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

void WeatherPlotter::addGenericGraph(SampleColumn column, SampleSet samples) {

    AxisType axisType = axisTypeForColumn(column);

    QCPGraph* graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(axisType));
    graph->setData(samples.timestamp, samplesForColumn(column,samples));

    GraphStyle(column).applyStyle(graph);

    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
}

void WeatherPlotter::addRainfallGraph(SampleSet samples, GraphStyle style)
{
    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(AT_RAINFALL));
    // How do you plot rainfall data so it doesn't look stupid?
    // I don't know. Needs to be lower resolution I guess.
    graph->setData(samples.timestamp, samples.rainfall);

    style.applyStyle(graph);

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
}

void WeatherPlotter::addWindDirectionGraph(SampleSet samples, GraphStyle style)
{
    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(AT_WIND_DIRECTION));
    QList<uint> keys = samples.windDirection.keys();
    qSort(keys.begin(), keys.end());
    QVector<double> timestamps;
    QVector<double> values;
    foreach(uint key, keys) {
        timestamps.append(key);
        values.append(samples.windDirection[key]);
    }
    graph->setData(timestamps,values);

    style.applyStyle(graph);

    graph->setProperty(GRAPH_TYPE, SC_WindDirection);
    graph->setProperty(GRAPH_AXIS, AT_WIND_DIRECTION);
}


void WeatherPlotter::addGraphs(SampleColumns columns, SampleSet samples)
{
    qDebug() << "Adding graphs:" << columns;

    if (columns.testFlag(SC_Temperature))
        addGenericGraph(SC_Temperature, samples);
        //addTemperatureGraph(samples);

    if (columns.testFlag(SC_IndoorTemperature))
        addGenericGraph(SC_IndoorTemperature, samples);
        //addIndoorTemperatureGraph(samples);

    if (columns.testFlag(SC_ApparentTemperature))
        addGenericGraph(SC_ApparentTemperature, samples);
        //addApparentTemperatureGraph(samples);

    if (columns.testFlag(SC_DewPoint))
        addGenericGraph(SC_DewPoint, samples);
        //addDewPointGraph(samples);

    if (columns.testFlag(SC_WindChill))
        addGenericGraph(SC_WindChill, samples);
        //addWindChillGraph(samples);

    if (columns.testFlag(SC_Humidity))
        addGenericGraph(SC_Humidity, samples);
        //addHumidityGraph(samples);

    if (columns.testFlag(SC_IndoorHumidity))
        addGenericGraph(SC_IndoorHumidity, samples);
        //addIndoorHumidityGraph(samples);

    if (columns.testFlag(SC_Pressure))
        addGenericGraph(SC_Pressure, samples);
        //addPressureGraph(samples);

    if (columns.testFlag(SC_Rainfall))
        addRainfallGraph(samples); // keep

    if (columns.testFlag(SC_AverageWindSpeed))
        addGenericGraph(SC_AverageWindSpeed, samples);
        //addAverageWindSpeedGraph(samples);

    if (columns.testFlag(SC_GustWindSpeed))
        addGenericGraph(SC_GustWindSpeed, samples);
        //addGustWindSpeedGraph(samples);

    if (columns.testFlag(SC_WindDirection))
        addWindDirectionGraph(samples); // keep
}

void WeatherPlotter::drawChart(SampleSet samples)
{
    qDebug() << "Samples: " << samples.sampleCount;

    chart->clearGraphs();
    chart->clearPlottables();
    foreach(AxisType type, axisReferences.keys())
        axisReferences[type] = 0;
    removeUnusedAxes();

    addGraphs(currentChartColumns, samples);


    if (chart->graphCount() > 1)
        chart->legend->setVisible(true);
    else
        chart->legend->setVisible(false);

    chart->rescaleAxes();
    chart->replot();
}

void WeatherPlotter::mergeSampleSet(SampleSet samples, SampleColumns columns)
{
    qDebug() << "Merging in columns:" << columns;
    if (columns.testFlag(SC_Temperature))
        sampleCache.temperature = samples.temperature;

    if (columns.testFlag(SC_IndoorTemperature))
        sampleCache.indoorTemperature = samples.indoorTemperature;

    if (columns.testFlag(SC_ApparentTemperature))
        sampleCache.apparentTemperature = samples.apparentTemperature;

    if (columns.testFlag(SC_DewPoint))
        sampleCache.dewPoint = samples.dewPoint;

    if (columns.testFlag(SC_WindChill))
        sampleCache.windChill = samples.windChill;

    if (columns.testFlag(SC_Humidity))
        sampleCache.humidity = samples.humidity;

    if (columns.testFlag(SC_IndoorHumidity))
        sampleCache.indoorHumidity = samples.indoorHumidity;

    if (columns.testFlag(SC_Pressure))
        sampleCache.pressure = samples.pressure;

    if (columns.testFlag(SC_Rainfall))
        sampleCache.rainfall = samples.rainfall;

    if (columns.testFlag(SC_AverageWindSpeed))
        sampleCache.averageWindSpeed = samples.averageWindSpeed;

    if (columns.testFlag(SC_GustWindSpeed))
        sampleCache.gustWindSpeed = samples.gustWindSpeed;

    if (columns.testFlag(SC_WindDirection))
        sampleCache.windDirection = samples.windDirection;

    dataSetColumns |= columns;
}

void WeatherPlotter::samplesReady(SampleSet samples) {
    qDebug() << "Samples ready";
    if (mergeSamples) {
        qDebug() << "Merging received samples into cache...";
        SampleColumns columns = mergeColumns;

        mergeSampleSet(samples, columns);

        // Add the new graphs into the chart.
        addGraphs(mergeColumns, samples);
        currentChartColumns |= mergeColumns;
        chart->rescaleAxes();
        chart->replot();
    } else {
        qDebug() << "Refreshing cache...";
        // Cache future samples for fast refreshing.
        sampleCache = samples;

        // Completely redraw the chart.
        drawChart(samples);
    }
    mergeSamples = false;
    mergeColumns = SC_NoColumns;
}

void WeatherPlotter::samplesError(QString message)
{
    // TODO: Find a better way of doing this
    QMessageBox::critical(0, "Error", message);
}

void WeatherPlotter::removeUnusedAxes()
{
    foreach(AxisType type, axisReferences.keys()) {
        if (axisReferences[type] == 0) {
            // Axis is now unused. Remove it.
            QPointer<QCPAxis> axis = configuredAxes[type];

            // Remove all the tracking information.
            configuredAxes.remove(type);
            axisTypes.remove(axis);
            axisReferences.remove(type);

            // And then the axis itself.
            if (axis == chart->yAxis) {
                chart->yAxis->setVisible(false);
                chart->yAxis->setTickLabels(false);
            } else if (axis == chart->yAxis2) {
                chart->yAxis2->setVisible(false);
                chart->yAxis2->setTickLabels(false);
            } else {
                chart->axisRect()->removeAxis(axis);
            }
        }
    }
    emit axisCountChanged(configuredAxes.count());
}

SampleColumns WeatherPlotter::availableColumns()
{
    SampleColumns availableColumns = ~currentChartColumns;

    // This will have gone and set all the unused bits in the int too.
    // Go clear anything we don't use.
    availableColumns &= ALL_SAMPLE_COLUMNS;

    // Then unset the timestamp flag if its set (its not a valid option here).
    if (availableColumns.testFlag(SC_Timestamp))
        availableColumns &= ~SC_Timestamp;

    return availableColumns;
}

void WeatherPlotter::addGraphs(SampleColumns columns) {
    if (columns == SC_NoColumns)
        return; // Nothing chosen - nothing to do

    // See if we already have everything we need in the sample cache.
    if ((columns & dataSetColumns) == columns) {
        // Looks like all the data is already there. Just need to re-add
        // the missing graphs
        qDebug() << "Data for graph already exists. Not refetching.";

        addGraphs(columns, sampleCache);
        currentChartColumns |= columns;
        chart->replot();
    } else {
        // Some data is missing. Go fetch it.

        qDebug() << "Requesting data for: " << columns;

        requestData(columns, true);
    }
}

void WeatherPlotter::removeGraph(SampleColumn column) {

    // Try to find the graph that goes with this column.
    QCPGraph* graph = 0;
    for (int i = 0; i < chart->graphCount(); i++) {
        SampleColumn graphColumn =
                (SampleColumn)chart->graph(i)->property(GRAPH_TYPE).toInt();

        if (graphColumn == column)
            graph = chart->graph(i);
    }

    if (graph == 0) {
        qWarning() << "Couldn't find graph to remove for column" << column;
        return;
    }

    currentChartColumns &= ~column;

    // One less use of this particular axis.
    AxisType axisType = (AxisType)graph->property(GRAPH_AXIS).toInt();
    axisReferences[axisType]--;

    removeUnusedAxes();

    chart->removeGraph(graph);
    chart->replot();
}

QString WeatherPlotter::defaultLabelForAxis(QCPAxis *axis) {
    AxisType type = axisTypes[axis];
    return axisLabels[type];
}
