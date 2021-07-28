#include "liveplotwindow.h"
#include "ui_liveplotwindow.h"

#include <QtDebug>

#include "datasource/abstractlivedatasource.h"
#include "datasource/databasedatasource.h"
#include "datasource/webdatasource.h"
#include "datasource/tcplivedatasource.h"
#include "datasource/dialogprogresslistener.h"
#include "datasource/livebuffer.h"
#include "qcp/qcustomplot.h"
#include "settings.h"
#include "constants.h"
#include "addlivegraphdialog.h"
#include "nonaggregatingliveaggregator.h"
#include "averagedliveaggregator.h"
#include "livechartoptionsdialog.h"
#include "plotwidget/valueaxistag.h"
#include "plotwidget/chartmousetracker.h"
#include "plotwidget/pluscursor.h"

#define PROP_GRAPH_TYPE "graph_type"
#define PROP_IS_POINT "is_point"

UnitConversions::unit_t metricUnitToImperial(UnitConversions::unit_t unit);

#define imperialiseUnitDict(type) units[type] = metricUnitToImperial(units[type])

LivePlotWindow::LivePlotWindow(LiveValues initialGraphs,
                               bool solarAvailalble,
                               hardware_type_t hardwareType,
                               ExtraColumns extraColumns,
                               QMap<ExtraColumn, QString> extraColumnNames,
                               QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LivePlotWindow)
{
    ui->setupUi(this);

    Settings& settings = Settings::getInstance();

    connect(ui->actionCrosshair, SIGNAL(triggered()), this, SLOT(toggleCursor()));
    connect(ui->actionTrack_Cursor, SIGNAL(triggered(bool)), this, SLOT(setMouseTrackingEnabled(bool)));
    ui->actionTrack_Cursor->setChecked(settings.liveChartTracksMouseEnabled());
    ui->actionCrosshair->setChecked(settings.liveChartCursorEnabled());

    imperial = settings.imperial();
    kmh = settings.kmh();

    this->hwType = hardwareType;
    this->solarAvailable = solarAvailalble;
    this->valuesToShow = LV_NoColumns;
    this->extraColumns = extraColumns;
    this->extraColumnNames = extraColumnNames;

    // All the possible axis unit types
    units[LV_Temperature] = UnitConversions::U_CELSIUS;
    units[LV_IndoorTemperature] = UnitConversions::U_CELSIUS;
    units[LV_ApparentTemperature] = UnitConversions::U_CELSIUS;
    units[LV_WindChill] = UnitConversions::U_CELSIUS;
    units[LV_DewPoint] = UnitConversions::U_CELSIUS;
    units[LV_Humidity] = UnitConversions::U_HUMIDITY;
    units[LV_IndoorHumidity] = UnitConversions::U_HUMIDITY;
    units[LV_Pressure] = UnitConversions::U_HECTOPASCALS;
    if (kmh) {
        units[LV_WindSpeed] = UnitConversions::U_KILOMETERS_PER_HOUR;
    } else {
        units[LV_WindSpeed] = UnitConversions::U_METERS_PER_SECOND;
    }
    units[LV_WindDirection] = UnitConversions::U_DEGREES;
    units[LV_StormRain] = UnitConversions::U_MILLIMETERS;
    units[LV_RainRate] = UnitConversions::U_MILLIMETERS_PER_HOUR;
    units[LV_BatteryVoltage] = UnitConversions::U_VOLTAGE;
    units[LV_UVIndex] = UnitConversions::U_UV_INDEX;
    units[LV_SolarRadiation] = UnitConversions::U_WATTS_PER_SQUARE_METER;
    units[LV_SoilTemperature1] = UnitConversions::U_CELSIUS;
    units[LV_SoilTemperature2] = UnitConversions::U_CELSIUS;
    units[LV_SoilTemperature3] = UnitConversions::U_CELSIUS;
    units[LV_SoilTemperature4] = UnitConversions::U_CELSIUS;
    units[LV_LeafTemperature1] = UnitConversions::U_CELSIUS;
    units[LV_LeafTemperature2] = UnitConversions::U_CELSIUS;
    units[LV_ExtraTemperature1] = UnitConversions::U_CELSIUS;
    units[LV_ExtraTemperature2] = UnitConversions::U_CELSIUS;
    units[LV_ExtraTemperature3] = UnitConversions::U_CELSIUS;
    units[LV_ExtraHumidity1] = UnitConversions::U_HUMIDITY;
    units[LV_ExtraHumidity2] = UnitConversions::U_HUMIDITY;
    units[LV_SoilMoisture1] = UnitConversions::U_CENTIBAR;
    units[LV_SoilMoisture2] = UnitConversions::U_CENTIBAR;
    units[LV_SoilMoisture3] = UnitConversions::U_CENTIBAR;
    units[LV_SoilMoisture4] = UnitConversions::U_CENTIBAR;
    units[LV_LeafWetness1] = UnitConversions::U_LEAF_WETNESS;
    units[LV_LeafWetness2] = UnitConversions::U_LEAF_WETNESS;

    // Axis types. We need this for PlusCursor to work :(
    axisTypes[LV_Temperature] = AT_TEMPERATURE;
    axisTypes[LV_IndoorTemperature] = AT_TEMPERATURE;
    axisTypes[LV_ApparentTemperature] = AT_TEMPERATURE;
    axisTypes[LV_WindChill] = AT_TEMPERATURE;
    axisTypes[LV_DewPoint] = AT_TEMPERATURE;
    axisTypes[LV_Humidity] = AT_HUMIDITY;
    axisTypes[LV_IndoorHumidity] = AT_HUMIDITY;
    axisTypes[LV_Pressure] = AT_PRESSURE;
    axisTypes[LV_WindSpeed] = AT_WIND_SPEED;
    axisTypes[LV_WindDirection] = AT_WIND_DIRECTION;
    axisTypes[LV_StormRain] = AT_RAINFALL;
    axisTypes[LV_RainRate] = AT_RAIN_RATE;
    axisTypes[LV_BatteryVoltage] = AT_VOLTAGE;
    axisTypes[LV_UVIndex] = AT_UV_INDEX;
    axisTypes[LV_SolarRadiation] = AT_SOLAR_RADIATION;
    axisTypes[LV_SoilTemperature1] = AT_TEMPERATURE;
    axisTypes[LV_SoilTemperature2] = AT_TEMPERATURE;
    axisTypes[LV_SoilTemperature3] = AT_TEMPERATURE;
    axisTypes[LV_SoilTemperature4] = AT_TEMPERATURE;
    axisTypes[LV_LeafTemperature1] = AT_TEMPERATURE;
    axisTypes[LV_LeafTemperature2] = AT_TEMPERATURE;
    axisTypes[LV_ExtraTemperature1] = AT_TEMPERATURE;
    axisTypes[LV_ExtraTemperature2] = AT_TEMPERATURE;
    axisTypes[LV_ExtraTemperature3] = AT_TEMPERATURE;
    axisTypes[LV_ExtraHumidity1] = AT_HUMIDITY;
    axisTypes[LV_ExtraHumidity2] = AT_HUMIDITY;
    axisTypes[LV_SoilMoisture1] = AT_SOIL_MOISTURE;
    axisTypes[LV_SoilMoisture2] = AT_SOIL_MOISTURE;
    axisTypes[LV_SoilMoisture3] = AT_SOIL_MOISTURE;
    axisTypes[LV_SoilMoisture4] = AT_SOIL_MOISTURE;
    axisTypes[LV_LeafWetness1] = AT_LEAF_WETNESS;
    axisTypes[LV_LeafWetness2] = AT_LEAF_WETNESS;

    if (imperial) {
        imperialiseUnitDict(LV_Temperature);
        imperialiseUnitDict(LV_IndoorTemperature);
        imperialiseUnitDict(LV_ApparentTemperature);
        imperialiseUnitDict(LV_WindChill);
        imperialiseUnitDict(LV_DewPoint);
        imperialiseUnitDict(LV_Pressure);
        imperialiseUnitDict(LV_WindSpeed);
        imperialiseUnitDict(LV_StormRain);
        imperialiseUnitDict(LV_RainRate);
        imperialiseUnitDict(LV_SoilTemperature1);
        imperialiseUnitDict(LV_SoilTemperature2);
        imperialiseUnitDict(LV_SoilTemperature3);
        imperialiseUnitDict(LV_SoilTemperature4);
        imperialiseUnitDict(LV_LeafTemperature1);
        imperialiseUnitDict(LV_LeafTemperature2);
        imperialiseUnitDict(LV_ExtraTemperature1);
        imperialiseUnitDict(LV_ExtraTemperature2);
        imperialiseUnitDict(LV_ExtraTemperature3);
    }

    // Suffixes for axis labels based on type
    axisLabelUnitSuffixes[UnitConversions::U_CELSIUS] = tr(DEGREE_SYMBOL "C");
    axisLabelUnitSuffixes[UnitConversions::U_FAHRENHEIT] = tr(DEGREE_SYMBOL "F");
    axisLabelUnitSuffixes[UnitConversions::U_HUMIDITY] = tr("%");
    axisLabelUnitSuffixes[UnitConversions::U_HECTOPASCALS] = tr("hPa");
    axisLabelUnitSuffixes[UnitConversions::U_INCHES_OF_MERCURY] = tr("InHg");
    axisLabelUnitSuffixes[UnitConversions::U_METERS_PER_SECOND] = tr("m/s");
    axisLabelUnitSuffixes[UnitConversions::U_KILOMETERS_PER_HOUR] = tr("km/h");
    axisLabelUnitSuffixes[UnitConversions::U_MILES_PER_HOUR] = tr("mph");
    axisLabelUnitSuffixes[UnitConversions::U_DEGREES] = tr(DEGREE_SYMBOL);
    axisLabelUnitSuffixes[UnitConversions::U_MILLIMETERS] = tr("mm");
    axisLabelUnitSuffixes[UnitConversions::U_INCHES] = tr("in");
    axisLabelUnitSuffixes[UnitConversions::U_MILLIMETERS_PER_HOUR] = tr("mm/h");
    axisLabelUnitSuffixes[UnitConversions::U_INCHES_PER_HOUR] = tr("in/h");
    axisLabelUnitSuffixes[UnitConversions::U_VOLTAGE] = tr("V");
    axisLabelUnitSuffixes[UnitConversions::U_UV_INDEX] = tr("");
    axisLabelUnitSuffixes[UnitConversions::U_WATTS_PER_SQUARE_METER] = tr("W/m" SQUARED_SYMBOL);
    axisLabelUnitSuffixes[UnitConversions::U_CENTIBAR] = tr("cbar");
    axisLabelUnitSuffixes[UnitConversions::U_LEAF_WETNESS] = tr("");

    // And the typed axis labels
    axisLabels[UnitConversions::U_CELSIUS] = tr("Temperature");
    axisLabels[UnitConversions::U_FAHRENHEIT] = tr("Fahrenheit");
    axisLabels[UnitConversions::U_HUMIDITY] = tr("Humidity");
    axisLabels[UnitConversions::U_HECTOPASCALS] = tr("Pressure");
    axisLabels[UnitConversions::U_INCHES_OF_MERCURY] = tr("Inches of Mercury");
    axisLabels[UnitConversions::U_METERS_PER_SECOND] = tr("Wind Speed");
    axisLabels[UnitConversions::U_KILOMETERS_PER_HOUR] = tr("Wind Speed");
    axisLabels[UnitConversions::U_MILES_PER_HOUR] = tr("Wind Speed");
    axisLabels[UnitConversions::U_DEGREES] = tr("Wind direction");
    axisLabels[UnitConversions::U_MILLIMETERS] = tr("Rainfall");
    axisLabels[UnitConversions::U_INCHES] = tr("Rainfall");
    axisLabels[UnitConversions::U_MILLIMETERS_PER_HOUR] = tr("Rain Rate");
    axisLabels[UnitConversions::U_INCHES_PER_HOUR] = tr("Rain rate");
    axisLabels[UnitConversions::U_VOLTAGE] = tr("Voltage");
    axisLabels[UnitConversions::U_UV_INDEX] = tr("UV Index");
    axisLabels[UnitConversions::U_WATTS_PER_SQUARE_METER] = tr("Solar Radiation");
    axisLabels[UnitConversions::U_CENTIBAR] = tr("Soil Moisture");
    axisLabels[UnitConversions::U_LEAF_WETNESS] = tr("Leaf Wetness");

    // Value name...
    valueNames[LV_Temperature] = tr("Temperature");
    valueNames[LV_IndoorTemperature] = tr("Inside Temperature");
    valueNames[LV_ApparentTemperature] = tr("Apparent Temperature");
    valueNames[LV_WindChill] = tr("Wind Chill");
    valueNames[LV_DewPoint] = tr("Dew Point");
    valueNames[LV_Humidity] = tr("Humidity");
    valueNames[LV_IndoorHumidity] = tr("Indoor Humidity");
    valueNames[LV_Pressure] = tr("Pressure");
    valueNames[LV_WindSpeed] = tr("Wind Speed");
    valueNames[LV_WindDirection] = tr("Wind Direction");
    valueNames[LV_StormRain] = tr("Storm Rain");
    valueNames[LV_RainRate] = tr("Rain Rate");
    valueNames[LV_BatteryVoltage] = tr("Battery Voltage");
    valueNames[LV_UVIndex] = tr("UV Index");
    valueNames[LV_SolarRadiation] = tr("Solar Radiation");

    // Really need a better way of handling this metadata...
    extraColumnMapping[LV_SoilMoisture1] = EC_SoilMoisture1;
    extraColumnMapping[LV_SoilMoisture2] = EC_SoilMoisture2;
    extraColumnMapping[LV_SoilMoisture3] = EC_SoilMoisture3;
    extraColumnMapping[LV_SoilMoisture4] = EC_SoilMoisture4;
    extraColumnMapping[LV_SoilTemperature1] = EC_SoilTemperature1;
    extraColumnMapping[LV_SoilTemperature2] = EC_SoilTemperature2;
    extraColumnMapping[LV_SoilTemperature3] = EC_SoilTemperature3;
    extraColumnMapping[LV_SoilTemperature4] = EC_SoilTemperature4;
    extraColumnMapping[LV_LeafWetness1] = EC_LeafWetness1;
    extraColumnMapping[LV_LeafWetness2] = EC_LeafWetness2;
    extraColumnMapping[LV_LeafTemperature1] = EC_LeafTemperature1;
    extraColumnMapping[LV_LeafTemperature2] = EC_LeafTemperature2;
    extraColumnMapping[LV_ExtraTemperature1] = EC_ExtraTemperature1;
    extraColumnMapping[LV_ExtraTemperature2] = EC_ExtraTemperature2;
    extraColumnMapping[LV_ExtraTemperature3] = EC_ExtraTemperature3;
    extraColumnMapping[LV_ExtraHumidity1] = EC_ExtraHumidity1;
    extraColumnMapping[LV_ExtraHumidity2] = EC_ExtraHumidity2;

    bool usingWebDs = false;

    switch(settings.liveDataSourceType()) {
    case Settings::DS_TYPE_DATABASE:
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
        break;
    case Settings::DS_TYPE_SERVER:
        ds = new TcpLiveDataSource(this);
        break;
    case Settings::DS_TYPE_WEB_INTERFACE:
        ds = new WebDataSource(new DialogProgressListener(this), this);
        usingWebDs = true;
        break;
    }

    // This is to give the averaging aggregator a constant stream of updates
    // so it keeps producing new samples when the weater station goes quiet.
    repeater.reset(new LiveDataRepeater(usingWebDs, this));
    connect(ds, SIGNAL(liveData(LiveDataSet)), repeater.data(),
            SLOT(incomingLiveData(LiveDataSet)));

    // Setup the aggregator
    aggregateSeconds = settings.liveAggregateSeconds();
    aggregate = settings.liveAggregate();
    maxRainRate = settings.liveMaxRainRate();
    stormRain = settings.liveStormRain();
    timespanMinutes = settings.liveTimespanMinutes();
    axisTags = settings.liveTagsEnabled();
    multipleAxisRects = settings.liveMultipleAxisRectsEnabled();

    if (aggregate) {
        aggregator.reset(new AveragedLiveAggregator(
                             aggregateSeconds, maxRainRate, stormRain, this));
    } else {
        aggregator.reset(new NonAggregatingLiveAggregator(stormRain, this));
    }

    connect(repeater.data(), SIGNAL(liveData(LiveDataSet)),
            aggregator.data(), SLOT(incomingLiveData(LiveDataSet)));
    connect(aggregator.data(), SIGNAL(liveData(LiveDataSet)),
            this, SLOT(liveData(LiveDataSet)));

    resetPlot();

    // Hookup toolbar buttons
    connect(ui->actionAdd_Graph, SIGNAL(triggered(bool)),
            this, SLOT(showAddGraphDialog()));

    connect(ui->actionOptions, SIGNAL(triggered(bool)),
            this, SLOT(showOptions()));

    addLiveValues(initialGraphs);


    resetData();

    // Load initial data
//    QDateTime minTime = QDateTime::currentDateTime().addSecs(0-timespanMinutes*60);
//    foreach (LiveDataSet lds, LiveBuffer::getInstance().getData()) {
//        if (lds.timestamp > minTime) {
//            //liveData(lds);
//            qDebug() << "Initial live data: " << lds.timestamp;
//            aggregator->incomingLiveData(lds);
//        }
//    }

    ds->enableLiveData();
}

