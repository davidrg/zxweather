#include "weatherplotter.h"
#include "settings.h"
#include "constants.h"

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

    // Set these to something invalid so they don't get incorrectly counted as
    // belonging to data set 0.
    chart->xAxis->setProperty(AXIS_DATASET, -1);
    chart->xAxis2->setProperty(AXIS_DATASET, -1);
}

void WeatherPlotter::setDataSource(AbstractDataSource *dataSource)
{
    cacheManager->setDataSource(dataSource);
}

void WeatherPlotter::drawChart(QList<DataSet> dataSets)
{
    cacheManager->flushCache();

    chart->clearGraphs();
    chart->clearPlottables();
    foreach(AxisType type, axisReferences.keys())
        axisReferences[type] = 0;
    removeUnusedAxes();

    foreach (DataSet dataSet, dataSets) {
        this->dataSets[dataSet.id] = dataSet;
    }

    cacheManager->getDataSets(dataSets);
}

void WeatherPlotter::addDataSet(DataSet dataSet) {
    this->dataSets[dataSet.id] = dataSet;
    QList<DataSet> foo;
    foo.append(dataSet);
    cacheManager->getDataSets(foo);
}

void WeatherPlotter::populateAxisLabels() {
    axisLabels.insert(AT_HUMIDITY, "Humidity (%)");
    axisLabels.insert(AT_PRESSURE, "Pressure (hPa)");
    axisLabels.insert(AT_RAINFALL, "Rainfall (mm)");
    axisLabels.insert(AT_TEMPERATURE, "Temperature (" TEMPERATURE_SYMBOL ")");
    axisLabels.insert(AT_WIND_SPEED, "Wind speed (m/s)");
    axisLabels.insert(AT_WIND_DIRECTION, "Wind direction (degrees)");
    axisLabels.insert(AT_SOLAR_RADIATION, "Solar Radiation (W/m" SQUARED_SYMBOL ")");
    axisLabels.insert(AT_UV_INDEX, "UV Index");
    axisLabels.insert(AT_RAIN_RATE, "Rain rate (mm/h)");
    axisLabels.insert(AT_RECEPTION, "Wireless Reception (%)");
    axisLabels.insert(AT_EVAPOTRANSPIRATION, "Evapotranspiration (mm)");
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
    QSharedPointer<QCPAxisTickerDateTime> ticker(new QCPAxisTickerDateTime());
    ticker->setTickStepStrategy(QCPAxisTicker::tssReadability);
    axis->setTicker(ticker);

    axis->grid()->setVisible(axisGridVisible());
    configuredKeyAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    axis->setProperty(AXIS_DATASET, dataSetId);

    emit axisCountChanged(configuredKeyAxes.count() + configuredValueAxes.count());

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::getKeyAxis(dataset_id_t dataSetId,
                                             bool referenceCount) {
    AxisType axisType = (AxisType)(AT_KEY + dataSetId);

    QPointer<QCPAxis> axis = NULL;
    if (!configuredKeyAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createKeyAxis(dataSetId);
    else
        // Axis already exists
        axis = configuredKeyAxes[axisType];

    if (referenceCount) {
        if (!axisReferences.contains(axisType))
            axisReferences.insert(axisType,0);
        axisReferences[axisType]++;
    }

    return axis;
}

WeatherPlotter::AxisType WeatherPlotter::axisTypeForColumn(SampleColumn column) {
    switch (column) {
    case SC_Temperature:
    case SC_IndoorTemperature:
    case SC_ApparentTemperature:
    case SC_WindChill:
    case SC_DewPoint:
    case SC_HighTemperature:
    case SC_LowTemperature:
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
    case SC_GustWindDirection:
        return AT_WIND_DIRECTION;

    case SC_SolarRadiation:
    case SC_HighSolarRadiation:
        return AT_SOLAR_RADIATION;

    case SC_UV_Index:
    case SC_HighUVIndex:
        return AT_UV_INDEX;

    case SC_HighRainRate:
        return AT_RAIN_RATE;

    case SC_Reception:
        return AT_RECEPTION;

    case SC_Evapotranspiration:
        return AT_EVAPOTRANSPIRATION;

    case SC_NoColumns:
    case SC_Timestamp:
    case SC_ForecastRuleId:
    default:
        // This should never happen.
        return AT_NONE;
    }
}

QVector<double> WeatherPlotter::samplesForColumn(SampleColumn column, SampleSet samples) {

    Q_ASSERT_X(column != SC_WindDirection, "samplesForColumn", "WindDirection is unsupported");
    Q_ASSERT_X(column != SC_GustWindDirection, "samplesForColumn", "GustWindDirection is unsupported");
    Q_ASSERT_X(column != SC_NoColumns, "samplesForColumn", "Invalid column SC_NoColumns");
    Q_ASSERT_X(column != SC_Timestamp, "samplesForColumn", "Invalid column SC_Timestamp");
    Q_ASSERT_X(column != SC_ForecastRuleId, "samplesForColumn", "Invalid column ForecastRuleId");


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
    case SC_UV_Index:
        return samples.uvIndex;
    case SC_SolarRadiation:
        return samples.solarRadiation;
    case SC_HighTemperature:
        return samples.highTemperature;
    case SC_LowTemperature:
        return samples.lowTemperature;
    case SC_HighSolarRadiation:
        return samples.highSolarRadiation;
    case SC_HighUVIndex:
        return samples.highUVIndex;
    case SC_HighRainRate:
        return samples.highRainRate;
    case SC_Reception:
        return samples.reception;
    case SC_Evapotranspiration:
        return samples.evapotranspiration;

    case SC_WindDirection:
    case SC_GustWindDirection:
    case SC_ForecastRuleId:
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
    if (graphStyles[dataSet.id].contains(column))
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

void WeatherPlotter::addRainfallGraph(DataSet dataSet, SampleSet samples, SampleColumn column)
{
    Q_ASSERT_X(column == SC_Rainfall || column == SC_HighRainRate,
               "addRainfallGraph", "Unsupported column type (must be rainfall or high rain rate)");

    WeatherPlotter::AxisType axisType = AT_RAINFALL;
    if (column == SC_HighRainRate) {
        axisType = AT_RAIN_RATE;
    }

    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(axisType));
    graph->setKeyAxis(getKeyAxis(dataSet.id));
    // How do you plot rainfall data so it doesn't look stupid?
    // I don't know. Needs to be lower resolution I guess.
    if (column == SC_Rainfall)
        graph->setData(samples.timestamp, samples.rainfall);
    else
        graph->setData(samples.timestamp, samples.highRainRate);

    GraphStyle gs;
    if (graphStyles[dataSet.id].contains(column))
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
    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addWindDirectionGraph(DataSet dataSet, SampleSet samples, SampleColumn column)
{
    QCPGraph * graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(AT_WIND_DIRECTION));
    graph->setKeyAxis(getKeyAxis(dataSet.id));

    if (column == SC_WindDirection) {
        QList<uint> keys = samples.windDirection.keys();
        qSort(keys.begin(), keys.end());
        QVector<double> timestamps;
        QVector<double> values;
        foreach(uint key, keys) {
            timestamps.append(key);
            values.append(samples.windDirection[key]);
        }
        graph->setData(timestamps,values);
    } else { // SC_GustWindDirection
        QList<uint> keys = samples.gustWindDirection.keys();
        qSort(keys.begin(), keys.end());
        QVector<double> timestamps;
        QVector<double> values;
        foreach(uint key, keys) {
            timestamps.append(key);
            values.append(samples.gustWindDirection[key]);
        }
        graph->setData(timestamps,values);
    }

    GraphStyle gs;
    if (graphStyles[dataSet.id].contains(column))
        gs = graphStyles[dataSet.id][column];
    else {
        gs = column;
        graphStyles[dataSet.id][column] = gs;
    }
    gs.applyStyle(graph);

    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, AT_WIND_DIRECTION);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addGraphs(QMap<dataset_id_t, SampleSet> sampleSets)
{
    foreach (dataset_id_t dataSetId, sampleSets.keys()) {
        DataSet ds = dataSets[dataSetId];
        SampleSet samples = sampleSets[dataSetId];

        if (samples.timestampUnix.isEmpty()) {
            qDebug() << "Skip data set " << dataSetId << "- no data.";
            continue;
        }

        dataSetMinimumTime[dataSetId] = QDateTime::fromTime_t(
                    samples.timestampUnix.first());
        dataSetMaximumTime[dataSetId] = QDateTime::fromTime_t(
                    samples.timestampUnix.last());

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
            addRainfallGraph(ds, samples, SC_Rainfall); // keep

        if (ds.columns.testFlag(SC_HighRainRate))
            addRainfallGraph(ds, samples, SC_HighRainRate);

        if (ds.columns.testFlag(SC_AverageWindSpeed))
            addGenericGraph(ds, SC_AverageWindSpeed, samples);
            //addAverageWindSpeedGraph(samples);

        if (ds.columns.testFlag(SC_GustWindSpeed))
            addGenericGraph(ds, SC_GustWindSpeed, samples);
            //addGustWindSpeedGraph(samples);

        if (ds.columns.testFlag(SC_WindDirection))
            addWindDirectionGraph(ds, samples, SC_WindDirection); // keep

        if (ds.columns.testFlag(SC_GustWindDirection))
            addWindDirectionGraph(ds, samples, SC_GustWindDirection); // keep

        if (ds.columns.testFlag(SC_UV_Index))
            addGenericGraph(ds, SC_UV_Index, samples);

        if (ds.columns.testFlag(SC_SolarRadiation))
            addGenericGraph(ds, SC_SolarRadiation, samples);

        if (ds.columns.testFlag(SC_HighTemperature))
            addGenericGraph(ds, SC_HighTemperature, samples);

        if (ds.columns.testFlag(SC_LowTemperature))
            addGenericGraph(ds, SC_LowTemperature, samples);

        if (ds.columns.testFlag(SC_HighSolarRadiation))
            addGenericGraph(ds, SC_HighSolarRadiation, samples);

        if (ds.columns.testFlag(SC_HighUVIndex))
            addGenericGraph(ds, SC_HighUVIndex, samples);

        if (ds.columns.testFlag(SC_Reception))
            addGenericGraph(ds, SC_Reception, samples);

        if (ds.columns.testFlag(SC_Evapotranspiration))
            addGenericGraph(ds, SC_Evapotranspiration, samples);
    }
}

void WeatherPlotter::drawChart(QMap<dataset_id_t, SampleSet> sampleSets)
{
    qDebug() << "Drawing Chart...";

    addGraphs(sampleSets);

    if (chart->graphCount() > 1)
        chart->legend->setVisible(true);
    else
        chart->legend->setVisible(false);

    multiRescale();
    chart->replot();
}

void WeatherPlotter::rescaleByTime() {
    multiRescale(RS_YEAR); // Align on year/month/day/hour/minute/second
    chart->replot();
}
void WeatherPlotter::rescaleByTimeOfYear() {
    multiRescale(RS_MONTH); // Align on month/day/hour/minute/second
    chart->replot();
}

void WeatherPlotter::rescaleByTimeOfDay() {
    multiRescale(RS_TIME); // Align on hour/minute/second
    chart->replot();
}

void WeatherPlotter::multiRescale(RescaleType rs_type) {
    qDebug() << "multiRescale" << rs_type;
    if (dataSets.count() < 2) {
        chart->rescaleAxes();
        return;
    }

    // 1. Find the X axis for the dataset that has the largest range.
    uint max_delta = 0;
    QDateTime max_range_ds_start;
    QDateTime max_range_ds_end;
    QDateTime min_start = QDateTime(QDate(3000,12,12));
    QDateTime max_end = QDateTime(QDate(0,1, 1));
    QTime min_time = QTime(23,59,59);
    int min_month = 12;
    int min_day_of_month = 31;

    foreach (dataset_id_t id, dataSets.keys()) {
        QDateTime start = dataSetMinimumTime[id];
        QDateTime end = dataSetMaximumTime[id];

        uint delta = end.toTime_t() - start.toTime_t();
        if (delta > max_delta) {
            max_delta = delta;
            max_range_ds_start = start;
            max_range_ds_end = end;
        }
        if (start < min_start) {
            min_start = start;
        }
        if (end > max_end) {
            max_end = end;
        }

        // This is used by time of day alignment (RS_TIME)
        if (start.time() < min_time) {
            min_time = start.time();
        }

        int month = start.date().month();
        int day = start.date().day();
        if (month < min_month) {
            min_month = month;
            min_day_of_month = day;
        } else if (month == min_month && day < min_day_of_month) {
            min_day_of_month = day;
        }
    }

    QCPRange range;
    if (rs_type == RS_YEAR) {
        // Time alignment:
        // The range needs go from the earliest timestamp in the earliest
        // data set to the latest timestamp in the latest dataset so as to
        // include all data in all datasets.
        range.lower = min_start.toTime_t();
        range.upper = max_end.toTime_t();

        foreach (dataset_id_t id, dataSets.keys()) {
            QPointer<QCPAxis> axis = getKeyAxis(id, false);
            axis->setRange(range);
        }
    } else if (rs_type == RS_MONTH || rs_type == RS_TIME) {
        // Time of Year alignment (RS_MONTH):
        // This sets all X axes to have the same size range. Each axis range
        // is chosen such that at any given point the second, hour, minute,
        // day of month and day on all axes will be the same - only the year
        // component will vary.
        // For example:
        //  14-JUN-15 5:50  14-JUN-15 5:55  14-JUN-15 6:00  14-JUN-15 6:05
        //                                  14-JUN-16 6:00  14-JUN-16 6:05
        //                                                  14-JUN-13 6:05
        //                  14-JUN-14 5:55  14-JUN-14 6:00  14-JUN-14 6:05
        //
        // Time of Day alignment (RS_TIME):
        // This sets all X axes to have the same size range. Each axis range
        // is chosen such that the time component of its earliest timestamp
        // lines up with the earliest matching time component of the axis
        // with the earliest time component.
        // For example:
        //  14-JUN-15 5:50  14-JUN-15 5:55  14-JUN-15 6:00  14-JUN-15 6:05
        //                                  18-MAY-16 6:00  18-MAY-16 6:05
        //                                                  20-DEC-13 6:05
        //                  19-FEB-15 5:55  19-FEB-15 6:00  19-FEB-15 6:05

        // Line up the starting point for all axes
        foreach (dataset_id_t id, dataSets.keys()) {
            QPointer<QCPAxis> axis = getKeyAxis(id, false);

            // Rescale the axis so we can get its min and max values
            axis->rescale();
            QCPRange axisRange = axis->range();
            QDateTime min_ts = QDateTime::fromTime_t(axisRange.lower);

            QDateTime start_time;

            if (rs_type == RS_MONTH) {
                start_time = QDateTime(QDate(min_ts.date().year(),
                                             min_month,
                                             min_day_of_month),
                                       min_time);
            } else if (rs_type == RS_TIME) {
                // Work out the start of the range. To do this we'll use
                // our start date and the minimum start time of any axis.
                start_time = QDateTime(min_ts.date(),
                                                 min_time);
            }

            axisRange.lower = start_time.toTime_t();
            axis->setRange(axisRange);
        }

        double max_range = 0;
        // Find the max range delta
        foreach (dataset_id_t id, dataSets.keys()) {
            QPointer<QCPAxis> axis = getKeyAxis(id, false);
            QCPRange range = axis->range();

            double r = range.upper - range.lower;
            if (r > max_range) {
                max_range = r;
            }
        }

        // Line up the ending point for all axes
        foreach (dataset_id_t id, dataSets.keys()) {
            QPointer<QCPAxis> axis = getKeyAxis(id, false);

            QCPRange axisRange = axis->range();
            QDateTime end_time = QDateTime::fromTime_t(
                        axisRange.lower + max_range);


            axisRange.upper = end_time.toTime_t();
            axis->setRange(axisRange);
        }
    }

    // Rescale all Y axes
    QList<QCPAxis*> yAxes = chart->axisRect()->axes(
                QCPAxis::atLeft | QCPAxis::atRight);
    foreach (QCPAxis* axis, yAxes) {
        axis->rescale();
    }
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

void WeatherPlotter::removeDataSet(dataset_id_t dataset_id) {
    dataSets.remove(dataset_id);
    dataSetMinimumTime.remove(dataset_id);
    dataSetMaximumTime.remove(dataset_id);
    emit dataSetRemoved(dataset_id);
}

void WeatherPlotter::removeUnusedAxes()
{
    qDebug() << "Removing unused axes...";
    foreach(AxisType type, axisReferences.keys()) {
        qDebug() << "Axis type" << type << "has" << axisReferences[type] << "references.";
        if (axisReferences[type] == 0) {
            // Axis is now unused. Remove it.
            QPointer<QCPAxis> axis;

            if (type >= AT_KEY) {
                if (dataSets.count() == 1) {
                    qDebug() << "Leaving Key Axis" << type << " - final data set";
                } else {
                    qDebug() << "Removing Key Axis" << type;
                    axis = configuredKeyAxes[type];
                    configuredKeyAxes.remove(type);
                }
            } else {
                qDebug() << "Removing Value Axis" << type;
                axis = configuredValueAxes[type];                
                configuredValueAxes.remove(type);
            }

            // Remove all the tracking information.

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

    chart->clearGraphs();
    chart->clearPlottables();

    cacheManager->getDataSets(dataSets.values());
}

QCPGraph* WeatherPlotter::getGraph(dataset_id_t dataSetId, SampleColumn column) {
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
    }
    return graph;
}

void WeatherPlotter::removeGraph(QCPGraph* graph, dataset_id_t dataSetId,
                                 SampleColumn column) {
    // Remove the graph from the dataset so it doesn't magically come back later
    dataSets[dataSetId].columns &= ~column;

    // One less use of this particular axis.
    AxisType axisType = (AxisType)graph->property(GRAPH_AXIS).toInt();
    axisReferences[axisType]--;
    qDebug() << "Value axis now has" << axisReferences[axisType] << "references";

    // And one less use of the key axis for the data set too
    AxisType keyType = (AxisType)(AT_KEY + dataSetId);
    axisReferences[keyType]--;
    qDebug() << "Key axis now has" << axisReferences[keyType] << "references";

    chart->removeGraph(graph);
}

void WeatherPlotter::removeGraph(dataset_id_t dataSetId, SampleColumn column) {
    qDebug() << "Removing graph" << column << "for data set" << dataSetId;
    // Try to find the graph that goes with this column.

    QCPGraph *graph = getGraph(dataSetId, column);
    if (graph == NULL) {
        return;
    }

    removeGraph(graph, dataSetId, column);

    removeUnusedAxes();

    if (dataSets[dataSetId].columns == SC_NoColumns) {
        // The dataset doesn't have any graphs left in the chart. Remove the
        // dataset itself if its not the last one remaining.
        if (dataSets.count() > 1) {
            removeDataSet(dataSetId);
        }
    }

    chart->replot();
}

void WeatherPlotter::removeGraphs(dataset_id_t dataSetId, SampleColumns columns) {

    SampleColumns dsColumns = dataSets[dataSetId].columns;

    columns &= dsColumns;

    QList<SampleColumn> columnList;

    if (columns.testFlag(SC_Temperature))
        columnList << SC_Temperature;

    if (columns.testFlag(SC_IndoorTemperature))
        columnList << SC_IndoorTemperature;

    if (columns.testFlag(SC_ApparentTemperature))
        columnList << SC_ApparentTemperature;

    if (columns.testFlag(SC_DewPoint))
        columnList << SC_DewPoint;

    if (columns.testFlag(SC_WindChill))
        columnList << SC_WindChill;

    if (columns.testFlag(SC_Humidity))
        columnList << SC_Humidity;

    if (columns.testFlag(SC_IndoorHumidity))
        columnList << SC_IndoorHumidity;

    if (columns.testFlag(SC_Pressure))
        columnList << SC_Pressure;

    if (columns.testFlag(SC_Rainfall))
        columnList << SC_Rainfall;

    if (columns.testFlag(SC_AverageWindSpeed))
        columnList << SC_AverageWindSpeed;

    if (columns.testFlag(SC_GustWindSpeed))
        columnList << SC_GustWindSpeed;

    if (columns.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.testFlag(SC_UV_Index))
        columnList << SC_UV_Index;

    if (columns.testFlag(SC_SolarRadiation))
        columnList << SC_SolarRadiation;

    if (columns.testFlag(SC_HighTemperature))
        columnList << SC_HighTemperature;

    if (columns.testFlag(SC_LowTemperature))
        columnList << SC_LowTemperature;

    if (columns.testFlag(SC_HighSolarRadiation))
        columnList << SC_HighSolarRadiation;

    if (columns.testFlag(SC_HighUVIndex))
        columnList << SC_HighUVIndex;

    if (columns.testFlag(SC_GustWindDirection))
        columnList << SC_GustWindDirection;

    if (columns.testFlag(SC_HighRainRate))
        columnList << SC_HighRainRate;

    if (columns.testFlag(SC_Reception))
        columnList << SC_Reception;

    if (columns.testFlag(SC_Evapotranspiration))
        columnList << SC_Evapotranspiration;

    for(int i = 0; i < columnList.count(); i++) {
        SampleColumn column = columnList[i];

        QCPGraph *graph = getGraph(dataSetId, column);
        if (graph == NULL) {
            // Graph is already missing?
            continue;
        }

        removeGraph(graph, dataSetId, column);
    }

    removeUnusedAxes();

    if (dataSets[dataSetId].columns == SC_NoColumns) {
        // The dataset doesn't have any graphs left in the chart. Remove the
        // dataset itself if its not the last remaining.
        if (dataSets.count() > 1) {
            removeDataSet(dataSetId);
        }
    }

    chart->replot();
}

QString WeatherPlotter::defaultLabelForAxis(QCPAxis *axis) {
    AxisType type = axisTypes[axis];

    if (type >= AT_KEY) {
        // Its an X axis. Its label comes from the dataset.
        //dataset_id_t dataSetId = type - AT_KEY;
        // TODO: Something else here
        return "Time";
    } else {
        return axisLabels[type];
    }
}

QMap<SampleColumn, GraphStyle> WeatherPlotter::getGraphStyles(dataset_id_t dataSetId) {
    return graphStyles[dataSetId];
}

GraphStyle& WeatherPlotter::getStyleForGraph(dataset_id_t dataSetId, SampleColumn column) {
    return graphStyles[dataSetId][column];
}

GraphStyle& WeatherPlotter::getStyleForGraph(QCPGraph* graph) {
    dataset_id_t dataset = graph->property(GRAPH_DATASET).toInt();
    SampleColumn col = (SampleColumn)graph->property(GRAPH_TYPE).toInt();

    return getStyleForGraph(dataset, col);
}

void WeatherPlotter::setGraphStyles(QMap<SampleColumn, GraphStyle> styles, dataset_id_t dataSetId) {
    graphStyles[dataSetId] = styles;
}
