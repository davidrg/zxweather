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
#include "axistag.h"

#define PROP_GRAPH_TYPE "graph_type"
#define PROP_IS_POINT "is_point"

UnitConversions::unit_t metricUnitToImperial(UnitConversions::unit_t unit);

#define imperialiseUnitDict(type) units[type] = metricUnitToImperial(units[type])

LivePlotWindow::LivePlotWindow(bool solarAvailalble, hardware_type_t hardwareType, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LivePlotWindow)
{
    ui->setupUi(this);

    imperial = Settings::getInstance().imperial();

    this->hwType = hardwareType;
    this->solarAvailable = solarAvailalble;
    this->valuesToShow = LV_NoColumns;

    // All the possible axis types
    units[LV_Temperature] = UnitConversions::U_CELSIUS;
    units[LV_IndoorTemperature] = UnitConversions::U_CELSIUS;
    units[LV_ApparentTemperature] = UnitConversions::U_CELSIUS;
    units[LV_WindChill] = UnitConversions::U_CELSIUS;
    units[LV_DewPoint] = UnitConversions::U_CELSIUS;
    units[LV_Humidity] = UnitConversions::U_HUMIDITY;
    units[LV_IndoorHumidity] = UnitConversions::U_HUMIDITY;
    units[LV_Pressure] = UnitConversions::U_HECTOPASCALS;
    units[LV_WindSpeed] = UnitConversions::U_METERS_PER_SECOND;
    units[LV_WindDirection] = UnitConversions::U_DEGREES;
    units[LV_StormRain] = UnitConversions::U_MILLIMETERS;
    units[LV_RainRate] = UnitConversions::U_MILLIMETERS_PER_HOUR;
    units[LV_BatteryVoltage] = UnitConversions::U_VOLTAGE;
    units[LV_UVIndex] = UnitConversions::U_UV_INDEX;
    units[LV_SolarRadiation] = UnitConversions::U_WATTS_PER_SQUARE_METER;

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
    }

    // And their labels
    axisLabels[UnitConversions::U_CELSIUS] = tr("Temperature (" DEGREE_SYMBOL "C)");
    axisLabels[UnitConversions::U_FAHRENHEIT] = tr("Fahrenheit (" DEGREE_SYMBOL "F)");
    axisLabels[UnitConversions::U_HUMIDITY] = tr("Humidity (%)");
    axisLabels[UnitConversions::U_HECTOPASCALS] = tr("Pressure (hPa)");
    axisLabels[UnitConversions::U_INCHES_OF_MERCURY] = tr("Inches of Mercury (InHg)");
    axisLabels[UnitConversions::U_METERS_PER_SECOND] = tr("Wind Speed (m/s)");
    axisLabels[UnitConversions::U_KILOMETERS_PER_HOUR] = tr("Wind Speed (km/h)");
    axisLabels[UnitConversions::U_MILES_PER_HOUR] = tr("Wind Speed (mph)");
    axisLabels[UnitConversions::U_DEGREES] = tr("Wind direction (" DEGREE_SYMBOL ")");
    axisLabels[UnitConversions::U_MILLIMETERS] = tr("Rainfall (mm)");
    axisLabels[UnitConversions::U_INCHES] = tr("Rainfall (in)");
    axisLabels[UnitConversions::U_MILLIMETERS_PER_HOUR] = tr("Rain Rate (mm/h)");
    axisLabels[UnitConversions::U_INCHES_PER_HOUR] = tr("Rain rate (in/h)");
    axisLabels[UnitConversions::U_VOLTAGE] = tr("Voltage (V)");
    axisLabels[UnitConversions::U_UV_INDEX] = tr("UV Index");
    axisLabels[UnitConversions::U_WATTS_PER_SQUARE_METER] = tr("Solar Radiation (W/m" SQUARED_SYMBOL ")");

    Settings &settings = Settings::getInstance();

    switch(settings.liveDataSourceType()) {
    case Settings::DS_TYPE_DATABASE:
        ds = new DatabaseDataSource(new DialogProgressListener(this), this);
        break;
    case Settings::DS_TYPE_SERVER:
        ds = new TcpLiveDataSource(this);
        break;
    case Settings::DS_TYPE_WEB_INTERFACE:
        ds = new WebDataSource(new DialogProgressListener(this), this);
        break;
    }

    // This is to give the averaging aggregator a constant stream of updates
    // so it keeps producing new samples when the weater station goes quiet.
    repeater.reset(new LiveDataRepeater(this));
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

    showAddGraphDialog(
                tr("Select the values to display in the live chart. More can be added "
                   "later."),
                tr("Choose graphs"));


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

        QCPAxisRect *rect = new QCPAxisRect(ui->plot);
        rect->setupFullAxesBox(true);

        Settings& settings = Settings::getInstance();

        rect->axis(QCPAxis::atTop)->setVisible(false);
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

        if (axisRects.count() == 1) {
            // We've just created the first axis rect. Also create a legend - hidden by
            // default.

            if (ui->plot->legend == NULL) {
                qDebug() << "Create legend";
                ui->plot->legend = new QCPLegend;
                ui->plot->legend->setVisible(false);
            }

            if (legendLayout == NULL) {
                qDebug() << "Create legend layout";
                legendLayout = new QCPLayoutGrid;
                legendLayout->setMargins(QMargins(5, 0, 5, 5));

                // Chuck it in the layout to ensure the legend doesn't get separated
                // from the plot when we reparent it.
                ui->plot->plotLayout()->addElement(legendLayout);
            }

            qDebug() << "Reparent legend";
            legendLayout->addElement(0, 0, ui->plot->legend);
            ui->plot->legend->setFillOrder(QCPLegend::foColumnsFirst);
        }

        if (legendLayout != NULL) {
            qDebug() << "Move legend layout to bottom of plot";
            // Shift the legend to the bottom
            ui->plot->plotLayout()->addElement(legendLayout);
            ui->plot->plotLayout()->simplify();
            ui->plot->plotLayout()->setRowStretchFactor(
                        ui->plot->plotLayout()->rowCount() - 1, 0.001);
        }

        return rect;
    } else {
        qDebug() << "Creating default axis rect";
        ui->plot->recreateDefaultAxisRect();
        ui->plot->axisRect()->axis(QCPAxis::atLeft)->setVisible(false);
        ui->plot->axisRect()->axis(QCPAxis::atRight)->setVisible(false);
        ui->plot->axisRect()->axis(QCPAxis::atBottom)->setTicker(ticker);
        axis.clear();

        qDebug() << "Default rect created";

        return ui->plot->axisRect();
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
            if(!y1->visible() && !axisTags) {
                this->axis[axisType] = y1;
                y1->setVisible(true);
            } else if (!y2->visible()) {
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
        axis->setLabel(axisLabels[units[type]]);

        if (axisTags) {
            axis->setPadding(10);
            axis->setLabelPadding(30);
        }
    }

    return axis;
}