UnitConversions::unit_t metricUnitToImperial(UnitConversions::unit_t unit) {
    switch(unit) {
    case UnitConversions::U_METERS_PER_SECOND:
    case UnitConversions::U_KILOMETERS_PER_HOUR:
        return UnitConversions::U_MILES_PER_HOUR;
    case UnitConversions::U_CELSIUS:
        return UnitConversions::U_FAHRENHEIT;
    case UnitConversions::U_HECTOPASCALS:
        return UnitConversions::U_INCHES_OF_MERCURY;
    case UnitConversions::U_MILLIMETERS:
    case UnitConversions::U_CENTIMETERS:
        return UnitConversions::U_INCHES;
    case UnitConversions::U_MILLIMETERS_PER_HOUR:
    case UnitConversions::U_CENTIMETERS_PER_HOUR:
        return UnitConversions::U_INCHES_PER_HOUR;
    default:
        return unit;
    }
}

double metricToImperial(LiveValue v, double value) {
    switch(v) {
    case LV_Temperature:
    case LV_IndoorTemperature:
    case LV_ApparentTemperature:
    case LV_WindChill:
    case LV_DewPoint:
        return UnitConversions::celsiusToFahrenheit(value);
    case LV_Pressure:
        return UnitConversions::hectopascalsToInchesOfMercury(value);
    case LV_WindSpeed:
        return UnitConversions::metersPerSecondToMilesPerHour(value);
    case LV_StormRain:
    case LV_RainRate:
        return UnitConversions::millimetersToInches(value);
    default:
    return value;

    }
}

