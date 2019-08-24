#include "weatherplotter.h"
#include "settings.h"
#include "constants.h"

#include <QtDebug>
#include <QMessageBox>
#include <QFontMetrics>

WeatherPlotter::WeatherPlotter(PlotWidget *chart, QObject *parent) :
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

    currentScaleType = RS_YEAR; // Align on exact timestamp match.

#ifdef FEATURE_PLUS_CURSOR
    connect(chart, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(updateCursor(QMouseEvent*)));
    connect(chart, SIGNAL(mouseLeave(QEvent*)),
            this, SLOT(hideCursor()));

    hCursor = new QCPItemLine(chart);
    hCursor->setLayer("overlay");
    hCursor->setVisible(false);
    hCursor->setSelectable(false);
    hCursor->start->setType(QCPItemPosition::ptAbsolute);
    hCursor->end->setType(QCPItemPosition::ptAbsolute);

    vCursor = new QCPItemLine(chart);
    vCursor->setLayer("overlay");
    vCursor->setVisible(false);
    vCursor->setSelectable(false);
    vCursor->start->setType(QCPItemPosition::ptAbsolute);
    vCursor->end->setType(QCPItemPosition::ptAbsolute);

    setCursorEnabled(true);
#endif
}

void WeatherPlotter::setDataSource(AbstractDataSource *dataSource)
{
    cacheManager->setDataSource(dataSource);
}

void WeatherPlotter::drawChart(QList<DataSet> dataSets)
{
    cacheManager->flushCache();

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
    if (Settings::getInstance().imperial()) {
        axisLabels.insert(AT_HUMIDITY, "Humidity (%)");
        axisLabels.insert(AT_PRESSURE, "Pressure (inHg)");
        axisLabels.insert(AT_RAINFALL, "Rainfall (in)");
        axisLabels.insert(AT_TEMPERATURE, "Temperature (" IMPERIAL_TEMPERATURE_SYMBOL ")");
        axisLabels.insert(AT_WIND_SPEED, "Wind speed (mph)");
        axisLabels.insert(AT_WIND_DIRECTION, "Wind direction (degrees)");
        axisLabels.insert(AT_SOLAR_RADIATION, "Solar radiation (W/m" SQUARED_SYMBOL ")");
        axisLabels.insert(AT_UV_INDEX, "UV Index");
        axisLabels.insert(AT_RAIN_RATE, "Rain rate (in/h)");
        axisLabels.insert(AT_RECEPTION, "Wireless reception (%)");
        axisLabels.insert(AT_EVAPOTRANSPIRATION, "Evapotranspiration (in)");
    } else {
        bool kmh = Settings::getInstance().kmh();
        axisLabels.insert(AT_HUMIDITY, "Humidity (%)");
        axisLabels.insert(AT_PRESSURE, "Pressure (hPa)");
        axisLabels.insert(AT_RAINFALL, "Rainfall (mm)");
        axisLabels.insert(AT_TEMPERATURE, "Temperature (" TEMPERATURE_SYMBOL ")");
        axisLabels.insert(AT_WIND_SPEED, kmh ? "Wind speed (km/h)" : "Wind speed (m/s)");
        axisLabels.insert(AT_WIND_DIRECTION, "Wind direction (degrees)");
        axisLabels.insert(AT_SOLAR_RADIATION, "Solar radiation (W/m" SQUARED_SYMBOL ")");
        axisLabels.insert(AT_UV_INDEX, "UV Index");
        axisLabels.insert(AT_RAIN_RATE, "Rain rate (mm/h)");
        axisLabels.insert(AT_RECEPTION, "Wireless reception (%)");
        axisLabels.insert(AT_EVAPOTRANSPIRATION, "Evapotranspiration (mm)");
    }
    axisLabels.insert(AT_SOIL_MOISTURE, "Soil Moisture (cbar)");
    axisLabels.insert(AT_LEAF_WETNESS, "Leaf Wetness");
}

void WeatherPlotter::reload() {

    int dataSetsToPlot = 0;
    foreach (DataSet dataSet, dataSets.values()) {
        if (dataSet.columns.standard != SC_NoColumns || dataSet.columns.extra != EC_NoColumns) dataSetsToPlot++;
    }
    if (dataSetsToPlot == 0)
        return; // No columns selected? nothing to do.

    cacheManager->flushCache();

    chart->clearPlottables();
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

    chart->clearPlottables();
    cacheManager->getDataSets(dataSets.values());
}