void LivePlotWindow::addLiveValue(LiveValue v) {
    valuesToShow |= v;

    //UnitConversions::unit_t axisType = units[v];

    // These will create any axes and axis rects if they don't already exist.
    QCPAxis *valueAxis = valueAxisForGraph(v);
    QCPAxis *keyAxis = keyAxisForGraph(v);

    ////////////////////// From here

    if (!graphs.contains(v)) {
        ChartColours colours = Settings::getInstance().getChartColours();

        GraphStyle style = GraphStyle(v);

        graphs[v] = ui->plot->addStyledGraph(keyAxis, valueAxis, style);
        graphs[v]->setProperty(PROP_GRAPH_TYPE, v);
        graphs[v]->setProperty(PROP_IS_POINT, false);

        points[v] = new QCPGraph(keyAxis, valueAxis);

        points[v]->setLineStyle(QCPGraph::lsNone);
        points[v]->setScatterStyle(QCPScatterStyle::ssDisc);
        points[v]->removeFromLegend();
        points[v]->setProperty(PROP_GRAPH_TYPE, v);
        points[v]->setProperty(PROP_IS_POINT, true);

        if (axisTags) {
            tags[v] = new AxisTag(valueAxis, this);
            tags[v]->setStyle(style);
        }

        switch(v) {
        case LV_Temperature:
            graphs[v]->setName(tr("Temperature"));
            graphs[v]->setPen(QPen(colours.temperature));
            break;
        case LV_IndoorTemperature:
            graphs[v]->setName(tr("Indoor Temperature"));
            graphs[v]->setPen(QPen(colours.indoorTemperature));
            break;
        case LV_ApparentTemperature:
            graphs[v]->setName(tr("Apparent Temperature"));
            graphs[v]->setPen(QPen(colours.apparentTemperature));
            break;
        case LV_WindChill:
            graphs[v]->setName(tr("Wind Chill"));
            graphs[v]->setPen(QPen(colours.windChill));
            break;
        case LV_DewPoint:
            graphs[v]->setName(tr("Dew Point"));
            graphs[v]->setPen(QPen(colours.dewPoint));
            break;
        case LV_Humidity:
            graphs[v]->setName(tr("Humidity"));
            graphs[v]->setPen(QPen(colours.humidity));
            break;
        case LV_IndoorHumidity:
            graphs[v]->setName(tr("Indoor Humidity"));
            graphs[v]->setPen(QPen(colours.indoorHumidity));
            break;
        case LV_Pressure:
            graphs[v]->setName(tr("Pressure"));
            graphs[v]->setPen(QPen(colours.pressure));
            break;
        case LV_WindSpeed:
            graphs[v]->setName(tr("Wind Speed"));
            graphs[v]->setPen(QPen(colours.averageWindSpeed));
            break;
        case LV_WindDirection:
            graphs[v]->setName(tr("Wind Direction"));
            graphs[v]->setPen(QPen(colours.windDirection));
            break;
        case LV_StormRain:
            graphs[v]->setName(tr("Storm Rain"));
            graphs[v]->setPen(QPen(colours.rainfall));
            break;
        case LV_RainRate:
            graphs[v]->setName(tr("Rain Rate"));
            graphs[v]->setPen(QPen(colours.rainRate));
            break;
        case LV_BatteryVoltage:
            graphs[v]->setName(tr("Battery Voltage"));
            graphs[v]->setPen(QPen(colours.consoleBatteryVoltage));
            break;
        case LV_UVIndex:
            graphs[v]->setName(tr("UV Index"));
            graphs[v]->setPen(QPen(colours.uvIndex));
            break;
        case LV_SolarRadiation:
            graphs[v]->setName(tr("Solar Radiation"));
            graphs[v]->setPen(QPen(colours.solarRadiation));
            break;
        case LV_NoColumns:
        default:
            break; // nothing to do.
        }

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
    qDebug() << "New plot data!";
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

    ui->plot->replot(QCustomPlot::rpQueuedReplot);
}

void LivePlotWindow::updateGraph(LiveValue type, double key, double range, double value) {

    if (imperial) {
        value = metricToImperial(type, value);
    }

    if (graphs.contains(type)) {
        graphs[type]->data()->removeBefore(key - range);
        graphs[type]->addData(key, value);
        points[type]->data()->clear();
        points[type]->addData(key, value);

        if (tags.contains(type)) {
            tags[type]->setValue(value);
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
                            message,
                            this);
    algd.setWindowTitle(title);

    if (algd.exec() == QDialog::Accepted) {
        addLiveValues(algd.selectedColumns());
    }
}

void LivePlotWindow::graphRemoving(QCPGraph *graph) {
    QVariant prop = graph->property(PROP_GRAPH_TYPE);
    if (prop.isNull() || !prop.isValid() || prop.type() != QVariant::Int) {
        return;
    }

    bool isPoint = graph->property(PROP_IS_POINT).toBool();
    LiveValue graphType = (LiveValue)prop.toInt();

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
    LiveChartOptionsDialog lcod(aggregate, aggregateSeconds, maxRainRate, stormRain,
                                hwType == HW_DAVIS, timespanMinutes, axisTags,
                                multipleAxisRects, this);

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

    // changing either axis tags or the number of axis rects requires the entire plot
    // to be reset
    if (resetPlot) {
        qDebug() << "Resetting plot...";
        LiveValues currentValues = valuesToShow;

        this->resetPlot();

        addLiveValues(currentValues);
    }

    resetData();

    Settings &settings = Settings::getInstance();
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
    if (prop.isNull() || !prop.isValid() || prop.type() != QVariant::Int) {
        return;
    }

    LiveValue graphType = (LiveValue)prop.toInt();

    if (points.contains(graphType)) {
        points[graphType]->setPen(newStyle.getPen());
    }

    if (axisTags && tags.contains(graphType)) {
        tags[graphType]->setStyle(newStyle);
    }
}

void LivePlotWindow::resetPlot() {
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
    ui->plot->setBackground(QBrush(Settings::getInstance().getChartColours().background));

    ticker.clear();
    ticker = QSharedPointer<QCPAxisTicker>(new QCPAxisTickerDateTime());
    ui->plot->plotLayout()->remove(ui->plot->axisRect());

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
            ui->actionLegend, SLOT(setChecked(bool)));
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