LivePlotWindow::~LivePlotWindow()
{
    delete ui;
}

LiveValues LivePlotWindow::liveValues() {
    return valuesToShow;
}

/** returns true if the required axis rect for the specified graph exists
 *
 * @param type Graph type
 * @return True if an axis rect for that graph type exists
 */
bool LivePlotWindow::axisRectExists(LiveValue type) {
    if (multipleAxisRects) {
        return axisRects.contains(type);
    }

    return ui->plot->axisRectCount() > 0;
}

/** Creates an axis rect for the specified graph type. If an axis rect already exists
 * for the graph type it will be removed.
 *
 * @param type Type of graph to create an axis rect for
 * @return A new axis rect
 */
QCPAxisRect* LivePlotWindow::createAxisRectForGraph(LiveValue type) {
    qDebug() << "Creating axis rect for" << type;
    if (multipleAxisRects) {
        if (axisRects.contains(type)) {
            qDebug() << "Rect exists - removing";
            ui->plot->plotLayout()->remove(axisRects[type]);
        }

        if (marginGroup.isNull()) {
            marginGroup = new QCPMarginGroup(ui->plot);
        }

        QCPAxisRect *rect = new QCPAxisRect(ui->plot);
        rect->setupFullAxesBox(true);

        Settings& settings = Settings::getInstance();

        rect->axis(QCPAxis::atTop)->setVisible(false);
        rect->axis(QCPAxis::atBottom)->setProperty(AXIS_TYPE, AT_KEY);
        rect->axis(axisTags ? QCPAxis::atLeft : QCPAxis::atRight)->setVisible(false);

        rect->axis(QCPAxis::atBottom)->setTicker(ticker);
        rect->axis(QCPAxis::atBottom)->setTickLabelFont(settings.defaultChartAxisTickLabelFont());
        rect->axis(QCPAxis::atBottom)->setLabelFont(settings.defaultChartAxisLabelFont());
        rect->axis(QCPAxis::atLeft)->setTickLabelFont(settings.defaultChartAxisTickLabelFont());
        rect->axis(QCPAxis::atLeft)->setLabelFont(settings.defaultChartAxisLabelFont());
        rect->axis(QCPAxis::atRight)->setTickLabelFont(settings.defaultChartAxisTickLabelFont());
        rect->axis(QCPAxis::atRight)->setLabelFont(settings.defaultChartAxisLabelFont());

        axisRects[type] = rect;

        qDebug() << "Rect created. Adding to layout.";
        ui->plot->plotLayout()->addElement(rect);

        // Keep everything nice and aligned
        rect->setMarginGroup(QCP::msRight | QCP::msLeft, marginGroup);

        if (axisRects.count() > 1) {
            ensureLegend();
        }

        return rect;
    } else {
        qDebug() << "Creating default axis rect";
        ui->plot->recreateDefaultAxisRect();
        ui->plot->axisRect()->axis(QCPAxis::atLeft)->setVisible(false);
        ui->plot->axisRect()->axis(QCPAxis::atRight)->setVisible(false);
        ui->plot->axisRect()->axis(QCPAxis::atBottom)->setTicker(ticker);
        ui->plot->axisRect()->axis(QCPAxis::atBottom)->setProperty(AXIS_TYPE, AT_KEY);
        axis.clear();

        qDebug() << "Default rect created";

        return ui->plot->axisRect();
    }
}