QPointer<QCPAxis> WeatherPlotter::createValueAxis(AxisType type) {
    Q_ASSERT_X(type < AT_KEY, "createValueAxis", "Axis type must not be for a key axis");

    bool atLeft;

    QCPAxis* axis = NULL;
    if (configuredValueAxes.isEmpty()) {

        axis = chart->yAxis;
        axis->setVisible(true);
        axis->setTickLabels(true);

        atLeft = true;
    } else if (configuredValueAxes.count() == 1) {
        axis = chart->yAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);

        atLeft = false;
    } else {
        // Every second axis can go on the right.
        if (configuredValueAxes.count() % 2 == 0) {
            axis = chart->axisRect()->addAxis(QCPAxis::atLeft);
            atLeft = true;
        } else {
            axis = chart->axisRect()->addAxis(QCPAxis::atRight);
            atLeft = false;
        }
    }
    axis->grid()->setVisible(axisGridVisible());
    configuredValueAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    axis->setTickLabelFont(Settings::getInstance().defaultChartAxisTickLabelFont());
    axis->setLabelFont(Settings::getInstance().defaultChartAxisLabelFont());

#ifdef FEATURE_PLUS_CURSOR
    QPointer<QCPItemText> tag = new QCPItemText(chart);
    tag->setLayer("overlay");
    tag->setClipToAxisRect(false);
    tag->setPadding(QMargins(3,0,3,0));
    tag->setBrush(QBrush(Qt::white));
    tag->setPen(QPen(Qt::black));
    tag->setSelectable(false);
    if (atLeft) {
        tag->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else {
        tag->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    tag->setText("0.0");
    tag->position->setAxes(chart->xAxis, axis);

    cursorAxisTags[type] = tag;
#endif

    emit axisCountChanged(configuredValueAxes.count(), configuredKeyAxes.count());

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::getValueAxis(AxisType axisType, bool referenceCount) {
    Q_ASSERT_X(axisType < AT_KEY, "getValueAxis", "Axis type must not be for a key axis");

    QPointer<QCPAxis> axis = NULL;
    if (!configuredValueAxes.contains(axisType))
        // Axis of specified type doesn't exist. Create it.
        axis = createValueAxis(axisType);
    else
        // Axis already exists
        axis = configuredValueAxes[axisType];

    if (referenceCount) {
        if (!axisReferences.contains(axisType))
            axisReferences.insert(axisType,0);
        axisReferences[axisType]++;
    }

    return axis;
}

QPointer<QCPAxis> WeatherPlotter::createKeyAxis(dataset_id_t dataSetId) {
    AxisType type = (AxisType)(AT_KEY + dataSetId);

    bool atTop;

    QCPAxis* axis = NULL;
    if (configuredKeyAxes.isEmpty()) {
        axis = chart->xAxis;
        axis->setVisible(true);
        axis->setTickLabels(true);
        atTop = false;
    } else if (configuredKeyAxes.count() == 1) {
        axis = chart->xAxis2;
        axis->setVisible(true);
        axis->setTickLabels(true);
        atTop = true;
    } else {
        // Every second axis can go on the top.
        if (configuredKeyAxes.count() % 2 == 0) {
            axis = chart->axisRect()->addAxis(QCPAxis::atBottom);
            atTop = false;
        } else {
            axis = chart->axisRect()->addAxis(QCPAxis::atTop);
            atTop = true;
        }
    }
    QSharedPointer<QCPAxisTickerDateTime> ticker(new QCPAxisTickerDateTime());
    ticker->setTickStepStrategy(QCPAxisTicker::tssReadability);
    axis->setTicker(ticker);

    axis->grid()->setVisible(axisGridVisible());
    configuredKeyAxes.insert(type, axis);
    axisTypes.insert(axis,type);
    axis->setLabel(axisLabels[type]);

    axis->setProperty(AXIS_DATASET, dataSetId);

    axis->setTickLabelFont(Settings::getInstance().defaultChartAxisTickLabelFont());
    axis->setLabelFont(Settings::getInstance().defaultChartAxisLabelFont());

#ifdef FEATURE_PLUS_CURSOR
    QPointer<QCPItemText> tag = new QCPItemText(chart);
    tag->setLayer("overlay");
    tag->setClipToAxisRect(false);
    tag->setPadding(QMargins(3,0,3,0));
    tag->setBrush(QBrush(Qt::white));
    tag->setPen(QPen(Qt::black));
    tag->setSelectable(false);
    if (atTop) {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    } else {
        tag->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }

    tag->setText("0.0");
    tag->position->setAxes(axis, chart->yAxis);

    cursorAxisTags[type] = tag;
#endif

    emit axisCountChanged(configuredValueAxes.count(), configuredKeyAxes.count());

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

WeatherPlotter::AxisType WeatherPlotter::axisTypeForColumn(StandardColumn column) {
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

WeatherPlotter::AxisType WeatherPlotter::axisTypeForColumn(ExtraColumn column) {
    switch (column) {
    case EC_LeafTemperature1:
    case EC_LeafTemperature2:
    case EC_SoilTemperature1:
    case EC_SoilTemperature2:
    case EC_SoilTemperature3:
    case EC_SoilTemperature4:
    case EC_ExtraTemperature1:
    case EC_ExtraTemperature2:
    case EC_ExtraTemperature3:
        return AT_TEMPERATURE;

    case EC_ExtraHumidity1:
    case EC_ExtraHumidity2:
        return AT_HUMIDITY;

    case EC_LeafWetness1:
    case EC_LeafWetness2:
        return AT_LEAF_WETNESS;

    case EC_SoilMoisture1:
    case EC_SoilMoisture2:
    case EC_SoilMoisture3:
    case EC_SoilMoisture4:
        return AT_SOIL_MOISTURE;

    case EC_NoColumns:
    default:
        // This should never happen.
        return AT_NONE;
    }
}

QVector<double> WeatherPlotter::samplesForColumn(StandardColumn column, SampleSet samples) {

    Q_ASSERT_X(column != SC_WindDirection, "samplesForColumn", "WindDirection is unsupported");
    Q_ASSERT_X(column != SC_GustWindDirection, "samplesForColumn", "GustWindDirection is unsupported");
    Q_ASSERT_X(column != SC_NoColumns, "samplesForColumn", "Invalid column SC_NoColumns");
    Q_ASSERT_X(column != SC_Timestamp, "samplesForColumn", "Invalid column SC_Timestamp");
    Q_ASSERT_X(column != SC_ForecastRuleId, "samplesForColumn", "Invalid column ForecastRuleId");

    switch(column) {
    case SC_Temperature:
    case SC_IndoorTemperature:
    case SC_ApparentTemperature:
    case SC_WindChill:
    case SC_DewPoint:
    case SC_Pressure:
    case SC_Rainfall:
    case SC_HighRainRate:
    case SC_Evapotranspiration:
    case SC_AverageWindSpeed:
    case SC_GustWindSpeed:
    case SC_HighTemperature:
    case SC_LowTemperature: {
        // These units all support conversion to imperial or other units.
        UnitConversions::unit_t units = SampleColumnUnits(column);
        if (Settings::getInstance().imperial()) {
            units = UnitConversions::metricToImperial(units);
        } else {
            if (units == UnitConversions::U_METERS_PER_SECOND && Settings::getInstance().kmh()) {
                units = UnitConversions::U_KILOMETERS_PER_HOUR;
            }
        }

        return SampleColumnInUnits(samples, column, units);
    }
    case SC_Humidity:
        return samples.humidity;
    case SC_IndoorHumidity:
        return samples.indoorHumidity;
    case SC_UV_Index:
        return samples.uvIndex;
    case SC_SolarRadiation:
        return samples.solarRadiation;
    case SC_HighSolarRadiation:
        return samples.highSolarRadiation;
    case SC_HighUVIndex:
        return samples.highUVIndex;
    case SC_Reception:
        return samples.reception;
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

QVector<double> WeatherPlotter::samplesForColumn(ExtraColumn column, SampleSet samples) {

    switch(column) {
    case EC_LeafTemperature1:
    case EC_LeafTemperature2:
    case EC_SoilTemperature1:
    case EC_SoilTemperature2:
    case EC_SoilTemperature3:
    case EC_SoilTemperature4:
    case EC_ExtraTemperature1:
    case EC_ExtraTemperature2:
    case EC_ExtraTemperature3: {
        // These units all support conversion to imperial or other units.
        UnitConversions::unit_t units = SampleColumnUnits(column);
        if (Settings::getInstance().imperial()) {
            units = UnitConversions::metricToImperial(units);
        } else {
            if (units == UnitConversions::U_METERS_PER_SECOND && Settings::getInstance().kmh()) {
                units = UnitConversions::U_KILOMETERS_PER_HOUR;
            }
        }

        return SampleColumnInUnits(samples, column, units);
    }
    case EC_ExtraHumidity1:
        return samples.extraHumidity1;
    case EC_ExtraHumidity2:
        return samples.extraHumidity2;
    case EC_LeafWetness1:
        return samples.leafWetness1;
    case EC_LeafWetness2:
        return samples.leafWetness2;
    case EC_SoilMoisture1:
        return samples.soilMoisture1;
    case EC_SoilMoisture2:
        return samples.soilMoisture2;
    case EC_SoilMoisture3:
        return samples.soilMoisture3;
    case EC_SoilMoisture4:
        return samples.soilMoisture4;
    default:
        // This should never happen.
        return QVector<double>();
    }
}

void WeatherPlotter::addGenericGraph(DataSet dataSet, StandardColumn column, SampleSet samples) {
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

    graph->setProperty(COLUMN_TYPE, "standard");
    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addGenericGraph(DataSet dataSet, ExtraColumn column, SampleSet samples) {
    qDebug() << "Adding graph for dataset" << dataSet.id << "column" << (int)column;
    AxisType axisType = axisTypeForColumn(column);

    QCPGraph* graph = chart->addGraph();
    graph->setValueAxis(getValueAxis(axisType));
    graph->setKeyAxis(getKeyAxis(dataSet.id));
    graph->setData(samples.timestamp, samplesForColumn(column,samples));

    GraphStyle gs;
    if (extraGraphStyles[dataSet.id].contains(column))
        gs = extraGraphStyles[dataSet.id][column];
    else {
        gs = column;
        extraGraphStyles[dataSet.id][column] = gs;
    }
    gs.applyStyle(graph);

    graph->setProperty(COLUMN_TYPE, "extra");
    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addRainfallGraph(DataSet dataSet, SampleSet samples, StandardColumn column)
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
    graph->setProperty(COLUMN_TYPE, "standard");
    graph->setProperty(GRAPH_TYPE, column);
    graph->setProperty(GRAPH_AXIS, axisType);
    graph->setProperty(GRAPH_DATASET, dataSet.id);
}

void WeatherPlotter::addWindDirectionGraph(DataSet dataSet, SampleSet samples, StandardColumn column)
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

    graph->setProperty(COLUMN_TYPE, "standard");
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

        qDebug() << "Adding graphs" << (int)ds.columns.standard << (int)ds.columns.extra << "for dataset" << ds.id;

        if (ds.columns.standard.testFlag(SC_Temperature))
            addGenericGraph(ds, SC_Temperature, samples);
            //addTemperatureGraph(samples);

        if (ds.columns.standard.testFlag(SC_IndoorTemperature))
            addGenericGraph(ds, SC_IndoorTemperature, samples);
            //addIndoorTemperatureGraph(samples);

        if (ds.columns.standard.testFlag(SC_ApparentTemperature))
            addGenericGraph(ds, SC_ApparentTemperature, samples);
            //addApparentTemperatureGraph(samples);

        if (ds.columns.standard.testFlag(SC_DewPoint))
            addGenericGraph(ds, SC_DewPoint, samples);
            //addDewPointGraph(samples);

        if (ds.columns.standard.testFlag(SC_WindChill))
            addGenericGraph(ds, SC_WindChill, samples);
            //addWindChillGraph(samples);

        if (ds.columns.standard.testFlag(SC_Humidity))
            addGenericGraph(ds, SC_Humidity, samples);
            //addHumidityGraph(samples);

        if (ds.columns.standard.testFlag(SC_IndoorHumidity))
            addGenericGraph(ds, SC_IndoorHumidity, samples);
            //addIndoorHumidityGraph(samples);

        if (ds.columns.standard.testFlag(SC_Pressure))
            addGenericGraph(ds, SC_Pressure, samples);
            //addPressureGraph(samples);

        if (ds.columns.standard.testFlag(SC_Rainfall))
            addRainfallGraph(ds, samples, SC_Rainfall); // keep

        if (ds.columns.standard.testFlag(SC_HighRainRate))
            addRainfallGraph(ds, samples, SC_HighRainRate);

        if (ds.columns.standard.testFlag(SC_AverageWindSpeed))
            addGenericGraph(ds, SC_AverageWindSpeed, samples);
            //addAverageWindSpeedGraph(samples);

        if (ds.columns.standard.testFlag(SC_GustWindSpeed))
            addGenericGraph(ds, SC_GustWindSpeed, samples);
            //addGustWindSpeedGraph(samples);

        if (ds.columns.standard.testFlag(SC_WindDirection))
            addWindDirectionGraph(ds, samples, SC_WindDirection); // keep

        if (ds.columns.standard.testFlag(SC_GustWindDirection))
            addWindDirectionGraph(ds, samples, SC_GustWindDirection); // keep

        if (ds.columns.standard.testFlag(SC_UV_Index))
            addGenericGraph(ds, SC_UV_Index, samples);

        if (ds.columns.standard.testFlag(SC_SolarRadiation))
            addGenericGraph(ds, SC_SolarRadiation, samples);

        if (ds.columns.standard.testFlag(SC_HighTemperature))
            addGenericGraph(ds, SC_HighTemperature, samples);

        if (ds.columns.standard.testFlag(SC_LowTemperature))
            addGenericGraph(ds, SC_LowTemperature, samples);

        if (ds.columns.standard.testFlag(SC_HighSolarRadiation))
            addGenericGraph(ds, SC_HighSolarRadiation, samples);

        if (ds.columns.standard.testFlag(SC_HighUVIndex))
            addGenericGraph(ds, SC_HighUVIndex, samples);

        if (ds.columns.standard.testFlag(SC_Reception))
            addGenericGraph(ds, SC_Reception, samples);

        if (ds.columns.standard.testFlag(SC_Evapotranspiration))
            addGenericGraph(ds, SC_Evapotranspiration, samples);

        if (ds.columns.extra.testFlag(EC_LeafWetness1))
            addGenericGraph(ds, EC_LeafWetness1, samples);

        if (ds.columns.extra.testFlag(EC_LeafWetness2))
            addGenericGraph(ds, EC_LeafWetness2, samples);

        if (ds.columns.extra.testFlag(EC_LeafTemperature1))
            addGenericGraph(ds, EC_LeafTemperature1, samples);

        if (ds.columns.extra.testFlag(EC_LeafTemperature2))
            addGenericGraph(ds, EC_LeafTemperature2, samples);

        if (ds.columns.extra.testFlag(EC_SoilMoisture1))
            addGenericGraph(ds, EC_SoilMoisture1, samples);

        if (ds.columns.extra.testFlag(EC_SoilMoisture2))
            addGenericGraph(ds, EC_SoilMoisture2, samples);

        if (ds.columns.extra.testFlag(EC_SoilMoisture3))
            addGenericGraph(ds, EC_SoilMoisture3, samples);

        if (ds.columns.extra.testFlag(EC_SoilMoisture4))
            addGenericGraph(ds, EC_SoilMoisture4, samples);

        if (ds.columns.extra.testFlag(EC_SoilTemperature1))
            addGenericGraph(ds, EC_SoilTemperature1, samples);

        if (ds.columns.extra.testFlag(EC_SoilTemperature2))
            addGenericGraph(ds, EC_SoilTemperature2, samples);

        if (ds.columns.extra.testFlag(EC_SoilTemperature3))
            addGenericGraph(ds, EC_SoilTemperature3, samples);

        if (ds.columns.extra.testFlag(EC_SoilTemperature4))
            addGenericGraph(ds, EC_SoilTemperature4, samples);

        if (ds.columns.extra.testFlag(EC_ExtraHumidity1))
            addGenericGraph(ds, EC_ExtraHumidity1, samples);

        if (ds.columns.extra.testFlag(EC_ExtraHumidity2))
            addGenericGraph(ds, EC_ExtraHumidity2, samples);

        if (ds.columns.extra.testFlag(EC_ExtraTemperature1))
            addGenericGraph(ds, EC_ExtraTemperature1, samples);

        if (ds.columns.extra.testFlag(EC_ExtraTemperature2))
            addGenericGraph(ds, EC_ExtraTemperature2, samples);

        if (ds.columns.extra.testFlag(EC_ExtraTemperature3))
            addGenericGraph(ds, EC_ExtraTemperature3, samples);
    }
}

void WeatherPlotter::drawChart(QMap<dataset_id_t, SampleSet> sampleSets)
{
    qDebug() << "Drawing Chart...";

    addGraphs(sampleSets);

    bool legendWasVisible = chart->legend->visible();
    if (chart->graphCount() > 1)
        chart->legend->setVisible(true);
    else
        chart->legend->setVisible(false);

    bool legendIsVisible = chart->legend->visible();

    if (legendIsVisible != legendWasVisible) {
        emit legendVisibilityChanged(legendIsVisible);
    }

    multiRescale();
    chart->replot();
}

void WeatherPlotter::rescale() {
    multiRescale(currentScaleType);
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
    currentScaleType = rs_type;
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

#ifdef FEATURE_PLUS_CURSOR
            if (cursorAxisTags.contains(type)) {
                QPointer<QCPItemText> tag = cursorAxisTags[type];
                if (!tag.isNull()) {
                    chart->removeItem(tag.data());
                    qDebug() << "Tag for axis" << type << "is null?" << tag.isNull();
                }
                cursorAxisTags.remove(type);
            }
#endif

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
    emit axisCountChanged(configuredValueAxes.count(), configuredKeyAxes.count());
}

SampleColumns WeatherPlotter::availableColumns(dataset_id_t dataSetId)
{
    //SampleColumns availableColumns = ~currentChartColumns;
    SampleColumns availableColumns;
    availableColumns.standard = ~dataSets[dataSetId].columns.standard;
    availableColumns.extra = ~dataSets[dataSetId].columns.extra;

    // This will have gone and set all the unused bits in the int too.
    // Go clear anything we don't use.
    availableColumns.standard &= ALL_SAMPLE_COLUMNS;
    availableColumns.extra &= ALL_EXTRA_COLUMNS;

    // Then unset the timestamp flag if its set (its not a valid option here).
    if (availableColumns.standard.testFlag(SC_Timestamp))
        availableColumns.standard &= ~SC_Timestamp;

    return availableColumns;
}

SampleColumns WeatherPlotter::selectedColumns(dataset_id_t dataSetId) {
    return dataSets[dataSetId].columns;
}

void WeatherPlotter::addGraphs(dataset_id_t dataSetId, SampleColumns columns) {
    dataSets[dataSetId].columns.standard |= columns.standard;
    dataSets[dataSetId].columns.extra |= columns.extra;

    chart->clearPlottables();

    cacheManager->getDataSets(dataSets.values());
}

QCPGraph* WeatherPlotter::getGraph(dataset_id_t dataSetId, StandardColumn column) {
    QCPGraph* graph = 0;
    for (int i = 0; i < chart->graphCount(); i++) {
        QString columnType = chart->graph(i)->property(COLUMN_TYPE).toString();
        if (columnType != "standard") continue;

        StandardColumn graphColumn =
                (StandardColumn)chart->graph(i)->property(GRAPH_TYPE).toInt();
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

QCPGraph* WeatherPlotter::getGraph(dataset_id_t dataSetId, ExtraColumn column) {
    QCPGraph* graph = 0;
    for (int i = 0; i < chart->graphCount(); i++) {
        QString columnType = chart->graph(i)->property(COLUMN_TYPE).toString();
        if (columnType != "extra") continue;

        ExtraColumn graphColumn =
                (ExtraColumn)chart->graph(i)->property(GRAPH_TYPE).toInt();
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
                                 StandardColumn column) {
    // Remove the graph from the dataset so it doesn't magically come back later
    dataSets[dataSetId].columns.standard &= ~column;

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

void WeatherPlotter::removeGraph(QCPGraph* graph, dataset_id_t dataSetId,
                                 ExtraColumn column) {
    // Remove the graph from the dataset so it doesn't magically come back later
    dataSets[dataSetId].columns.extra &= ~column;

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

void WeatherPlotter::removeGraph(dataset_id_t dataSetId, StandardColumn column) {
    qDebug() << "Removing graph" << column << "for data set" << dataSetId;
    // Try to find the graph that goes with this column.

    QCPGraph *graph = getGraph(dataSetId, column);
    if (graph == NULL) {
        return;
    }

    removeGraph(graph, dataSetId, column);

    removeUnusedAxes();

    if (dataSets[dataSetId].columns.standard == SC_NoColumns && dataSets[dataSetId].columns.extra == EC_NoColumns) {
        // The dataset doesn't have any graphs left in the chart. Remove the
        // dataset itself if its not the last one remaining.
        if (dataSets.count() > 1) {
            removeDataSet(dataSetId);
        }
    }

    chart->replot();
}

void WeatherPlotter::removeGraph(dataset_id_t dataSetId, ExtraColumn column) {
    qDebug() << "Removing graph" << column << "for data set" << dataSetId;
    // Try to find the graph that goes with this column.

    QCPGraph *graph = getGraph(dataSetId, column);
    if (graph == NULL) {
        return;
    }

    removeGraph(graph, dataSetId, column);

    removeUnusedAxes();

    if (dataSets[dataSetId].columns.standard == SC_NoColumns && dataSets[dataSetId].columns.extra == EC_NoColumns) {
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

    columns.standard &= dsColumns.standard;
    columns.extra &= dsColumns.extra;

    QList<StandardColumn> columnList;
    QList<ExtraColumn> extraColumnList;

    if (columns.standard.testFlag(SC_Temperature))
        columnList << SC_Temperature;

    if (columns.standard.testFlag(SC_IndoorTemperature))
        columnList << SC_IndoorTemperature;

    if (columns.standard.testFlag(SC_ApparentTemperature))
        columnList << SC_ApparentTemperature;

    if (columns.standard.testFlag(SC_DewPoint))
        columnList << SC_DewPoint;

    if (columns.standard.testFlag(SC_WindChill))
        columnList << SC_WindChill;

    if (columns.standard.testFlag(SC_Humidity))
        columnList << SC_Humidity;

    if (columns.standard.testFlag(SC_IndoorHumidity))
        columnList << SC_IndoorHumidity;

    if (columns.standard.testFlag(SC_Pressure))
        columnList << SC_Pressure;

    if (columns.standard.testFlag(SC_Rainfall))
        columnList << SC_Rainfall;

    if (columns.standard.testFlag(SC_AverageWindSpeed))
        columnList << SC_AverageWindSpeed;

    if (columns.standard.testFlag(SC_GustWindSpeed))
        columnList << SC_GustWindSpeed;

    if (columns.standard.testFlag(SC_WindDirection))
        columnList << SC_WindDirection;

    if (columns.standard.testFlag(SC_UV_Index))
        columnList << SC_UV_Index;

    if (columns.standard.testFlag(SC_SolarRadiation))
        columnList << SC_SolarRadiation;

    if (columns.standard.testFlag(SC_HighTemperature))
        columnList << SC_HighTemperature;

    if (columns.standard.testFlag(SC_LowTemperature))
        columnList << SC_LowTemperature;

    if (columns.standard.testFlag(SC_HighSolarRadiation))
        columnList << SC_HighSolarRadiation;

    if (columns.standard.testFlag(SC_HighUVIndex))
        columnList << SC_HighUVIndex;

    if (columns.standard.testFlag(SC_GustWindDirection))
        columnList << SC_GustWindDirection;

    if (columns.standard.testFlag(SC_HighRainRate))
        columnList << SC_HighRainRate;

    if (columns.standard.testFlag(SC_Reception))
        columnList << SC_Reception;

    if (columns.standard.testFlag(SC_Evapotranspiration))
        columnList << SC_Evapotranspiration;

    for(int i = 0; i < columnList.count(); i++) {
        StandardColumn column = columnList[i];

        QCPGraph *graph = getGraph(dataSetId, column);
        if (graph == NULL) {
            // Graph is already missing?
            continue;
        }

        removeGraph(graph, dataSetId, column);
    }

    if (columns.extra.testFlag(EC_LeafWetness1))
        extraColumnList << EC_LeafWetness1;

    if (columns.extra.testFlag(EC_LeafWetness2))
        extraColumnList << EC_LeafWetness2;

    if (columns.extra.testFlag(EC_LeafTemperature1))
        extraColumnList << EC_LeafTemperature1;

    if (columns.extra.testFlag(EC_LeafTemperature2))
        extraColumnList << EC_LeafTemperature2;

    if (columns.extra.testFlag(EC_SoilMoisture1))
        extraColumnList << EC_SoilMoisture1;

    if (columns.extra.testFlag(EC_SoilMoisture2))
        extraColumnList << EC_SoilMoisture2;

    if (columns.extra.testFlag(EC_SoilMoisture3))
        extraColumnList << EC_SoilMoisture3;

    if (columns.extra.testFlag(EC_SoilMoisture4))
        extraColumnList << EC_SoilMoisture4;

    if (columns.extra.testFlag(EC_SoilTemperature1))
        extraColumnList << EC_SoilTemperature1;

    if (columns.extra.testFlag(EC_SoilTemperature2))
        extraColumnList << EC_SoilTemperature2;

    if (columns.extra.testFlag(EC_SoilTemperature3))
        extraColumnList << EC_SoilTemperature3;

    if (columns.extra.testFlag(EC_SoilTemperature4))
        extraColumnList << EC_SoilTemperature4;

    if (columns.extra.testFlag(EC_ExtraHumidity1))
        extraColumnList << EC_ExtraHumidity1;

    if (columns.extra.testFlag(EC_ExtraHumidity2))
        extraColumnList << EC_ExtraHumidity2;

    if (columns.extra.testFlag(EC_ExtraTemperature1))
        extraColumnList << EC_ExtraTemperature1;

    if (columns.extra.testFlag(EC_ExtraTemperature2))
        extraColumnList << EC_ExtraTemperature2;

    if (columns.extra.testFlag(EC_ExtraTemperature3))
        extraColumnList << EC_ExtraTemperature3;

    for(int i = 0; i < extraColumnList.count(); i++) {
        ExtraColumn column = extraColumnList[i];

        QCPGraph *graph = getGraph(dataSetId, column);
        if (graph == NULL) {
            // Graph is already missing?
            continue;
        }

        removeGraph(graph, dataSetId, column);
    }

    removeUnusedAxes();

    if (dataSets[dataSetId].columns.standard == SC_NoColumns && dataSets[dataSetId].columns.extra == EC_NoColumns) {
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

graph_styles_t WeatherPlotter::getGraphStyles(dataset_id_t dataSetId) {
    graph_styles_t styles;
    styles.standardStyles = graphStyles[dataSetId];
    styles.extraStyles = extraGraphStyles[dataSetId];

    return styles;
}

GraphStyle& WeatherPlotter::getStyleForGraph(dataset_id_t dataSetId, StandardColumn column) {
    return graphStyles[dataSetId][column];
}

GraphStyle& WeatherPlotter::getStyleForGraph(dataset_id_t dataSetId, ExtraColumn column) {
    return extraGraphStyles[dataSetId][column];
}

GraphStyle& WeatherPlotter::getStyleForGraph(QCPGraph* graph) {
    dataset_id_t dataset = graph->property(GRAPH_DATASET).toInt();

    QString columnType = graph->property(COLUMN_TYPE).toString();
    if (columnType == "standard") {
        StandardColumn col = (StandardColumn)graph->property(GRAPH_TYPE).toInt();

        return getStyleForGraph(dataset, col);
    } else {
        ExtraColumn col = (ExtraColumn)graph->property(GRAPH_TYPE).toInt();
        return getStyleForGraph(dataset, col);
    }
}

void WeatherPlotter::setGraphStyles(QMap<StandardColumn, GraphStyle> styles, dataset_id_t dataSetId) {
    graphStyles[dataSetId] = styles;
}

void WeatherPlotter::setGraphStyles(QMap<ExtraColumn, GraphStyle> styles, dataset_id_t dataSetId) {
    extraGraphStyles[dataSetId] = styles;
}

#ifdef FEATURE_PLUS_CURSOR
void WeatherPlotter::setCursorEnabled(bool enabled) {
    this->cursorEnabled = enabled;

    if (!enabled) {
        hideCursor();
    }
}

bool WeatherPlotter::isCursorEnabled() {
    return cursorEnabled;
}

void WeatherPlotter::hideCursor() {
    if (!hCursor.isNull()) {
        hCursor->setVisible(false);
    }
    if (!vCursor.isNull()) {
        vCursor->setVisible(false);
    }

    foreach (int type, cursorAxisTags.keys()) {
        if (!cursorAxisTags[type].isNull()) {
            cursorAxisTags[type]->setVisible(false);
        }
    }

    chart->layer("overlay")->replot();
}

void WeatherPlotter::updateCursor(QMouseEvent *event) {

    if (!this->cursorEnabled) {
        return;
    }

    if (hCursor.isNull() || vCursor.isNull()) {
        return; // Cursor not initialised.
    }

    if (configuredKeyAxes.isEmpty() || configuredValueAxes.isEmpty()) {
        hCursor->setVisible(false);
        vCursor->setVisible(false);
        return; // There shouldn't be any graphs when there are no key or value axes.
    }

    if (!chart->rect().contains(event->pos())) {
        // Mouse has left the widget. Hide the cursor
        hideCursor();
        return;
    }

    // Update the cursor
    vCursor->start->setCoords(event->pos().x(),0);
    vCursor->end->setCoords(event->pos().x(), chart->height());
    vCursor->setVisible(true);

    hCursor->start->setCoords(0, event->pos().y());
    hCursor->end->setCoords(chart->width(), event->pos().y());
    hCursor->setVisible(true);

    // Update the tags
    foreach (int type, cursorAxisTags.keys()) {
        QPointer<QCPItemText> tag = cursorAxisTags[type];
        if (tag.isNull()) {
            qWarning() << "Tag for axis type" << type << "is null.";
            continue;
        }

        if ((AxisType)type < AT_KEY) {
            // Its a value axis (Y)
            QCPAxis* axis = getValueAxis((AxisType)type, false);

            QPointer<QCPAxis> keyAxis = tag->position->keyAxis();

            double axisValue = axis->pixelToCoord(event->pos().y());

            QCPRange range = axis->range();
            if (axisValue < range.lower || axisValue > range.upper) {
                tag->setVisible(false);
            } else {
                tag->setVisible(true);
                if (type == AT_HUMIDITY) {
                    tag->setText(QString::number(axisValue, 'f', 0));
                } else {
                    tag->setText(QString::number(axisValue, 'f', 1));
                }

                if (axis->axisType() == QCPAxis::atLeft) {
                    tag->position->setCoords(
                                keyAxis->pixelToCoord(chart->axisRect()->bottomLeft().x() - axis->offset()), axisValue);
                } else {
                    // +1 to align with axis rect border
                    tag->position->setCoords(
                                keyAxis->pixelToCoord(chart->axisRect()->bottomRight().x() + axis->offset() + 1),
                                axisValue);
                }
            }

        } else {
            // Its a key axis (X)

            dataset_id_t dataSet = type - AT_KEY;

            QCPAxis* axis = getKeyAxis(dataSet, false);

            double axisValue = axis->pixelToCoord(event->pos().x());

            QCPRange r = axis->range();
            if (axisValue < r.lower || axisValue > r.upper) {
                tag->setVisible(false);
            } else {
                tag->setVisible(true);

                tag->setText(QDateTime::fromMSecsSinceEpoch(axisValue * 1000).toString(Qt::SystemLocaleShortDate));

                QPointer<QCPAxis> valueAxis = tag->position->valueAxis();
                double valueZero = valueAxis->pixelToCoord(chart->axisRect()->bottomLeft().y());
                double valueMax = valueAxis->pixelToCoord(chart->axisRect()->topRight().y() -1); // -1 to align with border

                QFontMetrics m(tag->font());
                double halfWidth = m.width(tag->text()) / 2;

                double left = chart->axisRect()->bottomLeft().x();
                double right = chart->axisRect()->bottomRight().x();


                double minPos = axis->pixelToCoord(halfWidth + left);
                double maxPos = axis->pixelToCoord(right - halfWidth);

                // Prevent the tag from going off the end of the chart.
                double xValue = axisValue;
                if (xValue < minPos) {
                    xValue = minPos;
                } else if (xValue > maxPos) {
                    xValue = maxPos;
                }

                if (axis->axisType() == QCPAxis::atTop) {
                    // +1 to align with axis rect border
                    tag->position->setCoords(xValue, valueMax);
                } else {
                    tag->position->setCoords(xValue, valueZero);
                }
            }
        }

    }

    chart->layer("overlay")->replot();
}
#endif