void LivePlotWindow::legendVisibilityChanged(bool visible) {
    if (multipleAxisRects) {
        if (visible && ui->plot->legend == NULL) {
            ensureLegend(true);
            //ui->plot->legend->setVisible(true);
        } else {
            delete ui->plot->legend;
            delete legendLayout;
            legendLayout = NULL;
            ui->plot->legend = NULL;
            ui->plot->plotLayout()->simplify();
        }
    }

    ui->actionLegend->setChecked(visible);
}

void LivePlotWindow::ensureLegend(bool show) {
    bool legendCreated = false;

    if (ui->plot->legend == NULL) {
        qDebug() << "Create legend";
        ui->plot->legend = new QCPLegend;
        ui->plot->legend->setVisible(show);
        legendCreated = true;
    }

    if (legendLayout == NULL) {
        qDebug() << "Create legend layout";
        legendLayout = new QCPLayoutGrid;
        legendLayout->setMargins(QMargins(5, 0, 5, 5));

        // Chuck it in the layout to ensure the legend doesn't get separated
        // from the plot when we reparent it.
        ui->plot->plotLayout()->addElement(legendLayout);
    }

    if (legendCreated) {
        qDebug() << "Reparent legend";
        legendLayout->addElement(0, 0, ui->plot->legend);
        ui->plot->legend->setFillOrder(QCPLegend::foColumnsFirst);

        qDebug() << "Populating legend";
        for (int i = 0; i < ui->plot->graphCount(); i++) {
            QCPGraph *g = ui->plot->graph(i);
            if (!g->property(PROP_IS_POINT).toBool()) {
                g->addToLegend(ui->plot->legend);
            }
        }

    }

    moveLegendToBottom();

    if (legendCreated) {
        ui->plot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void LivePlotWindow::moveLegendToBottom() {
    if (legendLayout != NULL) {
        qDebug() << "Move legend layout to bottom of plot";
        // Shift the legend to the bottom
        ui->plot->plotLayout()->addElement(legendLayout);
        ui->plot->plotLayout()->simplify();
        ui->plot->plotLayout()->setRowStretchFactor(
                    ui->plot->plotLayout()->rowCount() - 1, 0.001);
    }
}


/** Gets an axis rect for the specified graph type. If one does not exist it will
 * be created.
 *
 * @param type Graph type to get an axis rect for.
 * @return
 */
QCPAxisRect* LivePlotWindow::axisRectForGraph(LiveValue type) {
    if (axisRectExists(type)) {
        if (multipleAxisRects) {
            return axisRects[type];
        } else {
            return ui->plot->axisRect();
        }
    }

    return createAxisRectForGraph(type);
}

QCPAxis* LivePlotWindow::keyAxisForGraph(LiveValue type) {
    bool newAxis = !axisRectExists(type);

    QCPAxisRect *rect = axisRectForGraph(type);

    QCPAxis *axis = rect->axis(QCPAxis::atBottom);

    if (newAxis) {
        axis->setVisible(true);
        axis->setTicker(ticker);
    }

    return axis;
}

QString LivePlotWindow::axisLabel(LiveValue value) {
    UnitConversions::unit_t u = units[value];

    QString suffix = axisLabelUnitSuffixes[u];
    QString base;

    if (multipleAxisRects) {
        switch(Settings::getInstance().liveMultipleAxisRectsAxisLabelType()) {
        case Settings::LMALT_SENSOR:
            if (valueNames.contains(value)) {
                base = valueNames[value];
            } else if (extraColumnMapping.contains(value)) {
                base = extraColumnNames[extraColumnMapping[value]];
            } else {
                base = axisLabels[u];
            }
            break;
        case Settings::LMALT_UNITS_ONLY:
            base = suffix;
            suffix = QString();
            break;
        case Settings::LMALT_TYPE:
        default:
            base = axisLabels[u];
            break;
        }
    } else {
        base = axisLabels[u];
    }

    if (suffix.isEmpty()) {
        return base;
    }
    return QString("%1 (%2)").arg(base).arg(suffix);
}

QCPAxis* LivePlotWindow::valueAxisForGraph(LiveValue type) {
    bool newAxis = !axisRectExists(type);

    QCPAxisRect *rect = axisRectForGraph(type);

    UnitConversions::unit_t axisType = units[type];

    QCPAxis* axis;

    if (multipleAxisRects) {
        // Axis rect per graph means we only ever have one value axis in each
        // axis rect. The side it will be on will depend on if axis tags are on or not.
        axis = rect->axis(axisTags ? QCPAxis::atRight : QCPAxis::atLeft);
    } else {
        // Multiple graphs in one axis rect means we'll have a whole bunch of value
        // axes depending on the units used by the various graphs in the rect.

        if (this->axis.contains(axisType)) {
            axis = this->axis[axisType];
            newAxis = false;
        } else {
            bool isLeft = (this->axis.count() % 2 == 0) && !axisTags;

            QCPAxis *y1 = ui->plot->axisRect()->axis(QCPAxis::atLeft);
            QCPAxis *y2 = ui->plot->axisRect()->axis(QCPAxis::atRight);

            // Use one of the initial axes if we're not doing axis tags and have less than
            // two axes allocated so far
            if(y1 != NULL && !y1->visible() && !axisTags) {
                this->axis[axisType] = y1;
                y1->setVisible(true);
            } else if (y2 != NULL && !y2->visible()) {
                this->axis[axisType] = y2;
                y2->setVisible(true);
            } else {
                // For more than two axes, create a new one at the opposite side from the
                // last axis created.
                this->axis[axisType] = ui->plot->axisRect()->addAxis(
                            isLeft ? QCPAxis::atLeft : QCPAxis::atRight);
            }

            axis = this->axis[axisType];
            newAxis = true;
        }
    }

    if (newAxis) {
        // If the axis rect didn't exist before then we'll need to setup the value
        // axis.
        axis->setVisible(true);
        axis->setTickLabels(true);
        axis->setProperty(AXIS_TYPE, axisTypes[type]);

        axis->setLabel(axisLabel(type));

        if (axisTags) {
            axis->setPadding(10);
            axis->setLabelPadding(30);
        }

        if (axisType == UnitConversions::U_LEAF_WETNESS) {
            axis->setRange(0,15);
        }
    }

    return axis;
}

#define SET_EC_STYLE_NAME(lv, ec) if (v == lv) {style.setName(extraColumnNames.value(ec));}

void LivePlotWindow::addLiveValue(LiveValue v) {
    valuesToShow |= v;

    //UnitConversions::unit_t axisType = units[v];

    // These will create any axes and axis rects if they don't already exist.
    QCPAxis *valueAxis = valueAxisForGraph(v);
    QCPAxis *keyAxis = keyAxisForGraph(v);

    Q_ASSERT_X(valueAxis->axisRect() == keyAxis->axisRect(), "addLiveValue", "Axes must be on the same rect");

    ////////////////////// From here

    if (!graphs.contains(v)) {
        ChartColours colours = Settings::getInstance().getChartColours();

        GraphStyle style = GraphStyle(v);

        // For extra columns we have to set the style name to the extra columns
        // configured name here as once the style is passed into the live plot
        // widget we've got no further control over it. If we don't do this
        // the graph will start off with the right name but the graph style
        // window will only have the default name.
        SET_EC_STYLE_NAME(LV_SoilMoisture1, EC_SoilMoisture1);
        SET_EC_STYLE_NAME(LV_SoilMoisture2, EC_SoilMoisture2);
        SET_EC_STYLE_NAME(LV_SoilMoisture3, EC_SoilMoisture3);
        SET_EC_STYLE_NAME(LV_SoilMoisture4, EC_SoilMoisture4);
        SET_EC_STYLE_NAME(LV_SoilTemperature1, EC_SoilTemperature1);
        SET_EC_STYLE_NAME(LV_SoilTemperature2, EC_SoilTemperature2);
        SET_EC_STYLE_NAME(LV_SoilTemperature3, EC_SoilTemperature3);
        SET_EC_STYLE_NAME(LV_SoilTemperature4, EC_SoilTemperature4);
        SET_EC_STYLE_NAME(LV_LeafWetness1, EC_LeafWetness1);
        SET_EC_STYLE_NAME(LV_LeafWetness2, EC_LeafWetness2);
        SET_EC_STYLE_NAME(LV_LeafTemperature1, EC_LeafTemperature1);
        SET_EC_STYLE_NAME(LV_LeafTemperature2, EC_LeafTemperature2);
        SET_EC_STYLE_NAME(LV_ExtraTemperature1, EC_ExtraTemperature1);
        SET_EC_STYLE_NAME(LV_ExtraTemperature2, EC_ExtraTemperature2);
        SET_EC_STYLE_NAME(LV_ExtraTemperature3, EC_ExtraTemperature3);
        SET_EC_STYLE_NAME(LV_ExtraHumidity1, EC_ExtraHumidity1);
        SET_EC_STYLE_NAME(LV_ExtraHumidity2, EC_ExtraHumidity2);

        graphs[v] = ui->plot->addStyledGraph(keyAxis, valueAxis, style);
        graphs[v]->setProperty(PROP_GRAPH_TYPE, v);
        graphs[v]->setProperty(PROP_IS_POINT, false);

        points[v] = new QCPGraph(keyAxis, valueAxis);

        points[v]->setLineStyle(QCPGraph::lsNone);
        points[v]->setScatterStyle(QCPScatterStyle::ssDisc);
        points[v]->removeFromLegend();
        points[v]->setProperty(PROP_GRAPH_TYPE, v);
        points[v]->setProperty(PROP_IS_POINT, true);
        points[v]->setSelectable(QCP::stNone);

        if (axisTags) {
            if (!tags[v].isNull()) {
                delete tags[v];
                tags.remove(v);
            }
            tags[v] = new ValueAxisTag(graphs[v], true, true, ui->plot);
            tags[v]->setStyle(style);
        }

        if (valueNames.contains(v)) {
            graphs[v]->setName(valueNames[v]);
        } else if (extraColumnMapping.contains(v)) {
            graphs[v]->setName(extraColumnNames[extraColumnMapping[v]]);
        } else {
            graphs[v]->setName(tr("uknown graph"));
        }

        switch(v) {
        case LV_Temperature:
            graphs[v]->setPen(QPen(colours.temperature));
            break;
        case LV_IndoorTemperature:
            graphs[v]->setPen(QPen(colours.indoorTemperature));
            break;
        case LV_ApparentTemperature:
            graphs[v]->setPen(QPen(colours.apparentTemperature));
            break;
        case LV_WindChill:
            graphs[v]->setPen(QPen(colours.windChill));
            break;
        case LV_DewPoint:
            graphs[v]->setPen(QPen(colours.dewPoint));
            break;
        case LV_Humidity:
            graphs[v]->setPen(QPen(colours.humidity));
            break;
        case LV_IndoorHumidity:
            graphs[v]->setPen(QPen(colours.indoorHumidity));
            break;
        case LV_Pressure:
            graphs[v]->setPen(QPen(colours.pressure));
            break;
        case LV_WindSpeed:
            graphs[v]->setPen(QPen(colours.averageWindSpeed));
            break;
        case LV_WindDirection:
            graphs[v]->setPen(QPen(colours.windDirection));
            break;
        case LV_StormRain:
            graphs[v]->setPen(QPen(colours.rainfall));
            break;
        case LV_RainRate:
            graphs[v]->setPen(QPen(colours.rainRate));
            break;
        case LV_BatteryVoltage:
            graphs[v]->setPen(QPen(colours.consoleBatteryVoltage));
            break;
        case LV_UVIndex:
            graphs[v]->setPen(QPen(colours.uvIndex));
            break;
        case LV_SolarRadiation:
            graphs[v]->setPen(QPen(colours.solarRadiation));
            break;

        case LV_SoilMoisture1:
            graphs[v]->setPen(QPen(colours.soilMoisture1));
            break;
        case LV_SoilMoisture2:
            graphs[v]->setPen(QPen(colours.soilMoisture2));
            break;
        case LV_SoilMoisture3:
            graphs[v]->setPen(QPen(colours.soilMoisture3));
            break;
        case LV_SoilMoisture4:
            graphs[v]->setPen(QPen(colours.soilMoisture4));
            break;
        case LV_SoilTemperature1:
            graphs[v]->setPen(QPen(colours.soilTemperature1));
            break;
        case LV_SoilTemperature2:
            graphs[v]->setPen(QPen(colours.soilTemperature2));
            break;
        case LV_SoilTemperature3:
            graphs[v]->setPen(QPen(colours.soilTemperature3));
            break;
        case LV_SoilTemperature4:
            graphs[v]->setPen(QPen(colours.soilTemperature4));
            break;
        case LV_LeafWetness1:
            graphs[v]->setPen(QPen(colours.leafWetness1));
            break;
        case LV_LeafWetness2:
            graphs[v]->setPen(QPen(colours.leafWetness2));
            break;
        case LV_LeafTemperature1:
            graphs[v]->setPen(QPen(colours.leafTemperature1));
            break;
        case LV_LeafTemperature2:
            graphs[v]->setPen(QPen(colours.leafTemperature2));
            break;
        case LV_ExtraTemperature1:
            graphs[v]->setPen(QPen(colours.extraTemperature1));
            break;
        case LV_ExtraTemperature2:
            graphs[v]->setPen(QPen(colours.extraTemperature2));
            break;
        case LV_ExtraTemperature3:
            graphs[v]->setPen(QPen(colours.extraTemperature3));
            break;
        case LV_ExtraHumidity1:
            graphs[v]->setPen(QPen(colours.extraHumidity1));
            break;
        case LV_ExtraHumidity2:
            graphs[v]->setPen(QPen(colours.extraHumidity2));
            break;


        case LV_NoColumns:
        default:
            break; // nothing to do.
        }

        graphStyleChanged(graphs[v], style);

        points[v]->setPen(graphs[v]->pen());
        points[v]->setBrush(graphs[v]->pen().color());

        if (ui->plot->legend != NULL) {
            ui->plot->legend->setVisible(graphs.count() > 1);
            ui->actionLegend->setChecked(ui->plot->legend->visible());
        }
    }

    ui->plot->replot();
}

void LivePlotWindow::liveData(LiveDataSet ds) {
    //qDebug() << "New plot data!";
    if (valuesToShow == LV_NoColumns) {
        return; // Nothing to do.
    }

    double ts = ds.timestamp.toMSecsSinceEpoch() / 1000.0;

    qint64 xRange = timespanMinutes * 60; // seconds
    double padding = 0.5 * timespanMinutes; // TempView uses 1.0 for 2 minutes, 100.0 for 2 hours.

    // Scroll the key axis
    double pos = ts + padding;
    if (multipleAxisRects) {
        foreach (QCPAxisRect *rect, axisRects) {
            rect->axis(QCPAxis::atBottom)->setRange(pos, xRange, Qt::AlignRight);
        }
    } else if (ui->plot->axisRectCount() > 0) { // Need at least one rect.
        ui->plot->axisRect()->axis(QCPAxis::atBottom)->setRange(pos,
                                                                xRange,
                                                                Qt::AlignRight);
    }

    updateGraph(LV_Temperature, ts, xRange, ds.temperature);
    updateGraph(LV_IndoorTemperature, ts, xRange, ds.indoorTemperature);
    updateGraph(LV_ApparentTemperature, ts, xRange, ds.apparentTemperature);
    updateGraph(LV_WindChill, ts, xRange, ds.windChill);
    updateGraph(LV_DewPoint, ts, xRange, ds.dewPoint);
    updateGraph(LV_Humidity, ts, xRange, ds.humidity);
    updateGraph(LV_IndoorHumidity, ts, xRange, ds.indoorHumidity);
    updateGraph(LV_Pressure, ts, xRange, ds.pressure);
    updateGraph(LV_WindSpeed, ts, xRange, ds.windSpeed);
    updateGraph(LV_WindDirection, ts, xRange, ds.windDirection);
    updateGraph(LV_StormRain, ts, xRange, ds.davisHw.stormRain);
    updateGraph(LV_RainRate, ts, xRange, ds.davisHw.rainRate);
    updateGraph(LV_BatteryVoltage, ts, xRange, ds.davisHw.consoleBatteryVoltage);
    updateGraph(LV_UVIndex, ts, xRange, ds.davisHw.uvIndex);
    updateGraph(LV_SolarRadiation, ts, xRange, ds.davisHw.solarRadiation);
    updateGraph(LV_SoilMoisture1, ts, xRange, ds.davisHw.soilMoisture1);
    updateGraph(LV_SoilMoisture2, ts, xRange, ds.davisHw.soilMoisture2);
    updateGraph(LV_SoilMoisture3, ts, xRange, ds.davisHw.soilMoisture3);
    updateGraph(LV_SoilMoisture4, ts, xRange, ds.davisHw.soilMoisture4);
    updateGraph(LV_SoilTemperature1, ts, xRange, ds.davisHw.soilTemperature1);
    updateGraph(LV_SoilTemperature2, ts, xRange, ds.davisHw.soilTemperature2);
    updateGraph(LV_SoilTemperature3, ts, xRange, ds.davisHw.soilTemperature3);
    updateGraph(LV_SoilTemperature4, ts, xRange, ds.davisHw.soilTemperature4);
    updateGraph(LV_LeafWetness1, ts, xRange, ds.davisHw.leafWetness1);
    updateGraph(LV_LeafWetness2, ts, xRange, ds.davisHw.leafWetness2);
    updateGraph(LV_LeafTemperature1, ts, xRange, ds.davisHw.leafTemperature1);
    updateGraph(LV_LeafTemperature2, ts, xRange, ds.davisHw.leafTemperature2);
    updateGraph(LV_ExtraTemperature1, ts, xRange, ds.davisHw.extraTemperature1);
    updateGraph(LV_ExtraTemperature2, ts, xRange, ds.davisHw.extraTemperature2);
    updateGraph(LV_ExtraTemperature3, ts, xRange, ds.davisHw.extraTemperature3);
    updateGraph(LV_ExtraHumidity1, ts, xRange, ds.davisHw.extraHumidity1);
    updateGraph(LV_ExtraHumidity2, ts, xRange, ds.davisHw.extraHumidity2);

    ui->plot->replot(QCustomPlot::rpQueuedReplot);
}

void LivePlotWindow::updateGraph(LiveValue type, double key, double range, double value) {

    if (imperial) {
        value = metricToImperial(type, value);
    } else if (kmh && type == LV_WindSpeed) {
        value = UnitConversions::metersPerSecondToKilometersPerHour(value);
    }

    if (graphs.contains(type)) {
        graphs[type]->data()->removeBefore(key - range);
        graphs[type]->addData(key, value);
        points[type]->data()->clear();
        points[type]->addData(key, value);

        if (tags.contains(type)) {
            tags[type]->setValue(1, value);
        }

        if (units[type] == UnitConversions::U_LEAF_WETNESS) {
            return; // Range is fixed 0-15 on axis creation.
        }

        QCPRange oldRange = graphs[type]->valueAxis()->range();
        graphs[type]->rescaleValueAxis();


        // Add a bit of padding to the Y axis - the range tends to be relatively small
        // and often you can end up with the line just following the very top and
        // bottom of the automatic range.
        QCPRange range = graphs[type]->valueAxis()->range();

        // But only apply padding if the range changed during the rescale. Otherwise we
        // just cause the axes range to slowly drift larger
        if (range.lower != oldRange.lower || range.upper != oldRange.upper) {
            graphs[type]->valueAxis()->setRange(range.lower - 1.0,
                                                range.upper + 1.0);
        }

        // For all value axes except temperature ensure they don't go below zero.
        // Because a negative rain rate would be concerning.
        if (units[type] != UnitConversions::U_CELSIUS
                && units[type] != UnitConversions::U_FAHRENHEIT) {
            if (graphs[type]->valueAxis()->range().lower < 0) {
                graphs[type]->valueAxis()->setRangeLower(0);
            }
        }
    }
}

#define TEST_ADD_COL(col) if(columns.testFlag(col)) addLiveValue(col)

void LivePlotWindow::addLiveValues(LiveValues columns) {
    TEST_ADD_COL(LV_Temperature);
    TEST_ADD_COL(LV_ApparentTemperature);
    TEST_ADD_COL(LV_IndoorTemperature);
    TEST_ADD_COL(LV_WindChill);
    TEST_ADD_COL(LV_DewPoint);
    TEST_ADD_COL(LV_Humidity);
    TEST_ADD_COL(LV_IndoorHumidity);
    TEST_ADD_COL(LV_Pressure);
    TEST_ADD_COL(LV_BatteryVoltage);
    TEST_ADD_COL(LV_WindSpeed);
    TEST_ADD_COL(LV_WindDirection);
    TEST_ADD_COL(LV_RainRate);
    TEST_ADD_COL(LV_StormRain);
    TEST_ADD_COL(LV_UVIndex);
    TEST_ADD_COL(LV_SolarRadiation);

    TEST_ADD_COL(LV_SoilMoisture1);
    TEST_ADD_COL(LV_SoilMoisture2);
    TEST_ADD_COL(LV_SoilMoisture3);
    TEST_ADD_COL(LV_SoilMoisture4);
    TEST_ADD_COL(LV_SoilTemperature1);
    TEST_ADD_COL(LV_SoilTemperature2);
    TEST_ADD_COL(LV_SoilTemperature3);
    TEST_ADD_COL(LV_SoilTemperature4);
    TEST_ADD_COL(LV_LeafWetness1);
    TEST_ADD_COL(LV_LeafWetness2);
    TEST_ADD_COL(LV_LeafTemperature1);
    TEST_ADD_COL(LV_LeafTemperature2);
    TEST_ADD_COL(LV_ExtraTemperature1);
    TEST_ADD_COL(LV_ExtraTemperature2);
    TEST_ADD_COL(LV_ExtraTemperature3);
    TEST_ADD_COL(LV_ExtraHumidity1);
    TEST_ADD_COL(LV_ExtraHumidity2);

    resetData();

    ui->plot->replot();

    // If all possible values are now in the plot, disable the option to
    // add more (all the options will be greyed out if the user brings up
    // that dialog)
    ui->actionAdd_Graph->setEnabled(valuesToShow != ALL_LIVE_COLUMNS);
    ui->plot->setAddGraphsEnabled(valuesToShow != ALL_LIVE_COLUMNS);
}

void LivePlotWindow::showAddGraphDialog(QString message, QString title) {
    AddLiveGraphDialog algd(~valuesToShow,
                            solarAvailable,
                            hwType,
                            extraColumns,
                            extraColumnNames,
                            message,
                            this);

    if (!title.isNull() && !title.isEmpty()) {
        algd.setWindowTitle(title);
    }

    if (algd.exec() == QDialog::Accepted) {
        addLiveValues(algd.selectedColumns());
    }
}

void LivePlotWindow::graphRemoving(QCPGraph *graph) {
    QVariant prop = graph->property(PROP_GRAPH_TYPE);
    if (prop.isNull() || !prop.isValid() || prop.type() != QVariant::UInt) {
        return;
    }

    bool isPoint = graph->property(PROP_IS_POINT).toBool();
    LiveValue graphType = (LiveValue)prop.toUInt();

    qDebug() << "Graph" << graphType << "is being removed!";

    if (isPoint && graphs.contains(graphType)) {
        // Remove the owning graph too.
        ui->plot->removeGraph(graphs[graphType]);
    }

    if (valuesToShow.testFlag(graphType)) {
        valuesToShow &= ~graphType;
    }

    if (graphs.contains(graphType)) {
        graphs.remove(graphType);
    }

    if (points.contains(graphType)) {
        ui->plot->removeGraph(points[graphType]);
        points.remove(graphType);
    }

    if (tags.contains(graphType)) {
        delete tags[graphType];
        tags.remove(graphType);
    }

    if (axisRects.contains(graphType)) {
        axisRects.remove(graphType);
    } else {
        UnitConversions::unit_t axisType = units[graphType];
        if (axis[axisType]->graphs().count() == 1) {
            // The graph we're about to remove is the last graph using this axis
            // so the axis will end up being removed too. Remove the axis from our
            // list of axes so we don't accidentally use it again.
            axis.remove(axisType);
        }

        // Turn the legend off if we're removing the final graph.
        if (valuesToShow == LV_NoColumns && ui->plot->legend->visible()) {
            ui->plot->toggleLegend();
        }
    }

    ui->actionAdd_Graph->setEnabled(valuesToShow != ALL_LIVE_COLUMNS);
}

void LivePlotWindow::selectionChanged() {
    bool graphsSelected = !ui->plot->selectedGraphs().isEmpty();
    ui->actionRemove_Graph->setEnabled(graphsSelected);
}

void LivePlotWindow::showOptions() {

    Settings &settings = Settings::getInstance();

    Settings::live_multi_axis_label_type_t axis_labels = settings.liveMultipleAxisRectsAxisLabelType();

    LiveChartOptionsDialog lcod(aggregate, aggregateSeconds, maxRainRate, stormRain,
                                hwType == HW_DAVIS, timespanMinutes, axisTags,
                                multipleAxisRects, axis_labels,
                                this);

    if (lcod.exec() != QDialog::Accepted) {
        return;
    }

    bool resetPlot = false;

    if (aggregate != lcod.aggregate() || maxRainRate != lcod.maxRainRate() ||
            stormRain != lcod.stormRain() || aggregateSeconds != lcod.aggregatePeriod()) {

        resetPlot = true;

        // User changed settings.
        aggregate = lcod.aggregate();
        maxRainRate = lcod.maxRainRate();
        stormRain = lcod.stormRain();
        aggregateSeconds = lcod.aggregatePeriod();

        if (aggregate) {
            aggregator.reset(new AveragedLiveAggregator(aggregateSeconds, maxRainRate, stormRain, this));
        } else {
            aggregator.reset(new NonAggregatingLiveAggregator(stormRain, this));
        }

        connect(repeater.data(), SIGNAL(liveData(LiveDataSet)),
                aggregator.data(), SLOT(incomingLiveData(LiveDataSet)));
        connect(aggregator.data(), SIGNAL(liveData(LiveDataSet)),
                this, SLOT(liveData(LiveDataSet)));
    }

    if (lcod.rangeMinutes() != timespanMinutes) {
        timespanMinutes = lcod.rangeMinutes();
    }

    if (lcod.tagsEnabled() != axisTags) {
        axisTags = lcod.tagsEnabled();
        resetPlot = true;
    }

    if (lcod.multipleAxisRectsEnabled() != multipleAxisRects) {
        multipleAxisRects = lcod.multipleAxisRectsEnabled();
        resetPlot = true;
    }

    if (lcod.multipleAxisRectsEnabled() && lcod.multiAxisLabels() != axis_labels) {
        settings.setLiveMultipleAxisRectsAxisLabelType(lcod.multiAxisLabels());
        resetPlot = true;
    }

    // changing either axis tags or the number of axis rects requires the entire plot
    // to be reset
    if (resetPlot) {
        qDebug() << "Resetting plot...";
        LiveValues currentValues = valuesToShow;

        this->resetPlot();

        addLiveValues(currentValues);
    }

    resetData();


    settings.setLiveAggregate(aggregate);
    settings.setLiveMaxRainRate(maxRainRate);
    settings.setLiveStormRain(stormRain);
    settings.setLiveAggregateSeconds(aggregateSeconds);
    settings.setLiveTimespanMinutes(timespanMinutes);
    settings.setLiveTagsEnabled(axisTags);
    settings.setLiveMultipleAxisRectsEnabled(multipleAxisRects);

}

void LivePlotWindow::graphStyleChanged(QCPGraph *graph, GraphStyle &newStyle)
{
    QVariant prop = graph->property(PROP_GRAPH_TYPE);
    if (prop.isNull() || !prop.isValid() || prop.type() != QVariant::UInt) {
        return;
    }

    LiveValue graphType = (LiveValue)prop.toUInt();

    if (points.contains(graphType)) {
        points[graphType]->setPen(newStyle.getPen());
    }

    if (axisTags && tags.contains(graphType)) {
        tags[graphType]->setStyle(newStyle);
    }
}

void LivePlotWindow::toggleCursor() {
    if (plusCursor.isNull()) {
        return; // can't toggle whats not there...
    }

    bool enabled = !plusCursor->isEnabled();
    Settings::getInstance().setLiveChartCursorEnabled(enabled);
    plusCursor->setEnabled(enabled);
    ui->actionCrosshair->setChecked(plusCursor->isEnabled());
}

void LivePlotWindow::setMouseTrackingEnabled(bool enabled) {
    if (mouseTracker.isNull()) {
        return;
    }
    mouseTracker->setEnabled(enabled);
    Settings::getInstance().setLiveChartTracksMouseEnabled(enabled);
}

void LivePlotWindow::resetPlot() {
    Settings& settings = Settings::getInstance();

    // Its easier and safer to just trash the plot and start again rather than return it
    // to its original state manually.
    delete ui->plot;
    graphs.clear();
    points.clear();
    tags.clear();
    axisRects.clear();
    legendLayout = 0;

    ui->plot = new LivePlot(ui->centralwidget);
    ui->plot->setObjectName("plot");
    ui->gridLayout->addWidget(ui->plot, 0, 0, 1, 1);
    ui->plot->plotLayout()->setFillOrder(QCPLayoutGrid::foRowsFirst);

    // Configure the plot
    ui->plot->setBackground(QBrush(settings.getChartColours().background));

    ticker.clear();
    ticker = QSharedPointer<QCPAxisTicker>(new QCPAxisTickerDateTime());
    ui->plot->plotLayout()->remove(ui->plot->axisRect());

    mouseTracker = new ChartMouseTracker(ui->plot);
    mouseTracker->setEnabled(settings.liveChartTracksMouseEnabled());

    plusCursor = new PlusCursor(ui->plot);
    plusCursor->setEnabled(settings.liveChartCursorEnabled());

    connect(ui->actionSave, SIGNAL(triggered(bool)),
            this->ui->plot, SLOT(save()));
    connect(ui->actionCopy, SIGNAL(triggered(bool)),
            this->ui->plot, SLOT(copy()));
    connect(ui->actionRemove_Graph, SIGNAL(triggered(bool)),
            ui->plot, SLOT(removeSelectedGraph()));
    connect(ui->actionLegend, SIGNAL(triggered(bool)),
            ui->plot, SLOT(toggleLegend()));
    connect(ui->actionTitle, SIGNAL(triggered(bool)),
            ui->plot, SLOT(toggleTitle()));

    // Events from the plotting widget
    connect(ui->plot, SIGNAL(addGraphRequested()),
            this, SLOT(showAddGraphDialog()));
    connect(ui->plot, SIGNAL(removingGraph(QCPGraph*)),
            this, SLOT(graphRemoving(QCPGraph*)));
    connect(ui->plot, SIGNAL(selectionChangedByUser()),
            this, SLOT(selectionChanged()));
    connect(ui->plot, SIGNAL(titleVisibilityChanged(bool)),
            ui->actionTitle, SLOT(setChecked(bool)));
    connect(ui->plot, SIGNAL(legendVisibilityChanged(bool)),
            this, SLOT(legendVisibilityChanged(bool)));
    connect(ui->plot, SIGNAL(graphStyleChanged(QCPGraph*,GraphStyle&)),
            this, SLOT(graphStyleChanged(QCPGraph*,GraphStyle&)));
}

void LivePlotWindow::resetData() {
    qDebug() << "Reset live plot data!";
    foreach (LiveValue v, graphs.keys()) {
        graphs[v]->data().clear();
        points[v]->data().clear();
    }

    ds->disableLiveData();
    aggregator->reset();

    QDateTime minTime = QDateTime::currentDateTime().addSecs(0-timespanMinutes*60);
    foreach (LiveDataSet lds, LiveBuffer::getInstance().getData()) {
        if (lds.timestamp > minTime) {
            //liveData(lds);
         //   qDebug() << "LiveBuffer data @" << lds.timestamp;
            repeater->incomingLiveData(lds);
        }
    }

    ds->enableLiveData();
    ui->plot->replot();
}
